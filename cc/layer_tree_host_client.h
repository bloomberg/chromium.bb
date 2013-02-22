// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_TREE_HOST_CLIENT_H_
#define CC_LAYER_TREE_HOST_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace gfx {
class Vector2d;
}

namespace cc {
class ContextProvider;
class InputHandler;
class OutputSurface;

class LayerTreeHostClient {
public:
    virtual void willBeginFrame() = 0;
    // Marks finishing compositing-related tasks on the main thread. In threaded mode, this corresponds to didCommit().
    virtual void didBeginFrame() = 0;
    virtual void animate(double frameBeginTime) = 0;
    virtual void layout() = 0;
    virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float pageScale) = 0;
    virtual scoped_ptr<OutputSurface> createOutputSurface() = 0;
    virtual void didRecreateOutputSurface(bool success) = 0;
    virtual scoped_ptr<InputHandler> createInputHandler() = 0;
    virtual void willCommit() = 0;
    virtual void didCommit() = 0;
    virtual void didCommitAndDrawFrame() = 0;
    virtual void didCompleteSwapBuffers() = 0;

    // Used only in the single-threaded path.
    virtual void scheduleComposite() = 0;

    // These must always return a valid ContextProvider. But the provider does not need to be capable of creating contexts.
    virtual scoped_refptr<cc::ContextProvider> OffscreenContextProviderForMainThread() = 0;
    virtual scoped_refptr<cc::ContextProvider> OffscreenContextProviderForCompositorThread() = 0;

    // This hook is for testing.
    virtual void willRetryRecreateOutputSurface() {}

protected:
    virtual ~LayerTreeHostClient() { }
};

}

#endif  // CC_LAYER_TREE_HOST_CLIENT_H_
