// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextureCopier_h
#define TextureCopier_h

#include "GraphicsContext3D.h"
#include "ProgramBinding.h"
#include "ShaderChromium.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Noncopyable.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {
class IntSize;

class TextureCopier {
public:
    struct Parameters {
        unsigned sourceTexture;
        unsigned destTexture;
        IntSize size;
    };
    // Copy the base level contents of |sourceTexture| to |destTexture|. Both texture objects
    // must be complete and have a base level of |size| dimensions. The color formats do not need
    // to match, but |destTexture| must have a renderable format.
    virtual void copyTexture(Parameters) = 0;
    virtual void flush() = 0;

    virtual ~TextureCopier() { }
};

#if USE(ACCELERATED_COMPOSITING)

class AcceleratedTextureCopier : public TextureCopier {
    WTF_MAKE_NONCOPYABLE(AcceleratedTextureCopier);
public:
    static PassOwnPtr<AcceleratedTextureCopier> create(WebKit::WebGraphicsContext3D* context, bool usingBindUniforms)
    {
        return adoptPtr(new AcceleratedTextureCopier(context, usingBindUniforms));
    }
    virtual ~AcceleratedTextureCopier();

    virtual void copyTexture(Parameters) OVERRIDE;
    virtual void flush() OVERRIDE;

protected:
    AcceleratedTextureCopier(WebKit::WebGraphicsContext3D*, bool usingBindUniforms);

private:
    typedef ProgramBinding<VertexShaderPosTexIdentity, FragmentShaderRGBATex> BlitProgram;

    WebKit::WebGraphicsContext3D* m_context;
    Platform3DObject m_fbo;
    Platform3DObject m_positionBuffer;
    OwnPtr<BlitProgram> m_blitProgram;
    bool m_usingBindUniforms;
};

#endif // USE(ACCELERATED_COMPOSITING)

}

#endif
