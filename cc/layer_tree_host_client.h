// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCLayerTreeHostClient_h
#define CCLayerTreeHostClient_h

#include "base/memory/scoped_ptr.h"

namespace WebKit {
class WebCompositorOutputSurface;
}

namespace cc {
class InputHandler;
class IntSize;

class LayerTreeHostClient {
public:
    virtual void willBeginFrame() = 0;
    // Marks finishing compositing-related tasks on the main thread. In threaded mode, this corresponds to didCommit().
    virtual void didBeginFrame() = 0;
    virtual void animate(double frameBeginTime) = 0;
    virtual void layout() = 0;
    virtual void applyScrollAndScale(const IntSize& scrollDelta, float pageScale) = 0;
    virtual scoped_ptr<WebKit::WebCompositorOutputSurface> createOutputSurface() = 0;
    virtual void didRecreateOutputSurface(bool success) = 0;
    virtual scoped_ptr<InputHandler> createInputHandler() = 0;
    virtual void willCommit() = 0;
    virtual void didCommit() = 0;
    virtual void didCommitAndDrawFrame() = 0;
    virtual void didCompleteSwapBuffers() = 0;

    // Used only in the single-threaded path.
    virtual void scheduleComposite() = 0;

protected:
    virtual ~LayerTreeHostClient() { }
};

}

#endif // CCLayerTreeHostClient_h
