// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {
namespace {

class HeadsUpDisplayTest : public LayerTreeTest {
protected:
    virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE
    {
        // Enable the HUD without requiring text.
        settings->initialDebugState.show_property_changed_rects = true;
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

    virtual void BeginTest() OVERRIDE
    {
        m_rootLayer1->SetBounds(gfx::Size(30, 30));
        m_rootLayer2->SetBounds(gfx::Size(30, 30));

        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommit() OVERRIDE
    {
        ++m_numCommits;

        ASSERT_TRUE(layer_tree_host()->hud_layer());

        switch (m_numCommits) {
        case 1:
            // Change directly to a new root layer.
            layer_tree_host()->SetRootLayer(m_rootLayer1);
            break;
        case 2:
            EXPECT_EQ(m_rootLayer1.get(), layer_tree_host()->hud_layer()->parent());
            // Unset the root layer.
            layer_tree_host()->SetRootLayer(NULL);
            break;
        case 3:
            EXPECT_EQ(0, layer_tree_host()->hud_layer()->parent());
            // Change back to the previous root layer.
            layer_tree_host()->SetRootLayer(m_rootLayer1);
            break;
        case 4:
            EXPECT_EQ(m_rootLayer1.get(), layer_tree_host()->hud_layer()->parent());
            // Unset the root layer.
            layer_tree_host()->SetRootLayer(NULL);
            break;
        case 5:
            EXPECT_EQ(0, layer_tree_host()->hud_layer()->parent());
            // Change to a new root layer from a null root.
            layer_tree_host()->SetRootLayer(m_rootLayer2);
            break;
        case 6:
            EXPECT_EQ(m_rootLayer2.get(), layer_tree_host()->hud_layer()->parent());
            // Change directly back to the last root layer/
            layer_tree_host()->SetRootLayer(m_rootLayer1);
            break;
        case 7:
            EXPECT_EQ(m_rootLayer1.get(), layer_tree_host()->hud_layer()->parent());
            EndTest();
            break;
        }
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    scoped_refptr<DrawsContentLayer> m_rootLayer1;
    scoped_refptr<DrawsContentLayer> m_rootLayer2;
    int m_numCommits;
};

MULTI_THREAD_TEST_F(HudWithRootLayerChange)

}  // namespace
}  // namespace cc
