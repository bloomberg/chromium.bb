// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/synchronization/lock.h"
#include "cc/animation/timing_function.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/scrollbar_layer.h"
#include "cc/output/output_surface.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/scheduler/frame_rate_controller.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_scrollbar_layer.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/thread_proxy.h"
#include "skia/ext/refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {
namespace {

class LayerTreeHostTest : public LayerTreeTest {};

// Test interleaving of redraws and commits
class LayerTreeHostTestCommitingWithContinuousRedraw : public LayerTreeHostTest {
public:
    LayerTreeHostTestCommitingWithContinuousRedraw()
        : m_numCompleteCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCompleteCommits++;
        if (m_numCompleteCommits == 2)
            EndTest();
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDraws == 1)
          PostSetNeedsCommitToMainThread();
        m_numDraws++;
        PostSetNeedsRedrawToMainThread();
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    int m_numCompleteCommits;
    int m_numDraws;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCommitingWithContinuousRedraw)

// Two setNeedsCommits in a row should lead to at least 1 commit and at least 1
// draw with frame 0.
class LayerTreeHostTestSetNeedsCommit1 : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetNeedsCommit1()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
        PostSetNeedsCommitToMainThread();
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDraws++;
        if (!impl->active_tree()->source_frame_number())
            EndTest();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
    }

    virtual void AfterTest() OVERRIDE
    {
        EXPECT_GE(1, m_numCommits);
        EXPECT_GE(1, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

//MULTI_THREAD_TEST_F(LayerTreeHostTestSetNeedsCommit1)

// A setNeedsCommit should lead to 1 commit. Issuing a second commit after that
// first committed frame draws should lead to another commit.
class LayerTreeHostTestSetNeedsCommit2 : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetNeedsCommit2()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        if (impl->active_tree()->source_frame_number() == 0)
            PostSetNeedsCommitToMainThread();
        else if (impl->active_tree()->source_frame_number() == 1)
            EndTest();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
    }

    virtual void AfterTest() OVERRIDE
    {
        EXPECT_EQ(2, m_numCommits);
        EXPECT_GE(2, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestSetNeedsCommit2)

// 1 setNeedsRedraw after the first commit has completed should lead to 1
// additional draw.
class LayerTreeHostTestSetNeedsRedraw : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetNeedsRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_EQ(0, impl->active_tree()->source_frame_number());
        if (!m_numDraws)
            PostSetNeedsRedrawToMainThread(); // Redraw again to verify that the second redraw doesn't commit.
        else
            EndTest();
        m_numDraws++;
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        EXPECT_EQ(0, m_numDraws);
        m_numCommits++;
    }

    virtual void AfterTest() OVERRIDE
    {
        EXPECT_GE(2, m_numDraws);
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestSetNeedsRedraw)

class LayerTreeHostTestNoExtraCommitFromInvalidate : public LayerTreeHostTest {
public:
    LayerTreeHostTestNoExtraCommitFromInvalidate()
        : m_rootLayer(ContentLayer::Create(&client_))
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        m_rootLayer->SetAutomaticallyComputeRasterScale(false);
        m_rootLayer->SetIsDrawable(true);
        m_rootLayer->SetBounds(gfx::Size(1, 1));
        layer_tree_host()->SetRootLayer(m_rootLayer);
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommit() OVERRIDE
    {
        switch (layer_tree_host()->commit_number()) {
        case 1:
            // Changing the content bounds will cause a single commit!
            m_rootLayer->SetRasterScale(4.0f);
            break;
        default:
            // No extra commits.
            EXPECT_EQ(2, layer_tree_host()->commit_number());
            EndTest();
        }
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient client_;
    scoped_refptr<ContentLayer> m_rootLayer;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestNoExtraCommitFromInvalidate)

class LayerTreeHostTestCompositeAndReadback : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadback()
        : m_numCommits(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommit() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            char pixels[4];
            layer_tree_host()->CompositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
        } else if (m_numCommits == 2) {
            // This is inside the readback. We should get another commit after it.
        } else if (m_numCommits == 3) {
            EndTest();
        } else {
          NOTREACHED();
        }
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCompositeAndReadback)

class LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws()
        : m_numCommits(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommit() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            layer_tree_host()->SetNeedsCommit();
        } else if (m_numCommits == 2) {
            char pixels[4];
            layer_tree_host()->CompositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
        } else if (m_numCommits == 3) {
            // This is inside the readback. We should get another commit after it.
        } else if (m_numCommits == 4) {
            EndTest();
        } else {
          NOTREACHED();
        }
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws)

// If the layerTreeHost says it can't draw, then we should not try to draw.
class LayerTreeHostTestCanDrawBlocksDrawing : public LayerTreeHostTest {
public:
    LayerTreeHostTestCanDrawBlocksDrawing()
        : m_numCommits(0)
        , m_done(false)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        if (m_done)
            return;
        // Only the initial draw should bring us here.
        EXPECT_TRUE(impl->CanDraw());
        EXPECT_EQ(0, impl->active_tree()->source_frame_number());
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        if (m_done)
            return;
        if (m_numCommits >= 1) {
            // After the first commit, we should not be able to draw.
            EXPECT_FALSE(impl->CanDraw());
        }
    }

    virtual void DidCommit() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            // Make the viewport empty so the host says it can't draw.
            layer_tree_host()->SetViewportSize(gfx::Size(0, 0), gfx::Size(0, 0));
        } else if (m_numCommits == 2) {
            char pixels[4];
            layer_tree_host()->CompositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
        } else if (m_numCommits == 3) {
            // Let it draw so we go idle and end the test.
            layer_tree_host()->SetViewportSize(gfx::Size(1, 1), gfx::Size(1, 1));
            m_done = true;
            EndTest();
        }
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
    bool m_done;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestCanDrawBlocksDrawing)

// beginLayerWrite should prevent draws from executing until a commit occurs
class LayerTreeHostTestWriteLayersRedraw : public LayerTreeHostTest {
public:
    LayerTreeHostTestWriteLayersRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostAcquireLayerTextures();
        PostSetNeedsRedrawToMainThread(); // should be inhibited without blocking
        PostSetNeedsCommitToMainThread();
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDraws++;
        EXPECT_EQ(m_numDraws, m_numCommits);
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
        EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestWriteLayersRedraw)

// Verify that when resuming visibility, requesting layer write permission
// will not deadlock the main thread even though there are not yet any
// scheduled redraws. This behavior is critical for reliably surviving tab
// switching. There are no failure conditions to this test, it just passes
// by not timing out.
class LayerTreeHostTestWriteLayersAfterVisible : public LayerTreeHostTest {
public:
    LayerTreeHostTestWriteLayersAfterVisible()
        : m_numCommits(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 2)
            EndTest();
        else if (m_numCommits < 2) {
            PostSetVisibleToMainThread(false);
            PostSetVisibleToMainThread(true);
            PostAcquireLayerTextures();
            PostSetNeedsCommitToMainThread();
        }
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestWriteLayersAfterVisible)

// A compositeAndReadback while invisible should force a normal commit without assertion.
class LayerTreeHostTestCompositeAndReadbackWhileInvisible : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadbackWhileInvisible()
        : m_numCommits(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommitAndDrawFrame() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            layer_tree_host()->SetVisible(false);
            layer_tree_host()->SetNeedsCommit();
            layer_tree_host()->SetNeedsCommit();
            char pixels[4];
            layer_tree_host()->CompositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
        } else
            EndTest();

    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCompositeAndReadbackWhileInvisible)

