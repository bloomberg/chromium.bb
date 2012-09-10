// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShaderChromium_h
#define ShaderChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "SkColorPriv.h"
#include <string>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class VertexShaderPosTex {
public:
    VertexShaderPosTex();

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    std::string getShaderString() const;

    int matrixLocation() const { return m_matrixLocation; }

private:
    int m_matrixLocation;
};

class VertexShaderPosTexYUVStretch {
public:
    VertexShaderPosTexYUVStretch();

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    std::string getShaderString() const;

    int matrixLocation() const { return m_matrixLocation; }
    int yWidthScaleFactorLocation() const { return m_yWidthScaleFactorLocation; }
    int uvWidthScaleFactorLocation() const { return m_uvWidthScaleFactorLocation; }

private:
    int m_matrixLocation;
    int m_yWidthScaleFactorLocation;
    int m_uvWidthScaleFactorLocation;
};

class VertexShaderPos {
public:
    VertexShaderPos();

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    std::string getShaderString() const;

    int matrixLocation() const { return m_matrixLocation; }

private:
    int m_matrixLocation;
};

class VertexShaderPosTexIdentity {
public:
    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex) { }
    std::string getShaderString() const;
};

class VertexShaderPosTexTransform {
public:
    VertexShaderPosTexTransform();

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    std::string getShaderString() const;

    int matrixLocation() const { return m_matrixLocation; }
    int texTransformLocation() const { return m_texTransformLocation; }

private:
    int m_matrixLocation;
    int m_texTransformLocation;
};

class VertexShaderQuad {
public:
    VertexShaderQuad();

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    std::string getShaderString() const;

    int matrixLocation() const { return m_matrixLocation; }
    int pointLocation() const { return m_pointLocation; }

private:
    int m_matrixLocation;
    int m_pointLocation;
};

class VertexShaderTile {
public:
    VertexShaderTile();

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    std::string getShaderString() const;

    int matrixLocation() const { return m_matrixLocation; }
    int pointLocation() const { return m_pointLocation; }
    int vertexTexTransformLocation() const { return m_vertexTexTransformLocation; }

private:
    int m_matrixLocation;
    int m_pointLocation;
    int m_vertexTexTransformLocation;
};

class VertexShaderVideoTransform {
public:
    VertexShaderVideoTransform();

    bool init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    std::string getShaderString() const;

    int matrixLocation() const { return m_matrixLocation; }
    int texMatrixLocation() const { return m_texMatrixLocation; }

private:
    int m_matrixLocation;
    int m_texMatrixLocation;
};

class FragmentTexAlphaBinding {
public:
    FragmentTexAlphaBinding();

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    int alphaLocation() const { return m_alphaLocation; }
    int edgeLocation() const { return -1; }
    int fragmentTexTransformLocation() const { return -1; }
    int samplerLocation() const { return m_samplerLocation; }

private:
    int m_samplerLocation;
    int m_alphaLocation;
};

class FragmentTexOpaqueBinding {
public:
    FragmentTexOpaqueBinding();

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    int alphaLocation() const { return -1; }
    int edgeLocation() const { return -1; }
    int fragmentTexTransformLocation() const { return -1; }
    int samplerLocation() const { return m_samplerLocation; }

private:
    int m_samplerLocation;
};

class FragmentShaderRGBATexFlipAlpha : public FragmentTexAlphaBinding {
public:
    std::string getShaderString() const;
};

class FragmentShaderRGBATexAlpha : public FragmentTexAlphaBinding {
public:
    std::string getShaderString() const;
};

class FragmentShaderRGBATexRectFlipAlpha : public FragmentTexAlphaBinding {
public:
    std::string getShaderString() const;
};

class FragmentShaderRGBATexRectAlpha : public FragmentTexAlphaBinding {
public:
    std::string getShaderString() const;
};

class FragmentShaderRGBATexOpaque : public FragmentTexOpaqueBinding {
public:
    std::string getShaderString() const;
};

class FragmentShaderRGBATex : public FragmentTexOpaqueBinding {
public:
    std::string getShaderString() const;
};

// Swizzles the red and blue component of sampled texel with alpha.
class FragmentShaderRGBATexSwizzleAlpha : public FragmentTexAlphaBinding {
public:
    std::string getShaderString() const;
};

// Swizzles the red and blue component of sampled texel without alpha.
class FragmentShaderRGBATexSwizzleOpaque : public FragmentTexOpaqueBinding {
public:
    std::string getShaderString() const;
};

