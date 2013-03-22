// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/shader.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

#define SHADER0(Src) #Src
#define SHADER(Src) SHADER0(Src)

using WebKit::WebGraphicsContext3D;

namespace cc {

namespace {

static void getProgramUniformLocations(WebGraphicsContext3D* context, unsigned program, const char** shaderUniforms, size_t count, size_t maxLocations, int* locations, bool usingBindUniform, int* baseUniformIndex)
{
    for (size_t uniformIndex = 0; uniformIndex < count; uniformIndex ++) {
        DCHECK(uniformIndex < maxLocations);

        if (usingBindUniform) {
            locations[uniformIndex] = (*baseUniformIndex)++;
            context->bindUniformLocationCHROMIUM(program, locations[uniformIndex], shaderUniforms[uniformIndex]);
        } else
            locations[uniformIndex] = context->getUniformLocation(program, shaderUniforms[uniformIndex]);
    }
}

}

VertexShaderPosTex::VertexShaderPosTex()
    : m_matrixLocation(-1)
{
}

void VertexShaderPosTex::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "matrix",
    };
    int locations[1];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_matrixLocation = locations[0];
    DCHECK(m_matrixLocation != -1);
}

std::string VertexShaderPosTex::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        varying vec2 v_texCoord;
        void main()
        {
            gl_Position = matrix * a_position;
            v_texCoord = a_texCoord;
        }
    );
}

VertexShaderPosTexYUVStretch::VertexShaderPosTexYUVStretch()
    : m_matrixLocation(-1)
    , m_texScaleLocation(-1)
{
}

void VertexShaderPosTexYUVStretch::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "matrix",
        "texScale",
    };
    int locations[2];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_matrixLocation = locations[0];
    m_texScaleLocation = locations[1];
    DCHECK(m_matrixLocation != -1 && m_texScaleLocation != -1);
}

std::string VertexShaderPosTexYUVStretch::getShaderString() const
{
    return SHADER(
        precision mediump float;
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        varying vec2 v_texCoord;
        uniform vec2 texScale;
        void main()
        {
            gl_Position = matrix * a_position;
            v_texCoord = a_texCoord * texScale;
        }
    );
}

VertexShaderPos::VertexShaderPos()
    : m_matrixLocation(-1)
{
}

void VertexShaderPos::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "matrix",
    };
    int locations[1];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_matrixLocation = locations[0];
    DCHECK(m_matrixLocation != -1);
}

std::string VertexShaderPos::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        uniform mat4 matrix;
        void main()
        {
            gl_Position = matrix * a_position;
        }
    );
}

VertexShaderPosTexTransform::VertexShaderPosTexTransform()
    : m_matrixLocation(-1)
    , m_texTransformLocation(-1)
    , m_vertexOpacityLocation(-1)
{
}

void VertexShaderPosTexTransform::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "matrix",
        "texTransform",
        "opacity",
    };
    int locations[3];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_matrixLocation = locations[0];
    m_texTransformLocation = locations[1];
    m_vertexOpacityLocation = locations[2];
    DCHECK(m_matrixLocation != -1 && m_texTransformLocation != -1 && m_vertexOpacityLocation != -1);
}

std::string VertexShaderPosTexTransform::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        attribute float a_index;
        uniform mat4 matrix[8];
        uniform vec4 texTransform[8];
        uniform float opacity[32];
        varying vec2 v_texCoord;
        varying float v_alpha;
        void main()
        {
            gl_Position = matrix[int(a_index * 0.25)] * a_position;
            vec4 texTrans = texTransform[int(a_index * 0.25)];
            v_texCoord = a_texCoord * texTrans.zw + texTrans.xy;
            v_alpha = opacity[int(a_index)];
        }
    );
}

std::string VertexShaderPosTexTransformFlip::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        attribute float a_index;
        uniform mat4 matrix[8];
        uniform vec4 texTransform[8];
        uniform float opacity[32];
        varying vec2 v_texCoord;
        varying float v_alpha;
        void main()
        {
            gl_Position = matrix[int(a_index * 0.25)] * a_position;
            vec4 texTrans = texTransform[int(a_index * 0.25)];
            v_texCoord = a_texCoord * texTrans.zw + texTrans.xy;
            v_texCoord.y = 1.0 - v_texCoord.y;
            v_alpha = opacity[int(a_index)];
        }
    );
}

std::string VertexShaderPosTexIdentity::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        varying vec2 v_texCoord;
        void main()
        {
            gl_Position = a_position;
            v_texCoord = (a_position.xy + vec2(1.0)) * 0.5;
        }
    );
}