class LayerTreeHostTestAbortFrameWhenInvisible : public LayerTreeHostTest {
public:
    LayerTreeHostTestAbortFrameWhenInvisible()
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        // Request a commit (from the main thread), which will trigger the commit flow from the impl side.
        layer_tree_host()->SetNeedsCommit();
        // Then mark ourselves as not visible before processing any more messages on the main thread.
        layer_tree_host()->SetVisible(false);
        // If we make it without kicking a frame, we pass!
        EndTestAfterDelay(1);
    }

    virtual void Layout() OVERRIDE
    {
        ASSERT_FALSE(true);
        EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
};

MULTI_THREAD_TEST_F(LayerTreeHostTestAbortFrameWhenInvisible)

// This test verifies that properties on the layer tree host are commited to the impl side.
class LayerTreeHostTestCommit : public LayerTreeHostTest {
public:

    LayerTreeHostTestCommit() { }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->SetViewportSize(gfx::Size(20, 20), gfx::Size(20, 20));
        layer_tree_host()->set_background_color(SK_ColorGRAY);
        layer_tree_host()->SetPageScaleFactorAndLimits(5, 5, 5);

        PostSetNeedsCommitToMainThread();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_EQ(gfx::Size(20, 20), impl->layout_viewport_size());
        EXPECT_EQ(SK_ColorGRAY, impl->active_tree()->background_color());
        EXPECT_EQ(5, impl->active_tree()->page_scale_factor());

        EndTest();
    }

    virtual void AfterTest() OVERRIDE { }
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCommit)

// Verifies that startPageScaleAnimation events propagate correctly from LayerTreeHost to
// LayerTreeHostImpl in the MT compositor.
class LayerTreeHostTestStartPageScaleAnimation : public LayerTreeHostTest {
public:

    LayerTreeHostTestStartPageScaleAnimation()
        : m_animationRequested(false)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->root_layer()->SetScrollable(true);
        layer_tree_host()->root_layer()->SetScrollOffset(gfx::Vector2d());
        PostSetNeedsCommitToMainThread();
        PostSetNeedsRedrawToMainThread();
    }

    void requestStartPageScaleAnimation()
    {
        layer_tree_host()->StartPageScaleAnimation(gfx::Vector2d(), false, 1.25, base::TimeDelta());
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        impl->active_tree()->root_layer()->SetScrollable(true);
        impl->active_tree()->root_layer()->SetScrollOffset(gfx::Vector2d());
        impl->active_tree()->SetPageScaleFactorAndLimits(impl->active_tree()->page_scale_factor(), 0.5, 2);

        // We request animation only once.
        if (!m_animationRequested) {
            impl->proxy()->MainThread()->PostTask(base::Bind(&LayerTreeHostTestStartPageScaleAnimation::requestStartPageScaleAnimation, base::Unretained(this)));
            m_animationRequested = true;
        }
    }

    virtual void ApplyScrollAndScale(gfx::Vector2d scrollDelta, float scale) OVERRIDE
    {
        gfx::Vector2d offset = layer_tree_host()->root_layer()->scroll_offset();
        layer_tree_host()->root_layer()->SetScrollOffset(offset + scrollDelta);
        layer_tree_host()->SetPageScaleFactorAndLimits(scale, 0.5, 2);
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        impl->ProcessScrollDeltas();
        // We get one commit before the first draw, and the animation doesn't happen until the second draw.
        if (impl->active_tree()->source_frame_number() == 1) {
            EXPECT_EQ(1.25, impl->active_tree()->page_scale_factor());
            EndTest();
        } else
            PostSetNeedsRedrawToMainThread();
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    bool m_animationRequested;
};

// TODO(aelias): This test is currently broken: http://crbug.com/178295
//MULTI_THREAD_TEST_F(LayerTreeHostTestStartPageScaleAnimation)

class LayerTreeHostTestSetVisible : public LayerTreeHostTest {
public:

    LayerTreeHostTestSetVisible()
        : m_numDraws(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
        PostSetVisibleToMainThread(false);
        PostSetNeedsRedrawToMainThread(); // This is suppressed while we're invisible.
        PostSetVisibleToMainThread(true); // Triggers the redraw.
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_TRUE(impl->visible());
        ++m_numDraws;
        EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numDraws);
    }

private:
    int m_numDraws;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestSetVisible)

class TestOpacityChangeLayerDelegate : public ContentLayerClient {
public:
    TestOpacityChangeLayerDelegate()
        : m_testLayer(0)
    {
    }

    void setTestLayer(Layer* testLayer)
    {
        m_testLayer = testLayer;
    }

    virtual void PaintContents(SkCanvas*, gfx::Rect, gfx::RectF*) OVERRIDE
    {
        // Set layer opacity to 0.
        if (m_testLayer)
            m_testLayer->SetOpacity(0);
    }

private:
    Layer* m_testLayer;
};

class ContentLayerWithUpdateTracking : public ContentLayer {
public:
    static scoped_refptr<ContentLayerWithUpdateTracking> Create(ContentLayerClient* client) { return make_scoped_refptr(new ContentLayerWithUpdateTracking(client)); }

    int paintContentsCount() { return m_paintContentsCount; }
    void resetPaintContentsCount() { m_paintContentsCount = 0; }

    virtual void Update(ResourceUpdateQueue* queue, const OcclusionTracker* occlusion, RenderingStats* stats) OVERRIDE
    {
        ContentLayer::Update(queue, occlusion, stats);
        m_paintContentsCount++;
    }

private:
    explicit ContentLayerWithUpdateTracking(ContentLayerClient* client)
        : ContentLayer(client)
        , m_paintContentsCount(0)
    {
        SetAnchorPoint(gfx::PointF(0, 0));
        SetBounds(gfx::Size(10, 10));
        SetIsDrawable(true);
    }
    virtual ~ContentLayerWithUpdateTracking()
    {
    }

    int m_paintContentsCount;
};

// Layer opacity change during paint should not prevent compositor resources from being updated during commit.
class LayerTreeHostTestOpacityChange : public LayerTreeHostTest {
public:
    LayerTreeHostTestOpacityChange()
        : m_testOpacityChangeDelegate()
        , m_updateCheckLayer(ContentLayerWithUpdateTracking::Create(&m_testOpacityChangeDelegate))
    {
        m_testOpacityChangeDelegate.setTestLayer(m_updateCheckLayer.get());
    }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
        layer_tree_host()->root_layer()->AddChild(m_updateCheckLayer);

        PostSetNeedsCommitToMainThread();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
        // Update() should have been called once.
        EXPECT_EQ(1, m_updateCheckLayer->paintContentsCount());

        // clear m_updateCheckLayer so LayerTreeHost dies.
        m_updateCheckLayer = NULL;
    }

private:
    TestOpacityChangeLayerDelegate m_testOpacityChangeDelegate;
    scoped_refptr<ContentLayerWithUpdateTracking> m_updateCheckLayer;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestOpacityChange)

class NoScaleContentLayer : public ContentLayer {
public:
    static scoped_refptr<NoScaleContentLayer> Create(ContentLayerClient* client) { return make_scoped_refptr(new NoScaleContentLayer(client)); }

    virtual void CalculateContentsScale(
        float ideal_contents_scale,
        bool animating_transform_to_screen,
        float* contents_scale_x,
        float* contents_scale_y,
        gfx::Size* contentBounds) OVERRIDE
    {
        // Skip over the ContentLayer's method to the base Layer class.
        Layer::CalculateContentsScale(
             ideal_contents_scale,
             animating_transform_to_screen,
             contents_scale_x,
             contents_scale_y,
             contentBounds);
    }

private:
    explicit NoScaleContentLayer(ContentLayerClient* client)
        : ContentLayer(client) { }
    virtual ~NoScaleContentLayer() { }
};

class LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers : public LayerTreeHostTest {
public:

    LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers()
        : m_rootLayer(NoScaleContentLayer::Create(&client_))
        , m_childLayer(ContentLayer::Create(&client_))
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->SetViewportSize(gfx::Size(40, 40), gfx::Size(60, 60));
        layer_tree_host()->SetDeviceScaleFactor(1.5);
        EXPECT_EQ(gfx::Size(40, 40), layer_tree_host()->layout_viewport_size());
        EXPECT_EQ(gfx::Size(60, 60), layer_tree_host()->device_viewport_size());

        m_rootLayer->AddChild(m_childLayer);

        m_rootLayer->SetIsDrawable(true);
        m_rootLayer->SetBounds(gfx::Size(30, 30));
        m_rootLayer->SetAnchorPoint(gfx::PointF(0, 0));

        m_childLayer->SetIsDrawable(true);
        m_childLayer->SetPosition(gfx::Point(2, 2));
        m_childLayer->SetBounds(gfx::Size(10, 10));
        m_childLayer->SetAnchorPoint(gfx::PointF(0, 0));

        layer_tree_host()->SetRootLayer(m_rootLayer);

        ASSERT_TRUE(layer_tree_host()->InitializeRendererIfNeeded());
        ResourceUpdateQueue queue;
        layer_tree_host()->UpdateLayers(&queue, std::numeric_limits<size_t>::max());
        PostSetNeedsCommitToMainThread();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        // Should only do one commit.
        EXPECT_EQ(0, impl->active_tree()->source_frame_number());
        // Device scale factor should come over to impl.
        EXPECT_NEAR(impl->device_scale_factor(), 1.5, 0.00001);

        // Both layers are on impl.
        ASSERT_EQ(1u, impl->active_tree()->root_layer()->children().size());

        // Device viewport is scaled.
        EXPECT_EQ(gfx::Size(40, 40), impl->layout_viewport_size());
        EXPECT_EQ(gfx::Size(60, 60), impl->device_viewport_size());

        LayerImpl* root = impl->active_tree()->root_layer();
        LayerImpl* child = impl->active_tree()->root_layer()->children()[0];

        // Positions remain in layout pixels.
        EXPECT_EQ(gfx::Point(0, 0), root->position());
        EXPECT_EQ(gfx::Point(2, 2), child->position());

        // Compute all the layer transforms for the frame.
        LayerTreeHostImpl::FrameData frameData;
        impl->PrepareToDraw(&frameData);
        impl->DidDrawAllLayers(frameData);

        const LayerTreeHostImpl::LayerList& renderSurfaceLayerList =
          *frameData.render_surface_layer_list;

        // Both layers should be drawing into the root render surface.
        ASSERT_EQ(1u, renderSurfaceLayerList.size());
        ASSERT_EQ(root->render_surface(), renderSurfaceLayerList[0]->render_surface());
        ASSERT_EQ(2u, root->render_surface()->layer_list().size());

        // The root render surface is the size of the viewport.
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 60, 60), root->render_surface()->content_rect());

        // The content bounds of the child should be scaled.
        gfx::Size childBoundsScaled = gfx::ToCeiledSize(gfx::ScaleSize(child->bounds(), 1.5));
        EXPECT_EQ(childBoundsScaled, child->content_bounds());

        gfx::Transform scaleTransform;
        scaleTransform.Scale(impl->device_scale_factor(), impl->device_scale_factor());

        // The root layer is scaled by 2x.
        gfx::Transform rootScreenSpaceTransform = scaleTransform;
        gfx::Transform rootDrawTransform = scaleTransform;

        EXPECT_EQ(rootDrawTransform, root->draw_transform());
        EXPECT_EQ(rootScreenSpaceTransform, root->screen_space_transform());

        // The child is at position 2,2, which is transformed to 3,3 after the scale
        gfx::Transform childScreenSpaceTransform;
        childScreenSpaceTransform.Translate(3, 3);
        gfx::Transform childDrawTransform = childScreenSpaceTransform;

        EXPECT_TRANSFORMATION_MATRIX_EQ(childDrawTransform, child->draw_transform());
        EXPECT_TRANSFORMATION_MATRIX_EQ(childScreenSpaceTransform, child->screen_space_transform());

        EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
        m_rootLayer = NULL;
        m_childLayer = NULL;
    }

private:
    FakeContentLayerClient client_;
    scoped_refptr<NoScaleContentLayer> m_rootLayer;
    scoped_refptr<ContentLayer> m_childLayer;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers)

// Verify atomicity of commits and reuse of textures.
class LayerTreeHostTestAtomicCommit : public LayerTreeHostTest {
public:
    virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE
    {
        // Make sure partial texture updates are turned off.
        settings->max_partial_texture_updates = 0;
        // Linear fade animator prevents scrollbars from drawing immediately.
        settings->use_linear_fade_scrollbar_animator = false;
    }

    virtual void SetupTree() OVERRIDE
    {
        m_layer = FakeContentLayer::Create(&client_);
        m_layer->SetBounds(gfx::Size(10, 20));

        bool paint_scrollbar = true;
        bool has_thumb = false;
        m_scrollbar = FakeScrollbarLayer::Create(
            paint_scrollbar, has_thumb, m_layer->id());
        m_scrollbar->SetPosition(gfx::Point(0, 10));
        m_scrollbar->SetBounds(gfx::Size(10, 10));

        m_layer->AddChild(m_scrollbar);

        layer_tree_host()->SetRootLayer(m_layer);
        LayerTreeHostTest::SetupTree();
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        ASSERT_EQ(0u,
                  layer_tree_host()->settings().max_partial_texture_updates);

        TestWebGraphicsContext3D* context = static_cast<TestWebGraphicsContext3D*>(impl->output_surface()->context3d());

        switch (impl->active_tree()->source_frame_number()) {
        case 0:
            // Number of textures should be one for each layer
            ASSERT_EQ(2, context->NumTextures());
            // Number of textures used for commit should be one for each layer.
            EXPECT_EQ(2, context->NumUsedTextures());
            // Verify that used texture is correct.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));

            context->ResetUsedTextures();
            PostSetNeedsCommitToMainThread();
            break;
        case 1:
            // Number of textures should be doubled as the first textures
            // are used by impl thread and cannot by used for update.
            ASSERT_EQ(4, context->NumTextures());
            // Number of textures used for commit should still be one for each layer.
            EXPECT_EQ(2, context->NumUsedTextures());
            // First textures should not have been used.
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(1)));
            // New textures should have been used.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));

            context->ResetUsedTextures();
            PostSetNeedsCommitToMainThread();
            break;
        case 2:
            EndTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        TestWebGraphicsContext3D* context = static_cast<TestWebGraphicsContext3D*>(impl->output_surface()->context3d());

        // Number of textures used for draw should always be one for each layer.
        EXPECT_EQ(2, context->NumUsedTextures());
        context->ResetUsedTextures();
    }

    virtual void Layout() OVERRIDE
    {
        m_layer->SetNeedsDisplay();
        m_scrollbar->SetNeedsDisplay();
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient client_;
    scoped_refptr<FakeContentLayer> m_layer;
    scoped_refptr<FakeScrollbarLayer> m_scrollbar;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestAtomicCommit)

