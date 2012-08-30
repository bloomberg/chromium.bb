// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeWebCompositorOutputSurface_h
#define FakeWebCompositorOutputSurface_h

#include <public/WebCompositorOutputSurface.h>
#include <public/WebGraphicsContext3D.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class FakeWebCompositorOutputSurface : public WebCompositorOutputSurface {
public:
    static inline PassOwnPtr<FakeWebCompositorOutputSurface> create(PassOwnPtr<WebGraphicsContext3D> context3D)
    {
        return adoptPtr(new FakeWebCompositorOutputSurface(context3D));
    }


    virtual bool bindToClient(WebCompositorOutputSurfaceClient* client) OVERRIDE
    {
        ASSERT(client);
        if (!m_context3D->makeContextCurrent())
            return false;
        m_client = client;
        return true;
    }

    virtual const Capabilities& capabilities() const OVERRIDE
    {
        return m_capabilities;
    }

    virtual WebGraphicsContext3D* context3D() const OVERRIDE
    {
        return m_context3D.get();
    }

    virtual void sendFrameToParentCompositor(const WebCompositorFrame&) OVERRIDE
    {
    }

private:
    explicit FakeWebCompositorOutputSurface(PassOwnPtr<WebGraphicsContext3D> context3D)
    {
        m_context3D = context3D;
    }

    OwnPtr<WebGraphicsContext3D> m_context3D;
    Capabilities m_capabilities;
    WebCompositorOutputSurfaceClient* m_client;
};

} // namespace WebKit

#endif // FakeWebCompositorOutputSurface_h