VertexShaderQuad::VertexShaderQuad()
    : m_matrixLocation(-1)
    , m_pointLocation(-1)
    , m_texScaleLocation(-1)
{
}

void VertexShaderQuad::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    // TODO(reveman): Some uniform names are causing drawing artifact.
    // crbug.com/223014
    static const char* shaderUniforms[] = {
        "matrix",
        "point_bug223014",
        "texScale",
    };
    int locations[3];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_matrixLocation = locations[0];
    m_pointLocation = locations[1];
    m_texScaleLocation = locations[2];
    DCHECK_NE(m_matrixLocation, -1);
    DCHECK_NE(m_pointLocation, -1);
    DCHECK_NE(m_texScaleLocation, -1);
}

std::string VertexShaderQuad::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        uniform vec2 point_bug223014[4];
        uniform vec2 texScale;
        varying vec2 v_texCoord;
        void main()
        {
            vec2 complement = abs(a_texCoord - 1.0);
            vec4 pos = vec4(0.0, 0.0, a_position.z, a_position.w);
            pos.xy += (complement.x * complement.y) * point_bug223014[0];
            pos.xy += (a_texCoord.x * complement.y) * point_bug223014[1];
            pos.xy += (a_texCoord.x * a_texCoord.y) * point_bug223014[2];
            pos.xy += (complement.x * a_texCoord.y) * point_bug223014[3];
            gl_Position = matrix * pos;
            v_texCoord = (pos.xy + vec2(0.5)) * texScale;
        }
    );
}

VertexShaderTile::VertexShaderTile()
    : m_matrixLocation(-1)
    , m_pointLocation(-1)
    , m_vertexTexTransformLocation(-1)
{
}

void VertexShaderTile::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "matrix",
        "point_bug223014",
        "vertexTexTransform",
    };
    int locations[3];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_matrixLocation = locations[0];
    m_pointLocation = locations[1];
    m_vertexTexTransformLocation = locations[2];
    DCHECK(m_matrixLocation != -1 && m_pointLocation != -1 && m_vertexTexTransformLocation != -1);
}

std::string VertexShaderTile::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        uniform vec2 point_bug223014[4];
        uniform vec4 vertexTexTransform;
        varying vec2 v_texCoord;
        void main()
        {
            vec2 complement = abs(a_texCoord - 1.0);
            vec4 pos = vec4(0.0, 0.0, a_position.z, a_position.w);
            pos.xy += (complement.x * complement.y) * point_bug223014[0];
            pos.xy += (a_texCoord.x * complement.y) * point_bug223014[1];
            pos.xy += (a_texCoord.x * a_texCoord.y) * point_bug223014[2];
            pos.xy += (complement.x * a_texCoord.y) * point_bug223014[3];
            gl_Position = matrix * pos;
            v_texCoord = pos.xy * vertexTexTransform.zw + vertexTexTransform.xy;
        }
    );
}

VertexShaderVideoTransform::VertexShaderVideoTransform()
    : m_matrixLocation(-1)
    , m_texMatrixLocation(-1)
{
}

bool VertexShaderVideoTransform::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "matrix",
        "texMatrix",
    };
    int locations[2];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_matrixLocation = locations[0];
    m_texMatrixLocation = locations[1];
    return m_matrixLocation != -1 && m_texMatrixLocation != -1;
}

std::string VertexShaderVideoTransform::getShaderString() const
{
    return SHADER(
        attribute vec4 a_position;
        attribute vec2 a_texCoord;
        uniform mat4 matrix;
        uniform mat4 texMatrix;
        varying vec2 v_texCoord;
        void main()
        {
            gl_Position = matrix * a_position;
            v_texCoord = vec2(texMatrix * vec4(a_texCoord.x, 1.0 - a_texCoord.y, 0.0, 1.0));
        }
    );
}

FragmentTexAlphaBinding::FragmentTexAlphaBinding()
    : m_samplerLocation(-1)
    , m_alphaLocation(-1)
{
}

void FragmentTexAlphaBinding::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "s_texture",
        "alpha",
    };
    int locations[2];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_samplerLocation = locations[0];
    m_alphaLocation = locations[1];
    DCHECK(m_samplerLocation != -1 && m_alphaLocation != -1);
}

FragmentTexOpaqueBinding::FragmentTexOpaqueBinding()
    : m_samplerLocation(-1)
{
}

void FragmentTexOpaqueBinding::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "s_texture",
    };
    int locations[1];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_samplerLocation = locations[0];
    DCHECK(m_samplerLocation != -1);
}