static void setLayerPropertiesForTesting(Layer* layer, Layer* parent, const gfx::Transform& transform, const gfx::PointF& anchor, const gfx::PointF& position, const gfx::Size& bounds, bool opaque)
{
    layer->RemoveAllChildren();
    if (parent)
        parent->AddChild(layer);
    layer->SetTransform(transform);
    layer->SetAnchorPoint(anchor);
    layer->SetPosition(position);
    layer->SetBounds(bounds);
    layer->SetContentsOpaque(opaque);
}

class LayerTreeHostTestAtomicCommitWithPartialUpdate : public LayerTreeHostTest {
public:
    LayerTreeHostTestAtomicCommitWithPartialUpdate()
        : m_numCommits(0)
    {
    }

    virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE
    {
        // Allow one partial texture update.
        settings->max_partial_texture_updates = 1;
        // Linear fade animator prevents scrollbars from drawing immediately.
        settings->use_linear_fade_scrollbar_animator = false;
    }

    virtual void SetupTree() OVERRIDE
    {
        parent_ = FakeContentLayer::Create(&client_);
        parent_->SetBounds(gfx::Size(10, 20));

        m_child = FakeContentLayer::Create(&client_);
        m_child->SetPosition(gfx::Point(0, 10));
        m_child->SetBounds(gfx::Size(3, 10));

        bool paint_scrollbar = true;
        bool has_thumb = false;
        m_scrollbarWithPaints = FakeScrollbarLayer::Create(
            paint_scrollbar, has_thumb, parent_->id());
        m_scrollbarWithPaints->SetPosition(gfx::Point(3, 10));
        m_scrollbarWithPaints->SetBounds(gfx::Size(3, 10));

        paint_scrollbar = false;
        m_scrollbarWithoutPaints = FakeScrollbarLayer::Create(
            paint_scrollbar, has_thumb, parent_->id());
        m_scrollbarWithoutPaints->SetPosition(gfx::Point(6, 10));
        m_scrollbarWithoutPaints->SetBounds(gfx::Size(3, 10));

        parent_->AddChild(m_child);
        parent_->AddChild(m_scrollbarWithPaints);
        parent_->AddChild(m_scrollbarWithoutPaints);

        layer_tree_host()->SetRootLayer(parent_);
        LayerTreeHostTest::SetupTree();
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        ASSERT_EQ(1u,
                  layer_tree_host()->settings().max_partial_texture_updates);

        TestWebGraphicsContext3D* context = static_cast<TestWebGraphicsContext3D*>(impl->output_surface()->context3d());

        switch (impl->active_tree()->source_frame_number()) {
        case 0:
            // Number of textures should be one for each layer.
            ASSERT_EQ(4, context->NumTextures());
            // Number of textures used for commit should be one for each layer.
            EXPECT_EQ(4, context->NumUsedTextures());
            // Verify that used textures are correct.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));

            context->ResetUsedTextures();
            PostSetNeedsCommitToMainThread();
            break;
        case 1:
            // Number of textures should be two for each content layer and one
            // for each scrollbar, since they always do a partial update.
            ASSERT_EQ(6, context->NumTextures());
            // Number of textures used for commit should be one for each content
            // layer, and one for the scrollbar layer that paints.
            EXPECT_EQ(3, context->NumUsedTextures());

            // First content textures should not have been used.
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(1)));
            // The non-painting scrollbar's texture wasn't updated.
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(2)));
            // The painting scrollbar's partial update texture was used.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));
            // New textures should have been used.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(4)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(5)));

            context->ResetUsedTextures();
            PostSetNeedsCommitToMainThread();
            break;
        case 2:
            // Number of textures should be two for each content layer and one
            // for each scrollbar, since they always do a partial update.
            ASSERT_EQ(6, context->NumTextures());
            // Number of textures used for commit should be one for each content
            // layer, and one for the scrollbar layer that paints.
            EXPECT_EQ(3, context->NumUsedTextures());

            // The non-painting scrollbar's texture wasn't updated.
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(2)));
            // The painting scrollbar does a partial update.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));
            // One content layer does a partial update also.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(4)));
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(5)));

            context->ResetUsedTextures();
            PostSetNeedsCommitToMainThread();
            break;
        case 3:
            // No textures should be used for commit.
            EXPECT_EQ(0, context->NumUsedTextures());

            context->ResetUsedTextures();
            PostSetNeedsCommitToMainThread();
            break;
        case 4:
            // Number of textures used for commit should be two. One for the
            // content layer, and one for the painting scrollbar. The
            // non-painting scrollbar doesn't update its texture.
            EXPECT_EQ(2, context->NumUsedTextures());

            context->ResetUsedTextures();
            PostSetNeedsCommitToMainThread();
            break;
        case 5:
            EndTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        TestWebGraphicsContext3D* context = static_cast<TestWebGraphicsContext3D*>(impl->output_surface()->context3d());

        // Number of textures used for drawing should one per layer except for
        // frame 3 where the viewport only contains one layer.
        if (impl->active_tree()->source_frame_number() == 3)
            EXPECT_EQ(1, context->NumUsedTextures());
        else
            EXPECT_EQ(4, context->NumUsedTextures());

        context->ResetUsedTextures();
    }

    virtual void Layout() OVERRIDE
    {
        switch (m_numCommits++) {
        case 0:
        case 1:
            parent_->SetNeedsDisplay();
            m_child->SetNeedsDisplay();
            m_scrollbarWithPaints->SetNeedsDisplay();
            m_scrollbarWithoutPaints->SetNeedsDisplay();
            break;
        case 2:
            // Damage part of layers.
            parent_->SetNeedsDisplayRect(gfx::RectF(0, 0, 5, 5));
            m_child->SetNeedsDisplayRect(gfx::RectF(0, 0, 5, 5));
            m_scrollbarWithPaints->SetNeedsDisplayRect(gfx::RectF(0, 0, 5, 5));
            m_scrollbarWithoutPaints->SetNeedsDisplayRect(gfx::RectF(0, 0, 5, 5));
            break;
        case 3:
            m_child->SetNeedsDisplay();
            m_scrollbarWithPaints->SetNeedsDisplay();
            m_scrollbarWithoutPaints->SetNeedsDisplay();
            layer_tree_host()->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
            break;
        case 4:
            layer_tree_host()->SetViewportSize(gfx::Size(10, 20), gfx::Size(10, 20));
            break;
        case 5:
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient client_;
    scoped_refptr<FakeContentLayer> parent_;
    scoped_refptr<FakeContentLayer> m_child;
    scoped_refptr<FakeScrollbarLayer> m_scrollbarWithPaints;
    scoped_refptr<FakeScrollbarLayer> m_scrollbarWithoutPaints;
    int m_numCommits;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestAtomicCommitWithPartialUpdate)

class LayerTreeHostTestFinishAllRendering : public LayerTreeHostTest {
public:
    LayerTreeHostTestFinishAllRendering()
        : m_once(false)
        , m_drawCount(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->SetNeedsRedraw();
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommitAndDrawFrame() OVERRIDE
    {
        if (m_once)
            return;
        m_once = true;
        layer_tree_host()->SetNeedsRedraw();
        layer_tree_host()->AcquireLayerTextures();
        {
            base::AutoLock lock(m_lock);
            m_drawCount = 0;
        }
        layer_tree_host()->FinishAllRendering();
        {
            base::AutoLock lock(m_lock);
            EXPECT_EQ(0, m_drawCount);
        }
        EndTest();
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        base::AutoLock lock(m_lock);
        ++m_drawCount;
    }

    virtual void AfterTest() OVERRIDE
    {
    }
private:

    bool m_once;
    base::Lock m_lock;
    int m_drawCount;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestFinishAllRendering)

class LayerTreeHostTestCompositeAndReadbackCleanup : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadbackCleanup() { }

    virtual void BeginTest() OVERRIDE
    {
        Layer* rootLayer = layer_tree_host()->root_layer();

        char pixels[4];
        layer_tree_host()->CompositeAndReadback(static_cast<void*>(&pixels), gfx::Rect(0, 0, 1, 1));
        EXPECT_FALSE(rootLayer->render_surface());

        EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestCompositeAndReadbackCleanup)

class LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit : public LayerTreeHostTest {
public:
    LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit()
        : m_rootLayer(ContentLayerWithUpdateTracking::Create(&m_fakeDelegate))
        , m_surfaceLayer1(ContentLayerWithUpdateTracking::Create(&m_fakeDelegate))
        , m_replicaLayer1(ContentLayerWithUpdateTracking::Create(&m_fakeDelegate))
        , m_surfaceLayer2(ContentLayerWithUpdateTracking::Create(&m_fakeDelegate))
        , m_replicaLayer2(ContentLayerWithUpdateTracking::Create(&m_fakeDelegate))
    {
    }

    virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE
    {
        settings->cache_render_pass_contents = true;
    }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

        m_rootLayer->SetBounds(gfx::Size(100, 100));
        m_surfaceLayer1->SetBounds(gfx::Size(100, 100));
        m_surfaceLayer1->SetForceRenderSurface(true);
        m_surfaceLayer1->SetOpacity(0.5);
        m_surfaceLayer2->SetBounds(gfx::Size(100, 100));
        m_surfaceLayer2->SetForceRenderSurface(true);
        m_surfaceLayer2->SetOpacity(0.5);

        m_surfaceLayer1->SetReplicaLayer(m_replicaLayer1.get());
        m_surfaceLayer2->SetReplicaLayer(m_replicaLayer2.get());

        m_rootLayer->AddChild(m_surfaceLayer1);
        m_surfaceLayer1->AddChild(m_surfaceLayer2);
        layer_tree_host()->SetRootLayer(m_rootLayer);

        PostSetNeedsCommitToMainThread();
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* hostImpl) OVERRIDE
    {
        Renderer* renderer = hostImpl->renderer();
        RenderPass::Id surface1RenderPassId = hostImpl->active_tree()->root_layer()->children()[0]->render_surface()->RenderPassId();
        RenderPass::Id surface2RenderPassId = hostImpl->active_tree()->root_layer()->children()[0]->children()[0]->render_surface()->RenderPassId();

        switch (hostImpl->active_tree()->source_frame_number()) {
        case 0:
            EXPECT_TRUE(renderer->HaveCachedResourcesForRenderPassId(surface1RenderPassId));
            EXPECT_TRUE(renderer->HaveCachedResourcesForRenderPassId(surface2RenderPassId));

            // Reduce the memory limit to only fit the root layer and one render surface. This
            // prevents any contents drawing into surfaces from being allocated.
            hostImpl->SetManagedMemoryPolicy(ManagedMemoryPolicy(100 * 100 * 4 * 2));
            break;
        case 1:
            EXPECT_FALSE(renderer->HaveCachedResourcesForRenderPassId(surface1RenderPassId));
            EXPECT_FALSE(renderer->HaveCachedResourcesForRenderPassId(surface2RenderPassId));

            EndTest();
            break;
        }
    }

    virtual void AfterTest() OVERRIDE
    {
        EXPECT_EQ(2, m_rootLayer->paintContentsCount());
        EXPECT_EQ(2, m_surfaceLayer1->paintContentsCount());
        EXPECT_EQ(2, m_surfaceLayer2->paintContentsCount());

        // Clear layer references so LayerTreeHost dies.
        m_rootLayer = NULL;
        m_surfaceLayer1 = NULL;
        m_replicaLayer1 = NULL;
        m_surfaceLayer2 = NULL;
        m_replicaLayer2 = NULL;
    }

private:
    FakeContentLayerClient m_fakeDelegate;
    scoped_refptr<ContentLayerWithUpdateTracking> m_rootLayer;
    scoped_refptr<ContentLayerWithUpdateTracking> m_surfaceLayer1;
    scoped_refptr<ContentLayerWithUpdateTracking> m_replicaLayer1;
    scoped_refptr<ContentLayerWithUpdateTracking> m_surfaceLayer2;
    scoped_refptr<ContentLayerWithUpdateTracking> m_replicaLayer2;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit)

class EvictionTestLayer : public Layer {
public:
    static scoped_refptr<EvictionTestLayer> Create() { return make_scoped_refptr(new EvictionTestLayer()); }

    virtual void Update(ResourceUpdateQueue*, const OcclusionTracker*, RenderingStats*) OVERRIDE;
    virtual bool DrawsContent() const OVERRIDE { return true; }

    virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;
    virtual void PushPropertiesTo(LayerImpl*) OVERRIDE;
    virtual void SetTexturePriorities(const PriorityCalculator&) OVERRIDE;

    bool haveBackingTexture() const { return m_texture.get() ? m_texture->have_backing_texture() : false; }

private:
    EvictionTestLayer() : Layer() { }
    virtual ~EvictionTestLayer() { }

    void createTextureIfNeeded()
    {
        if (m_texture.get())
            return;
        m_texture = PrioritizedResource::Create(layer_tree_host()->contents_texture_manager());
        m_texture->SetDimensions(gfx::Size(10, 10), GL_RGBA);
        m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    }

    scoped_ptr<PrioritizedResource> m_texture;
    SkBitmap m_bitmap;
};

class EvictionTestLayerImpl : public LayerImpl {
public:
    static scoped_ptr<EvictionTestLayerImpl> Create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new EvictionTestLayerImpl(treeImpl, id));
    }
    virtual ~EvictionTestLayerImpl() { }

    virtual void AppendQuads(QuadSink* quad_sink,
                             AppendQuadsData* append_quads_data) OVERRIDE
    {
        ASSERT_TRUE(m_hasTexture);
        ASSERT_NE(0u, layer_tree_impl()->resource_provider()->num_resources());
    }

    void setHasTexture(bool hasTexture) { m_hasTexture = hasTexture; }