// Fragment shader for external textures.
class FragmentShaderOESImageExternal : public FragmentTexAlphaBinding {
public:
    std::string getShaderString() const;
    bool init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
private:
    int m_samplerLocation;
};

class FragmentShaderRGBATexAlphaAA {
public:
    FragmentShaderRGBATexAlphaAA();

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    std::string getShaderString() const;

    int alphaLocation() const { return m_alphaLocation; }
    int samplerLocation() const { return m_samplerLocation; }
    int edgeLocation() const { return m_edgeLocation; }

private:
    int m_samplerLocation;
    int m_alphaLocation;
    int m_edgeLocation;
};

class FragmentTexClampAlphaAABinding {
public:
    FragmentTexClampAlphaAABinding();

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    int alphaLocation() const { return m_alphaLocation; }
    int samplerLocation() const { return m_samplerLocation; }
    int fragmentTexTransformLocation() const { return m_fragmentTexTransformLocation; }
    int edgeLocation() const { return m_edgeLocation; }

private:
    int m_samplerLocation;
    int m_alphaLocation;
    int m_fragmentTexTransformLocation;
    int m_edgeLocation;
};

class FragmentShaderRGBATexClampAlphaAA : public FragmentTexClampAlphaAABinding {
public:
    std::string getShaderString() const;
};

// Swizzles the red and blue component of sampled texel.
class FragmentShaderRGBATexClampSwizzleAlphaAA : public FragmentTexClampAlphaAABinding {
public:
    std::string getShaderString() const;
};

class FragmentShaderRGBATexAlphaMask {
public:
    FragmentShaderRGBATexAlphaMask();
    std::string getShaderString() const;

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    int alphaLocation() const { return m_alphaLocation; }
    int samplerLocation() const { return m_samplerLocation; }
    int maskSamplerLocation() const { return m_maskSamplerLocation; }
    int maskTexCoordScaleLocation() const { return m_maskTexCoordScaleLocation; }
    int maskTexCoordOffsetLocation() const { return m_maskTexCoordOffsetLocation; }

private:
    int m_samplerLocation;
    int m_maskSamplerLocation;
    int m_alphaLocation;
    int m_maskTexCoordScaleLocation;
    int m_maskTexCoordOffsetLocation;
};

class FragmentShaderRGBATexAlphaMaskAA {
public:
    FragmentShaderRGBATexAlphaMaskAA();
    std::string getShaderString() const;

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    int alphaLocation() const { return m_alphaLocation; }
    int samplerLocation() const { return m_samplerLocation; }
    int maskSamplerLocation() const { return m_maskSamplerLocation; }
    int edgeLocation() const { return m_edgeLocation; }
    int maskTexCoordScaleLocation() const { return m_maskTexCoordScaleLocation; }
    int maskTexCoordOffsetLocation() const { return m_maskTexCoordOffsetLocation; }

private:
    int m_samplerLocation;
    int m_maskSamplerLocation;
    int m_alphaLocation;
    int m_edgeLocation;
    int m_maskTexCoordScaleLocation;
    int m_maskTexCoordOffsetLocation;
};

class FragmentShaderYUVVideo {
public:
    FragmentShaderYUVVideo();
    std::string getShaderString() const;

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);

    int yTextureLocation() const { return m_yTextureLocation; }
    int uTextureLocation() const { return m_uTextureLocation; }
    int vTextureLocation() const { return m_vTextureLocation; }
    int alphaLocation() const { return m_alphaLocation; }
    int ccMatrixLocation() const { return m_ccMatrixLocation; }
    int yuvAdjLocation() const { return m_yuvAdjLocation; }

private:
    int m_yTextureLocation;
    int m_uTextureLocation;
    int m_vTextureLocation;
    int m_alphaLocation;
    int m_ccMatrixLocation;
    int m_yuvAdjLocation;
};

class FragmentShaderColor {
public:
    FragmentShaderColor();
    std::string getShaderString() const;

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    int colorLocation() const { return m_colorLocation; }

private:
    int m_colorLocation;
};

class FragmentShaderCheckerboard {
public:
    FragmentShaderCheckerboard();
    std::string getShaderString() const;

    void init(WebKit::WebGraphicsContext3D*, unsigned program, bool usingBindUniform, int* baseUniformIndex);
    int alphaLocation() const { return m_alphaLocation; }
    int texTransformLocation() const { return m_texTransformLocation; }
    int frequencyLocation() const { return m_frequencyLocation; }
private:
    int m_alphaLocation;
    int m_texTransformLocation;
    int m_frequencyLocation;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