bool FragmentShaderOESImageExternal::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "s_texture",
    };
    int locations[1];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_samplerLocation = locations[0];
    return m_samplerLocation != -1;
}

std::string FragmentShaderOESImageExternal::getShaderString() const
{
    // Cannot use the SHADER() macro because of the '#' char
    return "#extension GL_OES_EGL_image_external : require \n"
           "precision mediump float;\n"
           "varying vec2 v_texCoord;\n"
           "uniform samplerExternalOES s_texture;\n"
           "void main()\n"
           "{\n"
           "    vec4 texColor = texture2D(s_texture, v_texCoord);\n"
           "    gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w);\n"
           "}\n";
}

std::string FragmentShaderRGBATexAlpha::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = texColor * alpha;
        }
    );
}

std::string FragmentShaderRGBATexVaryingAlpha::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        varying float v_alpha;
        uniform sampler2D s_texture;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = texColor * v_alpha;
        }
    );
}

std::string FragmentShaderRGBATexRectVaryingAlpha::getShaderString() const
{
    return "#extension GL_ARB_texture_rectangle : require\n"
            "precision mediump float;\n"
            "varying vec2 v_texCoord;\n"
            "varying float v_alpha;\n"
            "uniform sampler2DRect s_texture;\n"
            "void main()\n"
            "{\n"
            "    vec4 texColor = texture2DRect(s_texture, v_texCoord);\n"
            "    gl_FragColor = texColor * v_alpha;\n"
            "}\n";
}

std::string FragmentShaderRGBATexOpaque::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = vec4(texColor.rgb, 1.0);
        }
    );
}

std::string FragmentShaderRGBATex::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        void main()
        {
            gl_FragColor = texture2D(s_texture, v_texCoord);
        }
    );
}

std::string FragmentShaderRGBATexSwizzleAlpha::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha;
        }
    );
}

std::string FragmentShaderRGBATexSwizzleOpaque::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, 1.0);
        }
    );
}

FragmentShaderRGBATexAlphaAA::FragmentShaderRGBATexAlphaAA()
    : m_samplerLocation(-1)
    , m_alphaLocation(-1)
    , m_edgeLocation(-1)
{
}

void FragmentShaderRGBATexAlphaAA::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "s_texture",
        "alpha",
        "edge",
    };
    int locations[3];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_samplerLocation = locations[0];
    m_alphaLocation = locations[1];
    m_edgeLocation = locations[2];
    DCHECK(m_samplerLocation != -1 && m_alphaLocation != -1 && m_edgeLocation != -1);
}

std::string FragmentShaderRGBATexAlphaAA::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        uniform vec3 edge[8];
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            vec3 pos = vec3(gl_FragCoord.xy, 1);
            float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
            float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
            float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
            float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
            float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
            float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
            float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
            float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
            gl_FragColor = texColor * alpha * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
        }
    );
}

FragmentTexClampAlphaAABinding::FragmentTexClampAlphaAABinding()
    : m_samplerLocation(-1)
    , m_alphaLocation(-1)
    , m_fragmentTexTransformLocation(-1)
    , m_edgeLocation(-1)
{
}

void FragmentTexClampAlphaAABinding::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "s_texture",
        "alpha",
        "fragmentTexTransform",
        "edge",
    };
    int locations[4];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_samplerLocation = locations[0];
    m_alphaLocation = locations[1];
    m_fragmentTexTransformLocation = locations[2];
    m_edgeLocation = locations[3];
    DCHECK(m_samplerLocation != -1 && m_alphaLocation != -1 && m_fragmentTexTransformLocation != -1 && m_edgeLocation != -1);
}

std::string FragmentShaderRGBATexClampAlphaAA::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        uniform vec4 fragmentTexTransform;
        uniform vec3 edge[8];
        void main()
        {
            vec2 texCoord = clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw + fragmentTexTransform.xy;
            vec4 texColor = texture2D(s_texture, texCoord);
            vec3 pos = vec3(gl_FragCoord.xy, 1);
            float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
            float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
            float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
            float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
            float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
            float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
            float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
            float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
            gl_FragColor = texColor * alpha * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
        }
    );
}

std::string FragmentShaderRGBATexClampSwizzleAlphaAA::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform float alpha;
        uniform vec4 fragmentTexTransform;
        uniform vec3 edge[8];
        void main()
        {
            vec2 texCoord = clamp(v_texCoord, 0.0, 1.0) * fragmentTexTransform.zw + fragmentTexTransform.xy;
            vec4 texColor = texture2D(s_texture, texCoord);
            vec3 pos = vec3(gl_FragCoord.xy, 1);
            float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
            float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
            float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
            float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
            float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
            float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
            float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
            float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
            gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
        }
    );
}

