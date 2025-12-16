#include <GL/glew.h>
#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Graphics/Image.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ======================= ЛОГИ ШЕЙДЕРОВ =======================
void ShaderLog(GLuint shader)
{
    GLint infologLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infologLen);
    if (infologLen > 1)
    {
        std::vector<char> infoLog(infologLen);
        GLsizei charsWritten = 0;
        glGetShaderInfoLog(shader, infologLen, &charsWritten, infoLog.data());
        std::cout << "Shader log:\n" << infoLog.data() << std::endl;
    }
}

void ProgramLog(GLuint prog)
{
    GLint infologLen = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infologLen);
    if (infologLen > 1)
    {
        std::vector<char> infoLog(infologLen);
        GLsizei charsWritten = 0;
        glGetProgramInfoLog(prog, infologLen, &charsWritten, infoLog.data());
        std::cout << "Program log:\n" << infoLog.data() << std::endl;
    }
}

GLuint CompileShader(GLenum type, const char* src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    ShaderLog(sh);
    return sh;
}

GLuint LinkProgram(GLuint vert, GLuint frag)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) ProgramLog(prog);
    return prog;
}

// ======================= МАТЕМАТИКА =======================
struct Vec2 { float x = 0, y = 0; };

struct Vec3
{
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float X, float Y, float Z) :x(X), y(Y), z(Z) {}
};

Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x,a.y + b.y,a.z + b.z }; }
Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x,a.y - b.y,a.z - b.z }; }
Vec3 operator*(const Vec3& a, float s) { return { a.x * s,a.y * s,a.z * s }; }

float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

float Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }

Vec3 Normalize(const Vec3& v)
{
    float len = Length(v);
    if (len <= 1e-6f) return v;
    return v * (1.0f / len);
}

// column-major Mat4
struct Mat4
{
    float m[16] = { 0 };

