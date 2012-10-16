// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef FakeCCLayerTreeHostClient_h
#define FakeCCLayerTreeHostClient_h

#include "config.h"

#include "CCInputHandler.h"
#include "CCLayerTreeHost.h"
#include "base/memory/scoped_ptr.h"
#include "cc/test/compositor_fake_web_graphics_context_3d.h"
#include "cc/test/fake_web_compositor_output_surface.h"

namespace cc {

class FakeCCLayerTreeHostClient : public CCLayerTreeHostClient {
public:
    virtual void willBeginFrame() OVERRIDE { }
    virtual void didBeginFrame() OVERRIDE { }
    virtual void animate(double monotonicFrameBeginTime) OVERRIDE { }
    virtual void layout() OVERRIDE { }
    virtual void applyScrollAndScale(const IntSize& scrollDelta, float pageScale) OVERRIDE { }

    virtual scoped_ptr<WebKit::WebCompositorOutputSurface> createOutputSurface() OVERRIDE;
    virtual void didRecreateOutputSurface(bool success) OVERRIDE { }
    virtual scoped_ptr<CCInputHandler> createInputHandler() OVERRIDE;
    virtual void willCommit() OVERRIDE { }
    virtual void didCommit() OVERRIDE { }
    virtual void didCommitAndDrawFrame() OVERRIDE { }
    virtual void didCompleteSwapBuffers() OVERRIDE { }

    // Used only in the single-threaded path.
    virtual void scheduleComposite() OVERRIDE { }
};

}
#endif // FakeCCLayerTreeHostClient_h