private:
    EvictionTestLayerImpl(LayerTreeImpl* treeImpl, int id)
        : LayerImpl(treeImpl, id)
        , m_hasTexture(false) { }

    bool m_hasTexture;
};

void EvictionTestLayer::SetTexturePriorities(const PriorityCalculator&)
{
    createTextureIfNeeded();
    if (!m_texture.get())
        return;
    m_texture->set_request_priority(PriorityCalculator::UIPriority(true));
}

void EvictionTestLayer::Update(ResourceUpdateQueue* queue, const OcclusionTracker*, RenderingStats*)
{
    createTextureIfNeeded();
    if (!m_texture.get())
        return;

    gfx::Rect fullRect(0, 0, 10, 10);
    ResourceUpdate upload = ResourceUpdate::Create(
        m_texture.get(), &m_bitmap, fullRect, fullRect, gfx::Vector2d());
    queue->appendFullUpload(upload);
}

scoped_ptr<LayerImpl> EvictionTestLayer::CreateLayerImpl(LayerTreeImpl* treeImpl)
{
    return EvictionTestLayerImpl::Create(treeImpl, layer_id_).PassAs<LayerImpl>();
}

void EvictionTestLayer::PushPropertiesTo(LayerImpl* layerImpl)
{
    Layer::PushPropertiesTo(layerImpl);

    EvictionTestLayerImpl* testLayerImpl = static_cast<EvictionTestLayerImpl*>(layerImpl);
    testLayerImpl->setHasTexture(m_texture->have_backing_texture());
}