    static Mat4 Identity()
    {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Mat4 Translation(float x, float y, float z)
    {
        Mat4 r = Identity();
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }

    static Mat4 Scale(float x, float y, float z)
    {
        Mat4 r;
        r.m[0] = x; r.m[5] = y; r.m[10] = z; r.m[15] = 1.0f;
        return r;
    }

    static Mat4 RotationY(float a)
    {
        Mat4 r = Identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c;  r.m[2] = s;
        r.m[8] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 RotationX(float a)
    {
        Mat4 r = Identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 Perspective(float fovyRad, float aspect, float zNear, float zFar)
    {
        Mat4 r;
        float t = std::tan(fovyRad / 2.0f);
        r.m[0] = 1.0f / (aspect * t);
        r.m[5] = 1.0f / t;
        r.m[10] = -(zFar + zNear) / (zFar - zNear);
        r.m[11] = -1.0f;
        r.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
        return r;
    }

    static Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
    {
        Vec3 f = Normalize(center - eye);
        Vec3 s = Normalize(Cross(f, up));
        Vec3 u = Cross(s, f);

        Mat4 r = Identity();
        r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
        r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;

        r.m[12] = -Dot(s, eye);
        r.m[13] = -Dot(u, eye);
        r.m[14] = Dot(f, eye);
        return r;
    }
};

Mat4 operator*(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
        {
            r.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    return r;
}

// ======================= ТЕКСТУРЫ =======================
GLuint LoadTextureFromFile(const std::string& filename, bool srgb = false)
{
    sf::Image img;
    if (!img.loadFromFile(filename))
    {
        std::cout << "Failed to load texture: " << filename << std::endl;
        return 0;
    }
    img.flipVertically();

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLint internalFmt = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;

    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt,
        (GLsizei)img.getSize().x, (GLsizei)img.getSize().y,
        0, GL_RGBA, GL_UNSIGNED_BYTE, img.getPixelsPtr());

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// ======================= OBJ (pos+uv+normal) + тангенты =======================
// Выходной формат вершины: pos(3) normal(3) uv(2) tangent(3) = 11 float
struct VertexPNUt
{
    Vec3 p;
    Vec3 n;
    Vec2 uv;
    Vec3 t; // tangent
};

static float frand(float a, float b)
{
    return a + (b - a) * (rand() / (float)RAND_MAX);
}

bool LoadOBJ_PNU(const std::string& filename, std::vector<VertexPNUt>& out)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cout << "Failed to open OBJ: " << filename << std::endl;
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<Vec2> texcoords;
    std::vector<Vec3> normals;

    struct Idx { int vi = 0, ti = 0, ni = 0; };

    auto parseIndex = [&](const std::string& s)->Idx
        {
            Idx r;
            size_t a = s.find('/');
            if (a == std::string::npos)
            {
                r.vi = std::stoi(s);
                return r;
            }
            std::string vStr = s.substr(0, a);
            if (!vStr.empty()) r.vi = std::stoi(vStr);

            size_t b = s.find('/', a + 1);
            std::string vtStr, vnStr;
            if (b == std::string::npos)
            {
                vtStr = s.substr(a + 1);
            }
            else
            {
                vtStr = s.substr(a + 1, b - a - 1);
                vnStr = s.substr(b + 1);
            }
            if (!vtStr.empty()) r.ti = std::stoi(vtStr);
            if (!vnStr.empty()) r.ni = std::stoi(vnStr);
            return r;
        };

    std::string line;
    std::vector<Idx> face;
    while (std::getline(file, line))
    {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string pref; iss >> pref;
        if (pref == "v")
        {
            float x, y, z; iss >> x >> y >> z;
            positions.emplace_back(x, y, z);
        }
        else if (pref == "vt")
        {
            float u, v; iss >> u >> v;
            texcoords.push_back({ u,v });
        }
        else if (pref == "vn")
        {
            float x, y, z; iss >> x >> y >> z;
            normals.emplace_back(x, y, z);
        }
        else if (pref == "f")
        {
            face.clear();
            std::string tok;
            while (iss >> tok) face.push_back(parseIndex(tok));
            if (face.size() < 3) continue;

            // fan triangulation
            for (size_t i = 1; i + 1 < face.size(); ++i)
            {
                Idx i0 = face[0];
                Idx i1 = face[i];
                Idx i2 = face[i + 1];

                auto getP = [&](int vi)->Vec3 { return positions[std::max(1, vi) - 1]; };
                auto getUV = [&](int ti)->Vec2 {
                    if (ti <= 0 || ti > (int)texcoords.size()) return { 0,0 };
                    return texcoords[ti - 1];
                    };
                auto getN = [&](int ni)->Vec3 {
                    if (ni <= 0 || ni > (int)normals.size()) return { 0,1,0 };
                    return normals[ni - 1];
                    };

                VertexPNUt v0, v1, v2;
                v0.p = getP(i0.vi); v1.p = getP(i1.vi); v2.p = getP(i2.vi);
                v0.uv = getUV(i0.ti); v1.uv = getUV(i1.ti); v2.uv = getUV(i2.ti);

                // нормали: если нет vn — посчитаем face normal
                Vec3 faceN = Normalize(Cross(v1.p - v0.p, v2.p - v0.p));
                v0.n = (i0.ni ? getN(i0.ni) : faceN);
                v1.n = (i1.ni ? getN(i1.ni) : faceN);
                v2.n = (i2.ni ? getN(i2.ni) : faceN);

                // tangent по треугольнику
                Vec3 e1 = v1.p - v0.p;
                Vec3 e2 = v2.p - v0.p;
                float du1 = v1.uv.x - v0.uv.x;
                float dv1 = v1.uv.y - v0.uv.y;
                float du2 = v2.uv.x - v0.uv.x;
                float dv2 = v2.uv.y - v0.uv.y;

                float f = (du1 * dv2 - du2 * dv1);
                Vec3 tangent = { 1,0,0 };
                if (std::fabs(f) > 1e-8f)
                {
                    float inv = 1.0f / f;
                    tangent = Normalize((e1 * dv2 - e2 * dv1) * inv);
                }

                v0.t = tangent; v1.t = tangent; v2.t = tangent;

                out.push_back(v0);
                out.push_back(v1);
                out.push_back(v2);
            }
        }
    }

    if (out.empty())
    {
        std::cout << "OBJ has no triangles: " << filename << std::endl;
        return false;
    }

    std::cout << "OBJ loaded: " << filename << ", vertices: " << out.size() << std::endl;
    return true;
}

// ======================= MESH =======================
struct Mesh
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLsizei vertexCount = 0;
    float minY = 0.0f;
};

Mesh CreateMesh_PNUt(const std::vector<VertexPNUt>& verts)
{
    Mesh m;
    m.vertexCount = (GLsizei)verts.size();
    
    // minY
    float mn = 1e9f;
    for (const auto& v : verts) mn = std::min(mn, v.p.y);
    m.minY = mn;

    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &m.VBO);

    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(VertexPNUt), verts.data(), GL_STATIC_DRAW);

