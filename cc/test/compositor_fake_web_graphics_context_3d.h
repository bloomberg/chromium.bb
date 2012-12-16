// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_COMPOSITOR_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_
#define CC_TEST_COMPOSITOR_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_

#include "base/memory/scoped_ptr.h"
#include "cc/test/fake_web_graphics_context_3d.h"

namespace cc {

// Test stub for WebGraphicsContext3D. Returns canned values needed for compositor initialization.
class CompositorFakeWebGraphicsContext3D : public FakeWebGraphicsContext3D {
public:
    static scoped_ptr<CompositorFakeWebGraphicsContext3D> create(Attributes attrs)
    {
        return make_scoped_ptr(new CompositorFakeWebGraphicsContext3D(attrs));
    }

    virtual bool makeContextCurrent();
    virtual WebKit::WebGLId createProgram();
    virtual WebKit::WebGLId createShader(WebKit::WGC3Denum);
    virtual void getShaderiv(WebKit::WebGLId, WebKit::WGC3Denum, WebKit::WGC3Dint* value);
    virtual void getProgramiv(WebKit::WebGLId, WebKit::WGC3Denum, WebKit::WGC3Dint* value);

protected:
    explicit CompositorFakeWebGraphicsContext3D(Attributes attrs)
    {
        m_attrs = attrs;
    }
};

}  // namespace cc

#endif  // CC_TEST_COMPOSITOR_FAKE_WEB_GRAPHICS_CONTEXT_3D_H_
