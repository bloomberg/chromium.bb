// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef FakeCCLayerTreeHostClient_h
#define FakeCCLayerTreeHostClient_h

#include "config.h"

#include "CCInputHandler.h"
#include "CCLayerTreeHost.h"
#include "CompositorFakeWebGraphicsContext3D.h"
#include "FakeWebCompositorOutputSurface.h"

namespace WebCore {

class FakeCCLayerTreeHostClient : public CCLayerTreeHostClient {
public:
    virtual void willBeginFrame() OVERRIDE { }
    virtual void didBeginFrame() OVERRIDE { }
    virtual void animate(double monotonicFrameBeginTime) OVERRIDE { }
    virtual void layout() OVERRIDE { }
    virtual void applyScrollAndScale(const IntSize& scrollDelta, float pageScale) OVERRIDE { }

    virtual PassOwnPtr<WebKit::WebCompositorOutputSurface> createOutputSurface() OVERRIDE
    {
        WebKit::WebGraphicsContext3D::Attributes attrs;
        return WebKit::FakeWebCompositorOutputSurface::create(WebKit::CompositorFakeWebGraphicsContext3D::create(attrs));
    }
    virtual void didRecreateOutputSurface(bool success) OVERRIDE { }
    virtual PassOwnPtr<CCInputHandler> createInputHandler() OVERRIDE { return nullptr; }
    virtual void willCommit() OVERRIDE { }
    virtual void didCommit() OVERRIDE { }
    virtual void didCommitAndDrawFrame() OVERRIDE { }
    virtual void didCompleteSwapBuffers() OVERRIDE { }

    // Used only in the single-threaded path.
    virtual void scheduleComposite() OVERRIDE { }
};

}
#endif // FakeCCLayerTreeHostClient_h