FragmentShaderRGBATexAlphaMask::FragmentShaderRGBATexAlphaMask()
    : m_samplerLocation(-1)
    , m_maskSamplerLocation(-1)
    , m_alphaLocation(-1)
    , m_maskTexCoordScaleLocation(-1)
{
}

void FragmentShaderRGBATexAlphaMask::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "s_texture",
        "s_mask",
        "alpha",
        "maskTexCoordScale",
        "maskTexCoordOffset",
    };
    int locations[5];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_samplerLocation = locations[0];
    m_maskSamplerLocation = locations[1];
    m_alphaLocation = locations[2];
    m_maskTexCoordScaleLocation = locations[3];
    m_maskTexCoordOffsetLocation = locations[4];
    DCHECK(m_samplerLocation != -1 && m_maskSamplerLocation != -1 && m_alphaLocation != -1);
}

std::string FragmentShaderRGBATexAlphaMask::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform sampler2D s_mask;
        uniform vec2 maskTexCoordScale;
        uniform vec2 maskTexCoordOffset;
        uniform float alpha;
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            vec2 maskTexCoord = vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x, maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);
            vec4 maskColor = texture2D(s_mask, maskTexCoord);
            gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha * maskColor.w;
        }
    );
}

FragmentShaderRGBATexAlphaMaskAA::FragmentShaderRGBATexAlphaMaskAA()
    : m_samplerLocation(-1)
    , m_maskSamplerLocation(-1)
    , m_alphaLocation(-1)
    , m_edgeLocation(-1)
    , m_maskTexCoordScaleLocation(-1)
{
}

void FragmentShaderRGBATexAlphaMaskAA::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "s_texture",
        "s_mask",
        "alpha",
        "edge",
        "maskTexCoordScale",
        "maskTexCoordOffset",
    };
    int locations[6];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_samplerLocation = locations[0];
    m_maskSamplerLocation = locations[1];
    m_alphaLocation = locations[2];
    m_edgeLocation = locations[3];
    m_maskTexCoordScaleLocation = locations[4];
    m_maskTexCoordOffsetLocation = locations[5];
    DCHECK(m_samplerLocation != -1 && m_maskSamplerLocation != -1 && m_alphaLocation != -1 && m_edgeLocation != -1);
}

std::string FragmentShaderRGBATexAlphaMaskAA::getShaderString() const
{
    return SHADER(
        precision mediump float;
        varying vec2 v_texCoord;
        uniform sampler2D s_texture;
        uniform sampler2D s_mask;
        uniform vec2 maskTexCoordScale;
        uniform vec2 maskTexCoordOffset;
        uniform float alpha;
        uniform vec3 edge[8];
        void main()
        {
            vec4 texColor = texture2D(s_texture, v_texCoord);
            vec2 maskTexCoord = vec2(maskTexCoordOffset.x + v_texCoord.x * maskTexCoordScale.x, maskTexCoordOffset.y + v_texCoord.y * maskTexCoordScale.y);
            vec4 maskColor = texture2D(s_mask, maskTexCoord);
            vec3 pos = vec3(gl_FragCoord.xy, 1);
            float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
            float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
            float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
            float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
            float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
            float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
            float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
            float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
            gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha * maskColor.w * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
        }
    );
}

FragmentShaderYUVVideo::FragmentShaderYUVVideo()
    : m_yTextureLocation(-1)
    , m_uTextureLocation(-1)
    , m_vTextureLocation(-1)
    , m_alphaLocation(-1)
    , m_yuvMatrixLocation(-1)
    , m_yuvAdjLocation(-1)
{
}

void FragmentShaderYUVVideo::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "y_texture",
        "u_texture",
        "v_texture",
        "alpha",
        "yuv_matrix",
        "yuv_adj",
    };
    int locations[6];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_yTextureLocation = locations[0];
    m_uTextureLocation = locations[1];
    m_vTextureLocation = locations[2];
    m_alphaLocation = locations[3];
    m_yuvMatrixLocation = locations[4];
    m_yuvAdjLocation = locations[5];

    DCHECK(m_yTextureLocation != -1 && m_uTextureLocation != -1 && m_vTextureLocation != -1
           && m_alphaLocation != -1 && m_yuvMatrixLocation != -1 && m_yuvAdjLocation != -1);
}

