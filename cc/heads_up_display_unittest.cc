// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/heads_up_display_layer.h"
#include "cc/layer.h"
#include "cc/layer_tree_host.h"
#include "cc/test/layer_tree_test_common.h"

namespace cc {
namespace {

class HeadsUpDisplayTest : public ThreadedTest {
protected:
    virtual void initializeSettings(LayerTreeSettings& settings) OVERRIDE
    {
        // Enable the HUD without requiring text.
        settings.initialDebugState.showPropertyChangedRects = true;
    }
};

class DrawsContentLayer : public Layer {
public:
    static scoped_refptr<DrawsContentLayer> Create() { return make_scoped_refptr(new DrawsContentLayer()); }
    virtual bool DrawsContent() const OVERRIDE { return true; }

private:
    DrawsContentLayer() : Layer() { }
    virtual ~DrawsContentLayer()
    {
    }
};

class HudWithRootLayerChange : public HeadsUpDisplayTest {
public:
    HudWithRootLayerChange()
        : m_rootLayer1(DrawsContentLayer::Create())
        , m_rootLayer2(DrawsContentLayer::Create())
        , m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_rootLayer1->SetBounds(gfx::Size(30, 30));
        m_rootLayer2->SetBounds(gfx::Size(30, 30));

        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        ++m_numCommits;

        ASSERT_TRUE(m_layerTreeHost->hud_layer());

        switch (m_numCommits) {
        case 1:
            // Change directly to a new root layer.
            m_layerTreeHost->SetRootLayer(m_rootLayer1);
            break;
        case 2:
            EXPECT_EQ(m_rootLayer1.get(), m_layerTreeHost->hud_layer()->parent());
            // Unset the root layer.
            m_layerTreeHost->SetRootLayer(NULL);
            break;
        case 3:
            EXPECT_EQ(0, m_layerTreeHost->hud_layer()->parent());
            // Change back to the previous root layer.
            m_layerTreeHost->SetRootLayer(m_rootLayer1);
            break;
        case 4:
            EXPECT_EQ(m_rootLayer1.get(), m_layerTreeHost->hud_layer()->parent());
            // Unset the root layer.
            m_layerTreeHost->SetRootLayer(NULL);
            break;
        case 5:
            EXPECT_EQ(0, m_layerTreeHost->hud_layer()->parent());
            // Change to a new root layer from a null root.
            m_layerTreeHost->SetRootLayer(m_rootLayer2);
            break;
        case 6:
            EXPECT_EQ(m_rootLayer2.get(), m_layerTreeHost->hud_layer()->parent());
            // Change directly back to the last root layer/
            m_layerTreeHost->SetRootLayer(m_rootLayer1);
            break;
        case 7:
            EXPECT_EQ(m_rootLayer1.get(), m_layerTreeHost->hud_layer()->parent());
            endTest();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    scoped_refptr<DrawsContentLayer> m_rootLayer1;
    scoped_refptr<DrawsContentLayer> m_rootLayer2;
    int m_numCommits;
};

TEST_F(HudWithRootLayerChange, runMultiThread)
{
    runTest(true);
}

}  // namespace
}  // namespace cc