    // location 0: pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNUt), (void*)offsetof(VertexPNUt, p));
    glEnableVertexAttribArray(0);
    // location 1: normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNUt), (void*)offsetof(VertexPNUt, n));
    glEnableVertexAttribArray(1);
    // location 2: uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPNUt), (void*)offsetof(VertexPNUt, uv));
    glEnableVertexAttribArray(2);
    // location 3: tangent
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNUt), (void*)offsetof(VertexPNUt, t));
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return m;
}


// Процедурный ground (квад), тоже с tangent
Mesh CreateGround(float size, float uvScale)
{
    std::vector<VertexPNUt> v;
    v.resize(6);

    Vec3 n = { 0,1,0 };
    Vec3 t = { 1,0,0 };

    float s = size;
    // два треугольника
    v[0] = { {-s,0,-s}, n, {0,0}, t };
    v[1] = { { s,0, s},  n, {uvScale,uvScale}, t };
    v[2] = { { s,0,-s}, n, {uvScale,0}, t };

    v[3] = { {-s,0,-s}, n, {0,0}, t };
    v[4] = { {-s,0, s}, n, {0,uvScale}, t };
    v[5] = { { s,0, s},  n, {uvScale,uvScale}, t };

    return CreateMesh_PNUt(v);
}

// ======================= ШЕЙДЕРЫ (diffuse + normalmap + lightmap) =======================
const char* vertexShaderSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTex;
layout(location=3) in vec3 aTangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out VS_OUT {
    vec2 uv;
    vec3 worldPos;
    mat3 TBN;
} vs_out;

void main()
{
    vec3 T = normalize(mat3(uModel) * aTangent);
    vec3 N = normalize(mat3(uModel) * aNormal);
    // ортонормализация
    T = normalize(T - dot(T,N)*N);
    vec3 B = cross(N, T);

    vs_out.TBN = mat3(T, B, N);
    vec4 wp = uModel * vec4(aPos,1.0);
    vs_out.worldPos = wp.xyz;
    vs_out.uv = aTex;

    gl_Position = uProj * uView * wp;
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core

in VS_OUT {
    vec2 uv;
    vec3 worldPos;
    mat3 TBN;
} fs_in;

out vec4 FragColor;

uniform sampler2D uDiffuse;
uniform sampler2D uNormalMap;
uniform sampler2D uLightMap;

uniform bool uUseNormalMap;
uniform bool uUseLightMap;

uniform vec3 uLightDir;   // направленный свет (в мировых координатах), например normalize(vec3(-1,-1,-0.2))
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;

void main()
{
    vec3 albedo = texture(uDiffuse, fs_in.uv).rgb;

    vec3 N = normalize(fs_in.TBN[2]); // default из TBN (N axis)
    if(uUseNormalMap)
    {
        vec3 nrm = texture(uNormalMap, fs_in.uv).xyz * 2.0 - 1.0;
        N = normalize(fs_in.TBN * nrm);
    }

    vec3 L = normalize(-uLightDir); // свет "из направления"
    float ndotl = max(dot(N, L), 0.0);

    vec3 lighting = uAmbientColor + uLightColor * ndotl;

    if(uUseLightMap)
    {
        // Lightmap обычно хранит “запечённый” свет; тут просто умножаем
        vec3 lm = texture(uLightMap, fs_in.uv).rgb;
        lighting *= lm;
    }

    vec3 color = albedo * lighting;
    FragColor = vec4(color, 1.0);
}
)";

