// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFakeWebGraphicsContext3D_h
#define CompositorFakeWebGraphicsContext3D_h

#include "base/memory/scoped_ptr.h"
#include "cc/test/fake_web_graphics_context_3d.h"

namespace WebKit {

// Test stub for WebGraphicsContext3D. Returns canned values needed for compositor initialization.
class CompositorFakeWebGraphicsContext3D : public FakeWebGraphicsContext3D {
public:
    static scoped_ptr<CompositorFakeWebGraphicsContext3D> create(Attributes attrs)
    {
        return make_scoped_ptr(new CompositorFakeWebGraphicsContext3D(attrs));
    }

    virtual bool makeContextCurrent() { return true; }
    virtual WebGLId createProgram() { return 1; }
    virtual WebGLId createShader(WGC3Denum) { return 1; }
    virtual void getShaderiv(WebGLId, WGC3Denum, WGC3Dint* value) { *value = 1; }
    virtual void getProgramiv(WebGLId, WGC3Denum, WGC3Dint* value) { *value = 1; }

protected:
    explicit CompositorFakeWebGraphicsContext3D(Attributes attrs)
    {
        m_attrs = attrs;
    }
};

}

#endif