class LayerTreeHostTestEvictTextures : public LayerTreeHostTest {
public:
    LayerTreeHostTestEvictTextures()
        : m_layer(EvictionTestLayer::Create())
        , m_implForEvictTextures(0)
        , m_numCommits(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->SetRootLayer(m_layer);
        layer_tree_host()->SetViewportSize(gfx::Size(10, 20), gfx::Size(10, 20));

        gfx::Transform identityMatrix;
        setLayerPropertiesForTesting(m_layer.get(), 0, identityMatrix, gfx::PointF(0, 0), gfx::PointF(0, 0), gfx::Size(10, 20), true);

        PostSetNeedsCommitToMainThread();
    }

    void postEvictTextures()
    {
        DCHECK(ImplThread());
        ImplThread()->PostTask(base::Bind(&LayerTreeHostTestEvictTextures::evictTexturesOnImplThread,
                               base::Unretained(this)));
    }

    void evictTexturesOnImplThread()
    {
        DCHECK(m_implForEvictTextures);
        m_implForEvictTextures->EnforceManagedMemoryPolicy(ManagedMemoryPolicy(0));
    }

    // Commit 1: Just commit and draw normally, then post an eviction at the end
    // that will trigger a commit.
    // Commit 2: Triggered by the eviction, let it go through and then set
    // needsCommit.
    // Commit 3: Triggered by the setNeedsCommit. In Layout(), post an eviction
    // task, which will be handled before the commit. Don't set needsCommit, it
    // should have been posted. A frame should not be drawn (note,
    // didCommitAndDrawFrame may be called anyway).
    // Commit 4: Triggered by the eviction, let it go through and then set
    // needsCommit.
    // Commit 5: Triggered by the setNeedsCommit, post an eviction task in
    // Layout(), a frame should not be drawn but a commit will be posted.
    // Commit 6: Triggered by the eviction, post an eviction task in
    // Layout(), which will be a noop, letting the commit (which recreates the
    // textures) go through and draw a frame, then end the test.
    //
    // Commits 1+2 test the eviction recovery path where eviction happens outside
    // of the beginFrame/commit pair.
    // Commits 3+4 test the eviction recovery path where eviction happens inside
    // the beginFrame/commit pair.
    // Commits 5+6 test the path where an eviction happens during the eviction
    // recovery path.
    virtual void DidCommitAndDrawFrame() OVERRIDE
    {
        switch (m_numCommits) {
        case 1:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            postEvictTextures();
            break;
        case 2:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            layer_tree_host()->SetNeedsCommit();
            break;
        case 3:
            break;
        case 4:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            layer_tree_host()->SetNeedsCommit();
            break;
        case 5:
            break;
        case 6:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            EndTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_implForEvictTextures = impl;
    }

    virtual void Layout() OVERRIDE
    {
        ++m_numCommits;
        switch (m_numCommits) {
        case 1:
        case 2:
            break;
        case 3:
            postEvictTextures();
            break;
        case 4:
            // We couldn't check in didCommitAndDrawFrame on commit 3, so check here.
            EXPECT_FALSE(m_layer->haveBackingTexture());
            break;
        case 5:
            postEvictTextures();
            break;
        case 6:
            // We couldn't check in didCommitAndDrawFrame on commit 5, so check here.
            EXPECT_FALSE(m_layer->haveBackingTexture());
            postEvictTextures();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient client_;
    scoped_refptr<EvictionTestLayer> m_layer;
    LayerTreeHostImpl* m_implForEvictTextures;
    int m_numCommits;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestEvictTextures)

class LayerTreeHostTestContinuousCommit : public LayerTreeHostTest {
public:
    LayerTreeHostTestContinuousCommit()
        : m_numCommitComplete(0)
        , m_numDrawLayers(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
        layer_tree_host()->root_layer()->SetBounds(gfx::Size(10, 10));

        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommit() OVERRIDE
    {
        if (m_numDrawLayers == 2)
            return;
        PostSetNeedsCommitToMainThread();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDrawLayers == 1)
            m_numCommitComplete++;
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDrawLayers++;
        if (m_numDrawLayers == 2)
            EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
        // Check that we didn't commit twice between first and second draw.
        EXPECT_EQ(1, m_numCommitComplete);
    }

private:
    int m_numCommitComplete;
    int m_numDrawLayers;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestContinuousCommit)

class LayerTreeHostTestContinuousInvalidate : public LayerTreeHostTest {
public:
    LayerTreeHostTestContinuousInvalidate()
        : m_numCommitComplete(0)
        , m_numDrawLayers(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->SetViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
        layer_tree_host()->root_layer()->SetBounds(gfx::Size(10, 10));

        m_contentLayer = ContentLayer::Create(&m_fakeDelegate);
        m_contentLayer->SetBounds(gfx::Size(10, 10));
        m_contentLayer->SetPosition(gfx::PointF(0, 0));
        m_contentLayer->SetAnchorPoint(gfx::PointF(0, 0));
        m_contentLayer->SetIsDrawable(true);
        layer_tree_host()->root_layer()->AddChild(m_contentLayer);

        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommit() OVERRIDE
    {
        if (m_numDrawLayers == 2)
            return;
        m_contentLayer->SetNeedsDisplay();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDrawLayers == 1)
            m_numCommitComplete++;
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDrawLayers++;
        if (m_numDrawLayers == 2)
            EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
        // Check that we didn't commit twice between first and second draw.
        EXPECT_EQ(1, m_numCommitComplete);

        // Clear layer references so LayerTreeHost dies.
        m_contentLayer = NULL;
    }

private:
    FakeContentLayerClient m_fakeDelegate;
    scoped_refptr<Layer> m_contentLayer;
    int m_numCommitComplete;
    int m_numDrawLayers;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestContinuousInvalidate)

class LayerTreeHostTestDeferCommits : public LayerTreeHostTest {
public:
    LayerTreeHostTestDeferCommits()
        : m_numCommitsDeferred(0)
        , m_numCompleteCommits(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidDeferCommit() OVERRIDE
    {
        m_numCommitsDeferred++;
        layer_tree_host()->SetDeferCommits(false);
    }

    virtual void DidCommit() OVERRIDE
    {
        m_numCompleteCommits++;
        switch (m_numCompleteCommits) {
        case 1:
            EXPECT_EQ(0, m_numCommitsDeferred);
            layer_tree_host()->SetDeferCommits(true);
            PostSetNeedsCommitToMainThread();
            break;
        case 2:
            EndTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void AfterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numCommitsDeferred);
        EXPECT_EQ(2, m_numCompleteCommits);
    }

private:
    int m_numCommitsDeferred;
    int m_numCompleteCommits;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestDeferCommits)

class LayerTreeHostWithProxy : public LayerTreeHost {
public:
    LayerTreeHostWithProxy(FakeLayerTreeHostClient* client, const LayerTreeSettings& settings, scoped_ptr<Proxy> proxy)
            : LayerTreeHost(client, settings)
    {
        EXPECT_TRUE(InitializeForTesting(proxy.Pass()));
    }
};

TEST(LayerTreeHostTest, LimitPartialUpdates)
{
    // When partial updates are not allowed, max updates should be 0.
    {
        FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

        scoped_ptr<FakeProxy> proxy = make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
        proxy->GetRendererCapabilities().allow_partial_texture_updates = false;
        proxy->SetMaxPartialTextureUpdates(5);

        LayerTreeSettings settings;
        settings.max_partial_texture_updates = 10;

        LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
        EXPECT_TRUE(host.InitializeRendererIfNeeded());

        EXPECT_EQ(0u, host.settings().max_partial_texture_updates);
    }

    // When partial updates are allowed, max updates should be limited by the proxy.
    {
        FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

        scoped_ptr<FakeProxy> proxy = make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
        proxy->GetRendererCapabilities().allow_partial_texture_updates = true;
        proxy->SetMaxPartialTextureUpdates(5);

        LayerTreeSettings settings;
        settings.max_partial_texture_updates = 10;

        LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
        EXPECT_TRUE(host.InitializeRendererIfNeeded());

        EXPECT_EQ(5u, host.settings().max_partial_texture_updates);
    }

    // When partial updates are allowed, max updates should also be limited by the settings.
    {
        FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

        scoped_ptr<FakeProxy> proxy = make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
        proxy->GetRendererCapabilities().allow_partial_texture_updates = true;
        proxy->SetMaxPartialTextureUpdates(20);

        LayerTreeSettings settings;
        settings.max_partial_texture_updates = 10;

        LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
        EXPECT_TRUE(host.InitializeRendererIfNeeded());

        EXPECT_EQ(10u, host.settings().max_partial_texture_updates);
    }
}

TEST(LayerTreeHostTest, PartialUpdatesWithGLRenderer)
{
    FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_3D);

    LayerTreeSettings settings;
    settings.max_partial_texture_updates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::Create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->InitializeRendererIfNeeded());
    EXPECT_EQ(4u, host->settings().max_partial_texture_updates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithSoftwareRenderer)
{
    FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DIRECT_SOFTWARE);

    LayerTreeSettings settings;
    settings.max_partial_texture_updates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::Create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->InitializeRendererIfNeeded());
    EXPECT_EQ(4u, host->settings().max_partial_texture_updates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithDelegatingRendererAndGLContent)
{
    FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DELEGATED_3D);

    LayerTreeSettings settings;
    settings.max_partial_texture_updates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::Create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->InitializeRendererIfNeeded());
    EXPECT_EQ(0u, host->settings().max_partial_texture_updates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithDelegatingRendererAndSoftwareContent)
{
    FakeLayerTreeHostClient client(FakeLayerTreeHostClient::DELEGATED_SOFTWARE);

    LayerTreeSettings settings;
    settings.max_partial_texture_updates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::Create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->InitializeRendererIfNeeded());
    EXPECT_EQ(0u, host->settings().max_partial_texture_updates);
}