// ======================= ИГРОВЫЕ СТРУКТУРЫ =======================
struct RenderObject
{
    Mesh* mesh = nullptr;
    GLuint texDiffuse = 0;
    GLuint texNormal = 0;
    GLuint texLight = 0;

    bool useNormal = false;
    bool useLight = false;

    Vec3 pos{ 0,0,0 };
    Vec3 rot{ 0,0,0 }; // rot.y = yaw, rot.x = pitch
    Vec3 scale{ 1,1,1 };

    float collisionRadius = 1.0f;
    bool alive = true;
};

void PlaceOnGround(RenderObject& o, float groundY = 0.0f, float extra = 0.0f)
{
    if (!o.mesh) return;
    o.pos.y = groundY - o.mesh->minY * o.scale.y + extra;
}


struct Package
{
    Vec3 pos{ 0,0,0 };
    Vec3 vel{ 0,-10,0 };
    float radius = 0.4f;
    bool alive = true;
};

static float deg2rad(float d) { return d * (float)M_PI / 180.0f; }

// матрица объекта (yaw + pitch + scale)
Mat4 BuildModelMatrix(const RenderObject& o)
{
    Mat4 T = Mat4::Translation(o.pos.x, o.pos.y, o.pos.z);
    Mat4 Ry = Mat4::RotationY(o.rot.y);
    Mat4 Rx = Mat4::RotationX(o.rot.x);
    Mat4 S = Mat4::Scale(o.scale.x, o.scale.y, o.scale.z);
    return T * Ry * Rx * S;
}

bool SphereHit(const Vec3& a, float ra, const Vec3& b, float rb)
{
    float d = Length(a - b);
    return d <= (ra + rb);
}