std::string FragmentShaderYUVVideo::getShaderString() const
{
    return SHADER(
        precision mediump float;
        precision mediump int;
        varying vec2 v_texCoord;
        uniform sampler2D y_texture;
        uniform sampler2D u_texture;
        uniform sampler2D v_texture;
        uniform float alpha;
        uniform vec3 yuv_adj;
        uniform mat3 yuv_matrix;
        void main()
        {
            float y_raw = texture2D(y_texture, v_texCoord).x;
            float u_unsigned = texture2D(u_texture, v_texCoord).x;
            float v_unsigned = texture2D(v_texture, v_texCoord).x;
            vec3 yuv = vec3(y_raw, u_unsigned, v_unsigned) + yuv_adj;
            vec3 rgb = yuv_matrix * yuv;
            gl_FragColor = vec4(rgb, float(1)) * alpha;
        }
    );
}

FragmentShaderColor::FragmentShaderColor()
    : m_colorLocation(-1)
{
}

void FragmentShaderColor::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "color",
    };
    int locations[1];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_colorLocation = locations[0];
    DCHECK(m_colorLocation != -1);
}

std::string FragmentShaderColor::getShaderString() const
{
    return SHADER(
        precision mediump float;
        uniform vec4 color;
        void main()
        {
            gl_FragColor = color;
        }
    );
}

FragmentShaderColorAA::FragmentShaderColorAA()
    : m_edgeLocation(-1)
    , m_colorLocation(-1)
{
}

void FragmentShaderColorAA::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "edge",
        "color",
    };
    int locations[2];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_edgeLocation = locations[0];
    m_colorLocation = locations[1];
    DCHECK(m_edgeLocation != -1 && m_colorLocation != -1);
}

std::string FragmentShaderColorAA::getShaderString() const
{
    return SHADER(
        precision mediump float;
        uniform vec4 color;
        uniform vec3 edge[8];
        void main()
        {
            vec3 pos = vec3(gl_FragCoord.xy, 1);
            float a0 = clamp(dot(edge[0], pos), 0.0, 1.0);
            float a1 = clamp(dot(edge[1], pos), 0.0, 1.0);
            float a2 = clamp(dot(edge[2], pos), 0.0, 1.0);
            float a3 = clamp(dot(edge[3], pos), 0.0, 1.0);
            float a4 = clamp(dot(edge[4], pos), 0.0, 1.0);
            float a5 = clamp(dot(edge[5], pos), 0.0, 1.0);
            float a6 = clamp(dot(edge[6], pos), 0.0, 1.0);
            float a7 = clamp(dot(edge[7], pos), 0.0, 1.0);
            gl_FragColor = color * min(min(a0, a2) * min(a1, a3), min(a4, a6) * min(a5, a7));
        }
    );
}

FragmentShaderCheckerboard::FragmentShaderCheckerboard()
    : m_alphaLocation(-1)
    , m_texTransformLocation(-1)
    , m_frequencyLocation(-1)
{
}

void FragmentShaderCheckerboard::init(WebGraphicsContext3D* context, unsigned program, bool usingBindUniform, int* baseUniformIndex)
{
    static const char* shaderUniforms[] = {
        "alpha",
        "texTransform",
        "frequency",
        "color",
    };
    int locations[4];

    getProgramUniformLocations(context, program, shaderUniforms, arraysize(shaderUniforms), arraysize(locations), locations, usingBindUniform, baseUniformIndex);

    m_alphaLocation = locations[0];
    m_texTransformLocation = locations[1];
    m_frequencyLocation = locations[2];
    m_colorLocation = locations[3];
    DCHECK(m_alphaLocation != -1 && m_texTransformLocation != -1 && m_frequencyLocation != -1 && m_colorLocation != -1);
}

std::string FragmentShaderCheckerboard::getShaderString() const
{
    // Shader based on Example 13-17 of "OpenGL ES 2.0 Programming Guide"
    // by Munshi, Ginsburg, Shreiner.
    return SHADER(
        precision mediump float;
        precision mediump int;
        varying vec2 v_texCoord;
        uniform float alpha;
        uniform float frequency;
        uniform vec4 texTransform;
        uniform vec4 color;
        void main()
        {
            vec4 color1 = vec4(1.0, 1.0, 1.0, 1.0);
            vec4 color2 = color;
            vec2 texCoord = clamp(v_texCoord, 0.0, 1.0) * texTransform.zw + texTransform.xy;
            vec2 coord = mod(floor(texCoord * frequency * 2.0), 2.0);
            float picker = abs(coord.x - coord.y);
            gl_FragColor = mix(color1, color2, picker) * alpha;
        }
    );
}

}  // namespace cc
