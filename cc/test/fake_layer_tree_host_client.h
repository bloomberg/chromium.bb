// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "cc/input_handler.h"
#include "cc/layer_tree_host.h"
#include "cc/test/fake_context_provider.h"
#include "cc/test/fake_output_surface.h"

namespace cc {

class FakeLayerImplTreeHostClient : public LayerTreeHostClient {
public:
    FakeLayerImplTreeHostClient(bool useSoftwareRendering = false, bool useDelegatingRenderer = false);
    virtual ~FakeLayerImplTreeHostClient();

    virtual void willBeginFrame() OVERRIDE { }
    virtual void didBeginFrame() OVERRIDE { }
    virtual void animate(double monotonicFrameBeginTime) OVERRIDE { }
    virtual void layout() OVERRIDE { }
    virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float pageScale) OVERRIDE { }

    virtual scoped_ptr<OutputSurface> createOutputSurface() OVERRIDE;
    virtual void didRecreateOutputSurface(bool success) OVERRIDE { }
    virtual scoped_ptr<InputHandler> createInputHandler() OVERRIDE;
    virtual void willCommit() OVERRIDE { }
    virtual void didCommit() OVERRIDE { }
    virtual void didCommitAndDrawFrame() OVERRIDE { }
    virtual void didCompleteSwapBuffers() OVERRIDE { }

    // Used only in the single-threaded path.
    virtual void scheduleComposite() OVERRIDE { }

    virtual scoped_refptr<cc::ContextProvider> OffscreenContextProviderForMainThread() OVERRIDE;
    virtual scoped_refptr<cc::ContextProvider> OffscreenContextProviderForCompositorThread() OVERRIDE;

private:
    bool m_useSoftwareRendering;
    bool m_useDelegatingRenderer;

    scoped_refptr<FakeContextProvider> m_mainThreadContexts;
    scoped_refptr<FakeContextProvider> m_compositorThreadContexts;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_