// ======================= MAIN =======================
int main()
{
    setlocale(LC_ALL, "ru_RU.utf8");
    srand((unsigned)time(nullptr));

    sf::Window window(
        sf::VideoMode({ 1200u, 900u }),
        "Airship Delivery (OpenGL 3.3, SFML)",
        sf::Style::Default
    );
    window.setFramerateLimit(60);
    window.setActive(true);

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cout << "glewInit failed: " << (const char*)glewGetErrorString(err) << "\n";
        return 1;
    }

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GLSL:   " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // --- Шейдеры ---
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    GLuint prog = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint uModelLoc = glGetUniformLocation(prog, "uModel");
    GLint uViewLoc = glGetUniformLocation(prog, "uView");
    GLint uProjLoc = glGetUniformLocation(prog, "uProj");

    GLint uDiffuseLoc = glGetUniformLocation(prog, "uDiffuse");
    GLint uNormalLoc = glGetUniformLocation(prog, "uNormalMap");
    GLint uLightLoc = glGetUniformLocation(prog, "uLightMap");

    GLint uUseNormalLoc = glGetUniformLocation(prog, "uUseNormalMap");
    GLint uUseLightmapLoc = glGetUniformLocation(prog, "uUseLightMap");

    GLint uLightDirLoc = glGetUniformLocation(prog, "uLightDir");
    GLint uLightColorLoc = glGetUniformLocation(prog, "uLightColor");
    GLint uAmbientLoc = glGetUniformLocation(prog, "uAmbientColor");

    // --- Проекция ---
    auto makeProjection = [&](unsigned w, unsigned h)
        {
            float aspect = (h == 0) ? 1.0f : (float)w / (float)h;
            return Mat4::Perspective(deg2rad(60.0f), aspect, 0.1f, 1000.0f);
        };
    Mat4 proj = makeProjection(window.getSize().x, window.getSize().y);

    // ======================= ЗАГРУЗКА МЕШЕЙ =======================
    // Ground
    Mesh groundMesh = CreateGround(80.0f, 20.0f);

    // Airship
    std::vector<VertexPNUt> airshipVerts;
    if (!LoadOBJ_PNU("assets/airship.obj", airshipVerts)) return 1;
    Mesh airshipMesh = CreateMesh_PNUt(airshipVerts);

    // Tree
    std::vector<VertexPNUt> treeVerts;
    if (!LoadOBJ_PNU("assets/tree.obj", treeVerts)) return 1;
    Mesh treeMesh = CreateMesh_PNUt(treeVerts);

    // House (target)
    std::vector<VertexPNUt> houseVerts;
    if (!LoadOBJ_PNU("assets/house.obj", houseVerts)) return 1;
    Mesh houseMesh = CreateMesh_PNUt(houseVerts);

    // Decorations
    std::vector<VertexPNUt> deco1Verts;
    if (!LoadOBJ_PNU("assets/deco1.obj", deco1Verts)) return 1;
    Mesh deco1Mesh = CreateMesh_PNUt(deco1Verts);

    std::vector<VertexPNUt> deco2Verts;
    if (!LoadOBJ_PNU("assets/deco2.obj", deco2Verts)) return 1;
    Mesh deco2Mesh = CreateMesh_PNUt(deco2Verts);

    // ======================= ТЕКСТУРЫ =======================
    GLuint groundTex = LoadTextureFromFile("assets/ground_diffuse.png", true);
    if (!groundTex) {
        // не критично: если нет текстуры земли — просто продолжим, но будет чёрной (можешь заменить на белую 1x1)
        std::cout << "WARN: no ground texture. Put assets/ground_diffuse.png\n";
    }
    std::cout << "groundTex id = " << groundTex << "\n";

    GLuint airshipDiffuse = LoadTextureFromFile("assets/airship_diffuse.png", true);
    GLuint airshipNormal = LoadTextureFromFile("assets/airship_normal.png", false);
    if (!airshipDiffuse || !airshipNormal)
    {
        std::cout << "Need airship_diffuse + airship_normal\n";
        return 1;
    }

    GLuint treeDiffuse = LoadTextureFromFile("assets/tree_diffuse.png", true);
    if (!treeDiffuse) return 1;

    GLuint houseDiffuse = LoadTextureFromFile("assets/house_diffuse.png", true);
    GLuint houseLight = LoadTextureFromFile("assets/house_lightmap.png", false);
    if (!houseDiffuse || !houseLight)
    {
        std::cout << "Need house_diffuse + house_lightmap\n";
        return 1;
    }

    GLuint deco1Tex = LoadTextureFromFile("assets/deco1_diffuse.png", true);
    GLuint deco2Tex = LoadTextureFromFile("assets/deco2_diffuse.png", true);
    if (!deco1Tex || !deco2Tex) return 1;

    // ======================= ОБЪЕКТЫ СЦЕНЫ =======================
    RenderObject ground;
    ground.mesh = &groundMesh;
    ground.texDiffuse = groundTex;
    ground.useNormal = false;
    ground.useLight = false;
    ground.scale = { 1,1,1 };
    ground.pos = { 0, 0, 0 };
    ground.collisionRadius = 99999.0f; // не нужно

    RenderObject tree;
    tree.mesh = &treeMesh;
    tree.texDiffuse = treeDiffuse;
    tree.pos = { 0,0,0 };
    tree.scale = { 0.005f, 0.005f, 0.005f };
    PlaceOnGround(tree, 0.0f);
    tree.collisionRadius = 2.5f;

    RenderObject airship;
    airship.mesh = &airshipMesh;
    airship.texDiffuse = airshipDiffuse;
    airship.texNormal = airshipNormal;
    airship.useNormal = true;
    airship.useLight = false;
    airship.pos = { 0, 12.0f, 20.0f };
    airship.scale = { 0.0005f,0.0005f,0.0005f };
    airship.rot = { 0.0f, 0.0f, 0.0f };
    airship.collisionRadius = 2.0f;

    // Рандомные цели и декорации
    std::vector<RenderObject> targets;
    std::vector<RenderObject> decos;

    auto regenerateScene = [&]()
        {
            targets.clear();
            decos.clear();

            // >=5 домиков
            int targetCount = 7;
            for (int i = 0; i < targetCount; i++)
            {
                RenderObject h;
                h.mesh = &houseMesh;
                h.texDiffuse = houseDiffuse;
                h.texLight = houseLight;
                h.useLight = true;
                h.useNormal = false;
                h.alive = true;

                // рандомная позиция, не слишком близко к ёлке
                for (int tries = 0; tries < 100; ++tries)
                {
                    float x = frand(-55.0f, 55.0f);
                    float z = frand(-55.0f, 55.0f);
                    Vec3 p = { x, 0.0f, z };
                    if (Length(p - tree.pos) > 8.0f)
                    {
                        h.pos = p;
                        break;
                    }
                }

                h.scale = { 2.0f,2.0f,2.0f };
                PlaceOnGround(h, 0.0f);
                h.rot.y = frand(0.0f, (float)M_PI * 2.0f);
                h.collisionRadius = 2.2f;

                targets.push_back(h);
            }

            // декорации: минимум 2 вида, накидаем по 10
            for (int i = 0; i < 10; i++)
            {
                RenderObject d;
                d.mesh = &deco1Mesh;
                d.texDiffuse = deco1Tex;
                d.scale = { frand(1.0f, 2.5f), frand(1.0f, 2.5f), frand(1.0f, 2.5f) };
                d.rot.y = frand(0.0f, (float)M_PI * 2.0f);

                float x = frand(-60.0f, 60.0f);
                float z = frand(-60.0f, 60.0f);
                d.pos = { x, 0.0f, z };
                PlaceOnGround(d, 0.0f);
                if (Length(d.pos - tree.pos) < 6.0f) d.pos = d.pos + Vec3(8, 0, 8);

                d.alive = true;
                d.collisionRadius = 0.0f;
                decos.push_back(d);
            }
            for (int i = 0; i < 10; i++)
            {
                RenderObject d;
                d.mesh = &deco2Mesh;
                d.texDiffuse = deco2Tex;
                d.scale = { frand(1.0f, 2.5f), frand(1.0f, 2.5f), frand(1.0f, 2.5f) };
                d.rot.y = frand(0.0f, (float)M_PI * 2.0f);

                float x = frand(-60.0f, 60.0f);
                float z = frand(-60.0f, 60.0f);
                d.pos = { x, 0.0f, z };
                PlaceOnGround(d, 0.0f);
                if (Length(d.pos - tree.pos) < 6.0f) d.pos = d.pos + Vec3(-8, 0, 8);

                d.alive = true;
                d.collisionRadius = 0.0f;
                decos.push_back(d);
            }

            std::cout << "Scene regenerated: targets=" << targets.size()
                << ", decos=" << decos.size() << "\n";
        };

    regenerateScene();

    // ======================= ПАКЕТЫ =======================
    std::vector<Package> packages;
    int hits = 0;

    // ======================= КАМЕРА (следует за дирижаблем) =======================
    Vec3 worldUp(0, 1, 0);
    float camYaw = 0.0f;     // будем брать из дирижабля
    float camPitch = deg2rad(-15.0f);
    float camDist = 18.0f;
    float camHeight = 8.0f;

    // ======================= ВРЕМЯ/УПРАВЛЕНИЕ =======================
    sf::Clock clock;
    float moveSpeed = 20.0f;
    float rotSpeed = deg2rad(80.0f);

    bool prevDrop = false;
    bool prevR = false;

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        while (auto ev = window.pollEvent())
        {
            if (ev->is<sf::Event::Closed>()) window.close();

            if (const auto* resized = ev->getIf<sf::Event::Resized>())
            {
                glViewport(0, 0, resized->size.x, resized->size.y);
                proj = makeProjection(resized->size.x, resized->size.y);
            }
        }

        /*if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
            window.close();*/

        // ====== Управление дирижаблем ======
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
            airship.rot.y -= rotSpeed * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
            airship.rot.y += rotSpeed * dt;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
            camPitch += deg2rad(30.0f) * dt;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
            camPitch -= deg2rad(30.0f) * dt;

        camPitch = std::clamp(camPitch, deg2rad(-60.0f), deg2rad(-5.0f));

        Vec3 forward = { std::sin(airship.rot.y), 0.0f, std::cos(airship.rot.y) };
        forward = Normalize(forward);
        Vec3 right = Normalize(Cross(forward, worldUp));

        Vec3 delta(0, 0, 0);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) delta = delta + forward * (moveSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) delta = delta - forward * (moveSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) delta = delta - right * (moveSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) delta = delta + right * (moveSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))  delta = delta + worldUp * (moveSpeed * dt);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)) delta = delta - worldUp * (moveSpeed * dt);

        airship.pos = airship.pos + delta;

        // Ограничим высоту, чтобы не улетал в бесконечность
        airship.pos.y = std::clamp(airship.pos.y, 3.0f, 40.0f);

        // ====== Сброс посылки ======
        bool dropNow = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter);
        if (dropNow && !prevDrop)
        {
            Package p;
            p.pos = airship.pos + Vec3(0.0f, -1.0f, 0.0f);
            p.vel = Vec3(0.0f, -25.0f, 0.0f);
            p.radius = 0.6f;
            p.alive = true;
            packages.push_back(p);
        }
        prevDrop = dropNow;

        // ====== Перегенерация сцены ======
        bool rNow = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R);
        if (rNow && !prevR)
        {
            regenerateScene();
            packages.clear();
            hits = 0;
        }
        prevR = rNow;

        // ====== Обновление посылок + коллизии ======
        for (auto& p : packages)
        {
            if (!p.alive) continue;

            p.pos = p.pos + p.vel * dt;

            // удар о землю
            if (p.pos.y <= 0.0f)
            {
                p.alive = false;
                continue;
            }

            // столкновение с целями
            for (auto& t : targets)
            {
                if (!t.alive) continue;
                if (SphereHit(p.pos, p.radius, t.pos + Vec3(0, 1.5f, 0), t.collisionRadius))
                {
                    t.alive = false;
                    p.alive = false;
                    hits++;
                    std::cout << "Hit! total=" << hits << "\n";
                    break;
                }
            }
        }

        // чистка "мертвых" посылок
        packages.erase(
            std::remove_if(packages.begin(), packages.end(), [](const Package& p) { return !p.alive; }),
            packages.end()
        );

        // ======================= КАМЕРА СЗАДИ-СВЕРХУ =======================
        camYaw = airship.rot.y;

        // offset: назад по forward + вверх
        Vec3 camPos = airship.pos - forward * camDist + worldUp * camHeight;
        // немного "наклон" вниз реализуем через lookAt в точку дирижабля
        Vec3 camTarget = airship.pos + forward * 2.0f;

        Mat4 view = Mat4::LookAt(camPos, camTarget, worldUp);

        // ======================= РЕНДЕР =======================
        glClearColor(0.03f, 0.05f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog);

        // глобальный направленный свет
        Vec3 lightDir = Normalize(Vec3(-1.0f, -1.2f, -0.2f));
        glUniform3f(uLightDirLoc, lightDir.x, lightDir.y, lightDir.z);
        glUniform3f(uLightColorLoc, 1.0f, 1.0f, 1.0f);
        glUniform3f(uAmbientLoc, 0.25f, 0.25f, 0.28f);

        glUniformMatrix4fv(uViewLoc, 1, GL_FALSE, view.m);
        glUniformMatrix4fv(uProjLoc, 1, GL_FALSE, proj.m);

        auto drawObj = [&](const RenderObject& o)
            {
                if (!o.alive) return;
                if (!o.mesh) return;

                Mat4 model = BuildModelMatrix(o);
                glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, model.m);

                glUniform1i(uUseNormalLoc, o.useNormal ? 1 : 0);
                glUniform1i(uUseLightmapLoc, o.useLight ? 1 : 0);

                // текстурные слоты:
                // 0 diffuse, 1 normal, 2 light
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, o.texDiffuse);
                glUniform1i(uDiffuseLoc, 0);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, o.texNormal ? o.texNormal : 0);
                glUniform1i(uNormalLoc, 1);

                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, o.texLight ? o.texLight : 0);
                glUniform1i(uLightLoc, 2);

                glBindVertexArray(o.mesh->VAO);
                glDrawArrays(GL_TRIANGLES, 0, o.mesh->vertexCount);

                glBindVertexArray(0);
            };

        // Земля (если нет текстуры — будет 0 => чёрное, лучше положить ground_diffuse.png)
        drawObj(ground);

        // Ёлка в поле
        drawObj(tree);

        // Домики-цели (lightmap)
        for (const auto& t : targets) drawObj(t);

        // Декорации
        for (const auto& d : decos) drawObj(d);

        // Дирижабль (normalmap)
        drawObj(airship);

        // Посылки — чтобы не городить отдельный меш, рисуем как маленькие "домики" или "deco1"
        // (можешь заменить на отдельный cube.obj)
        RenderObject pkgObj;
        pkgObj.mesh = &deco1Mesh;
        pkgObj.texDiffuse = deco1Tex;
        pkgObj.useNormal = false;
        pkgObj.useLight = false;
        pkgObj.scale = { 0.5f,0.5f,0.5f };
        pkgObj.rot = { 0,0,0 };

        for (const auto& p : packages)
        {
            pkgObj.pos = p.pos;
            drawObj(pkgObj);
        }

        glUseProgram(0);
        window.display();

        // Можно обновлять заголовок окна счётчиком
        static float titleTimer = 0.0f;
        titleTimer += dt;
        if (titleTimer > 0.3f)
        {
            titleTimer = 0.0f;
            window.setTitle("Airship Delivery | hits=" + std::to_string(hits) +
                " | targets left=" + std::to_string(
                    std::count_if(targets.begin(), targets.end(),
                        [](const RenderObject& t) { return t.alive; })
                ));
        }
    }

    // ======================= CLEANUP =======================
    auto destroyMesh = [](Mesh& m)
        {
            if (m.VBO) glDeleteBuffers(1, &m.VBO);
            if (m.VAO) glDeleteVertexArrays(1, &m.VAO);
            m.VBO = 0; m.VAO = 0; m.vertexCount = 0;
        };

    destroyMesh(groundMesh);
    destroyMesh(airshipMesh);
    destroyMesh(treeMesh);
    destroyMesh(houseMesh);
    destroyMesh(deco1Mesh);
    destroyMesh(deco2Mesh);

    auto delTex = [](GLuint& t) { if (t) glDeleteTextures(1, &t); t = 0; };
    delTex(groundTex);
    delTex(airshipDiffuse);
    delTex(airshipNormal);
    delTex(treeDiffuse);
    delTex(houseDiffuse);
    delTex(houseLight);
    delTex(deco1Tex);
    delTex(deco2Tex);

    glDeleteProgram(prog);
    return 0;
}