class LayerTreeHostTestCapturePicture : public LayerTreeHostTest {
public:
    LayerTreeHostTestCapturePicture()
        : bounds_(gfx::Size(100, 100))
        , m_layer(PictureLayer::Create(&m_contentClient))
    {
    }

    virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE
    {
        settings->impl_side_painting = true;
    }

    class FillRectContentLayerClient : public ContentLayerClient {
    public:
        virtual void PaintContents(SkCanvas* canvas, gfx::Rect clip, gfx::RectF* opaque) OVERRIDE
        {
            SkPaint paint;
            paint.setColor(SK_ColorGREEN);

            SkRect rect = SkRect::MakeWH(canvas->getDeviceSize().width(), canvas->getDeviceSize().height());
            *opaque = gfx::RectF(rect.width(), rect.height());
            canvas->drawRect(rect, paint);
        }
    };

    virtual void BeginTest() OVERRIDE
    {
        m_layer->SetIsDrawable(true);
        m_layer->SetBounds(bounds_);
        layer_tree_host()->SetViewportSize(bounds_, bounds_);
        layer_tree_host()->SetRootLayer(m_layer);

        EXPECT_TRUE(layer_tree_host()->InitializeRendererIfNeeded());
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommitAndDrawFrame() OVERRIDE
    {
        m_picture = layer_tree_host()->CapturePicture();
        EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
        EXPECT_EQ(bounds_, gfx::Size(m_picture->width(), m_picture->height()));

        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, bounds_.width(), bounds_.height());
        bitmap.allocPixels();
        bitmap.eraseARGB(0, 0, 0, 0);
        SkCanvas canvas(bitmap);

        m_picture->draw(&canvas);

        bitmap.lockPixels();
        SkColor* pixels = reinterpret_cast<SkColor*>(bitmap.getPixels());
        EXPECT_EQ(SK_ColorGREEN, pixels[0]);
        bitmap.unlockPixels();
    }

private:
    gfx::Size bounds_;
    FillRectContentLayerClient m_contentClient;
    scoped_refptr<PictureLayer> m_layer;
    skia::RefPtr<SkPicture> m_picture;
};

MULTI_THREAD_TEST_F(LayerTreeHostTestCapturePicture);

class LayerTreeHostTestMaxPendingFrames : public LayerTreeHostTest {
public:
    LayerTreeHostTestMaxPendingFrames()
        : LayerTreeHostTest()
    {
    }

    virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE
    {
        if (m_delegatingRenderer)
            return FakeOutputSurface::CreateDelegating3d().PassAs<OutputSurface>();
        return FakeOutputSurface::Create3d().PassAs<OutputSurface>();
    }

    virtual void BeginTest() OVERRIDE
    {
        PostSetNeedsCommitToMainThread();
    }

    virtual void DrawLayersOnThread(LayerTreeHostImpl* hostImpl) OVERRIDE
    {
        DCHECK(hostImpl->proxy()->HasImplThread());

        const ThreadProxy* proxy = static_cast<ThreadProxy*>(hostImpl->proxy());
        if (m_delegatingRenderer)
            EXPECT_EQ(1, proxy->MaxFramesPendingForTesting());
        else
            EXPECT_EQ(FrameRateController::DEFAULT_MAX_FRAMES_PENDING, proxy->MaxFramesPendingForTesting());
        EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
    }

protected:
    bool m_delegatingRenderer;
};

TEST_F(LayerTreeHostTestMaxPendingFrames, DelegatingRenderer)
{
    m_delegatingRenderer = true;
    RunTest(true);
}

TEST_F(LayerTreeHostTestMaxPendingFrames, GLRenderer)
{
    m_delegatingRenderer = false;
    RunTest(true);
}

class LayerTreeHostTestShutdownWithOnlySomeResourcesEvicted : public LayerTreeHostTest {
public:
    LayerTreeHostTestShutdownWithOnlySomeResourcesEvicted()
        : m_rootLayer(FakeContentLayer::Create(&client_))
        , m_childLayer1(FakeContentLayer::Create(&client_))
        , m_childLayer2(FakeContentLayer::Create(&client_))
        , m_numCommits(0)
    {
    }

    virtual void BeginTest() OVERRIDE
    {
        layer_tree_host()->SetViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));
        m_rootLayer->SetBounds(gfx::Size(100, 100));
        m_childLayer1->SetBounds(gfx::Size(100, 100));
        m_childLayer2->SetBounds(gfx::Size(100, 100));
        m_rootLayer->AddChild(m_childLayer1);
        m_rootLayer->AddChild(m_childLayer2);
        layer_tree_host()->SetRootLayer(m_rootLayer);
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidSetVisibleOnImplTree(LayerTreeHostImpl* hostImpl, bool visible) OVERRIDE
    {
        // One backing should remain unevicted.
        EXPECT_EQ(
            100 * 100 * 4 * 1, 
            layer_tree_host()->contents_texture_manager()->MemoryUseBytes());
        // Make sure that contents textures are marked as having been
        // purged.
        EXPECT_TRUE(hostImpl->active_tree()->ContentsTexturesPurged());
        // End the test in this state.
        EndTest();
    }

    virtual void CommitCompleteOnThread(LayerTreeHostImpl* hostImpl) OVERRIDE
    {
        ++m_numCommits;
        switch(m_numCommits) {
        case 1:
            // All three backings should have memory.
            EXPECT_EQ(
                100 * 100 * 4 * 3, 
                layer_tree_host()->contents_texture_manager()->MemoryUseBytes());
            // Set a new policy that will kick out 1 of the 3 resources.
            // Because a resource was evicted, a commit will be kicked off.
            hostImpl->SetManagedMemoryPolicy(ManagedMemoryPolicy(
                100 * 100 * 4 * 2,
                ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING,
                100 * 100 * 4 * 1,
                ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING));
            break;
        case 2:
            // Only two backings should have memory.
            EXPECT_EQ(
                100 * 100 * 4 * 2, 
                layer_tree_host()->contents_texture_manager()->MemoryUseBytes());
            // Become backgrounded, which will cause 1 more resource to be
            // evicted.
            PostSetVisibleToMainThread(false);
            break;
        default:
            // No further commits should happen because this is not visible
            // anymore.
            NOTREACHED();
            break;
        }
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient client_;
    scoped_refptr<FakeContentLayer> m_rootLayer;
    scoped_refptr<FakeContentLayer> m_childLayer1;
    scoped_refptr<FakeContentLayer> m_childLayer2;
    int m_numCommits;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestShutdownWithOnlySomeResourcesEvicted)

class LayerTreeHostTestPinchZoomScrollbarCreation : public LayerTreeHostTest {
public:
    LayerTreeHostTestPinchZoomScrollbarCreation()
        : m_rootLayer(ContentLayer::Create(&client_))
    {
    }

    virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE
    {
        settings->use_pinch_zoom_scrollbars = true;
    }

    virtual void BeginTest() OVERRIDE
    {
        m_rootLayer->SetIsDrawable(true);
        m_rootLayer->SetBounds(gfx::Size(100, 100));
        layer_tree_host()->SetRootLayer(m_rootLayer);
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommit() OVERRIDE
    {
        // We always expect two pinch-zoom scrollbar layers.
        ASSERT_TRUE(2 == m_rootLayer->children().size());

        // Pinch-zoom scrollbar layers always have invalid scrollLayerIds.
        ScrollbarLayer* layer1 = m_rootLayer->children()[0]->ToScrollbarLayer();
        ASSERT_TRUE(layer1);
        EXPECT_EQ(Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID, layer1->scroll_layer_id());
        EXPECT_EQ(0, layer1->opacity());
        EXPECT_TRUE(layer1->OpacityCanAnimateOnImplThread());
        EXPECT_TRUE(layer1->DrawsContent());

        ScrollbarLayer* layer2 = m_rootLayer->children()[1]->ToScrollbarLayer();
        ASSERT_TRUE(layer2);
        EXPECT_EQ(Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID, layer2->scroll_layer_id());
        EXPECT_EQ(0, layer2->opacity());
        EXPECT_TRUE(layer2->OpacityCanAnimateOnImplThread());
        EXPECT_TRUE(layer2->DrawsContent());

        EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient client_;
    scoped_refptr<ContentLayer> m_rootLayer;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestPinchZoomScrollbarCreation)

class LayerTreeHostTestPinchZoomScrollbarResize : public LayerTreeHostTest {
public:
    LayerTreeHostTestPinchZoomScrollbarResize()
        : m_rootLayer(ContentLayer::Create(&client_))
        , m_numCommits(0)
    {
    }

    virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE
    {
        settings->use_pinch_zoom_scrollbars = true;
    }

    virtual void BeginTest() OVERRIDE
    {
        m_rootLayer->SetIsDrawable(true);
        m_rootLayer->SetBounds(gfx::Size(100, 100));
        layer_tree_host()->SetRootLayer(m_rootLayer);
        layer_tree_host()->SetViewportSize(gfx::Size(100, 100),
            gfx::Size(100, 100));
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommit() OVERRIDE
    {
        m_numCommits++;

        ScrollbarLayer* layer1 = m_rootLayer->children()[0]->ToScrollbarLayer();
        ASSERT_TRUE(layer1);
        ScrollbarLayer* layer2 = m_rootLayer->children()[1]->ToScrollbarLayer();
        ASSERT_TRUE(layer2);

        // Get scrollbar thickness from horizontal scrollbar's height.
        int thickness = layer1->bounds().height();

        if (!layer1->Orientation() == WebKit::WebScrollbar::Horizontal)
          std::swap(layer1, layer2);

        gfx::Size viewportSize = layer_tree_host()->layout_viewport_size();
        EXPECT_EQ(viewportSize.width() - thickness, layer1->bounds().width());
        EXPECT_EQ(viewportSize.height() - thickness, layer2->bounds().height());

        switch (m_numCommits) {
          case 1:
          // Resizing the viewport should also resize the pinch-zoom scrollbars.
          layer_tree_host()->SetViewportSize(gfx::Size(120, 150),
              gfx::Size(120, 150));
          break;
        default:
          EndTest();
        }
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient client_;
    scoped_refptr<ContentLayer> m_rootLayer;
    int m_numCommits;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestPinchZoomScrollbarResize)

class LayerTreeHostTestPinchZoomScrollbarNewRootLayer : public LayerTreeHostTest {
public:
    LayerTreeHostTestPinchZoomScrollbarNewRootLayer()
        : m_rootLayer(ContentLayer::Create(&client_))
        , m_numCommits(0)
    {
    }

    virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE
    {
        settings->use_pinch_zoom_scrollbars = true;
    }

    virtual void BeginTest() OVERRIDE
    {
        m_rootLayer->SetIsDrawable(true);
        m_rootLayer->SetBounds(gfx::Size(100, 100));
        layer_tree_host()->SetRootLayer(m_rootLayer);
        PostSetNeedsCommitToMainThread();
    }

    virtual void DidCommit() OVERRIDE
    {
        m_numCommits++;

        // We always expect two pinch-zoom scrollbar layers.
        ASSERT_TRUE(2 == m_rootLayer->children().size());

        // Pinch-zoom scrollbar layers always have invalid scrollLayerIds.
        ScrollbarLayer* layer1 = m_rootLayer->children()[0]->ToScrollbarLayer();
        ASSERT_TRUE(layer1);
        EXPECT_EQ(Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID, layer1->scroll_layer_id());
        EXPECT_EQ(0, layer1->opacity());
        EXPECT_TRUE(layer1->DrawsContent());

        ScrollbarLayer* layer2 = m_rootLayer->children()[1]->ToScrollbarLayer();
        ASSERT_TRUE(layer2);
        EXPECT_EQ(Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID, layer2->scroll_layer_id());
        EXPECT_EQ(0, layer2->opacity());
        EXPECT_TRUE(layer2->DrawsContent());

        if (m_numCommits == 1) {
            // Create a new root layer and attach to tree to verify the pinch
            // zoom scrollbars get correctly re-attached.
            m_rootLayer = ContentLayer::Create(&client_);
            m_rootLayer->SetIsDrawable(true);
            m_rootLayer->SetBounds(gfx::Size(100, 100));
            layer_tree_host()->SetRootLayer(m_rootLayer);
            PostSetNeedsCommitToMainThread();
        } else
            EndTest();
    }

    virtual void AfterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient client_;
    scoped_refptr<ContentLayer> m_rootLayer;
    int m_numCommits;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestPinchZoomScrollbarNewRootLayer)

}  // namespace
}  // namespace cc
