// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/occlusion_tracker.h"

#include "cc/animation/layer_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/debug/overdraw_metrics.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class TestContentLayer : public Layer {
public:
    TestContentLayer()
        : Layer()
        , m_overrideOpaqueContentsRect(false)
    {
    }

    virtual bool DrawsContent() const OVERRIDE { return true; }
    virtual Region VisibleContentOpaqueRegion() const OVERRIDE
    {
        if (m_overrideOpaqueContentsRect)
            return gfx::IntersectRects(m_opaqueContentsRect, visible_content_rect());
        return Layer::VisibleContentOpaqueRegion();
    }
    void setOpaqueContentsRect(const gfx::Rect& opaqueContentsRect)
    {
        m_overrideOpaqueContentsRect = true;
        m_opaqueContentsRect = opaqueContentsRect;
    }

private:
    virtual ~TestContentLayer()
    {
    }

    bool m_overrideOpaqueContentsRect;
    gfx::Rect m_opaqueContentsRect;
};

class TestContentLayerImpl : public LayerImpl {
public:
    TestContentLayerImpl(LayerTreeImpl* treeImpl, int id)
        : LayerImpl(treeImpl, id)
        , m_overrideOpaqueContentsRect(false)
    {
        SetDrawsContent(true);
    }

    virtual Region VisibleContentOpaqueRegion() const OVERRIDE
    {
        if (m_overrideOpaqueContentsRect)
            return gfx::IntersectRects(m_opaqueContentsRect, visible_content_rect());
        return LayerImpl::VisibleContentOpaqueRegion();
    }
    void setOpaqueContentsRect(const gfx::Rect& opaqueContentsRect)
    {
        m_overrideOpaqueContentsRect = true;
        m_opaqueContentsRect = opaqueContentsRect;
    }

private:
    bool m_overrideOpaqueContentsRect;
    gfx::Rect m_opaqueContentsRect;
};

static inline bool layerImplDrawTransformIsUnknown(const Layer* layer) { return layer->draw_transform_is_animating(); }
static inline bool layerImplDrawTransformIsUnknown(const LayerImpl*) { return false; }

template<typename LayerType, typename RenderSurfaceType>
class TestOcclusionTrackerWithClip : public TestOcclusionTrackerBase<LayerType, RenderSurfaceType> {
public:
    TestOcclusionTrackerWithClip(gfx::Rect viewportRect, bool recordMetricsForFrame = false)
        : TestOcclusionTrackerBase<LayerType, RenderSurfaceType>(viewportRect, recordMetricsForFrame)
    {
    }

    bool occludedLayer(const LayerType* layer, const gfx::Rect& contentRect, bool* hasOcclusionFromOutsideTargetSurface = 0) const
    {
        return this->Occluded(layer->render_target(), contentRect, layer->draw_transform(), layerImplDrawTransformIsUnknown(layer), layer->is_clipped(), layer->clip_rect(), hasOcclusionFromOutsideTargetSurface);
    }
    // Gives an unoccluded sub-rect of |contentRect| in the content space of the layer. Simple wrapper around unoccludedContentRect.
    gfx::Rect unoccludedLayerContentRect(const LayerType* layer, const gfx::Rect& contentRect, bool* hasOcclusionFromOutsideTargetSurface = 0) const
    {
        return this->UnoccludedContentRect(layer->render_target(), contentRect, layer->draw_transform(), layerImplDrawTransformIsUnknown(layer), layer->is_clipped(), layer->clip_rect(), hasOcclusionFromOutsideTargetSurface);
    }
};

struct OcclusionTrackerTestMainThreadTypes {
    typedef Layer LayerType;
    typedef LayerTreeHost HostType;
    typedef RenderSurface RenderSurfaceType;
    typedef TestContentLayer ContentLayerType;
    typedef scoped_refptr<Layer> LayerPtrType;
    typedef scoped_refptr<ContentLayerType> ContentLayerPtrType;
    typedef LayerIterator<Layer, std::vector<scoped_refptr<Layer> >, RenderSurface, LayerIteratorActions::FrontToBack> TestLayerIterator;
    typedef OcclusionTracker OcclusionTrackerType;

    static LayerPtrType createLayer(HostType*)
    {
        return Layer::Create();
    }
    static ContentLayerPtrType createContentLayer(HostType*) { return make_scoped_refptr(new ContentLayerType()); }

    static LayerPtrType passLayerPtr(ContentLayerPtrType& layer)
    {
        LayerPtrType ref(layer);
        layer = NULL;
        return ref;
    }

    static LayerPtrType passLayerPtr(LayerPtrType& layer)
    {
        LayerPtrType ref(layer);
        layer = NULL;
        return ref;
    }

    static void destroyLayer(LayerPtrType& layer)
    {
        layer = NULL;
    }
};

struct OcclusionTrackerTestImplThreadTypes {
    typedef LayerImpl LayerType;
    typedef LayerTreeImpl HostType;
    typedef RenderSurfaceImpl RenderSurfaceType;
    typedef TestContentLayerImpl ContentLayerType;
    typedef scoped_ptr<LayerImpl> LayerPtrType;
    typedef scoped_ptr<ContentLayerType> ContentLayerPtrType;
    typedef LayerIterator<LayerImpl, std::vector<LayerImpl*>, RenderSurfaceImpl, LayerIteratorActions::FrontToBack> TestLayerIterator;
    typedef OcclusionTrackerImpl OcclusionTrackerType;

    static LayerPtrType createLayer(HostType* host) { return LayerImpl::Create(host, nextLayerImplId++); }
    static ContentLayerPtrType createContentLayer(HostType* host) { return make_scoped_ptr(new ContentLayerType(host, nextLayerImplId++)); }
    static int nextLayerImplId;

    static LayerPtrType passLayerPtr(LayerPtrType& layer)
    {
        return layer.Pass();
    }

    static LayerPtrType passLayerPtr(ContentLayerPtrType& layer)
    {
        return layer.PassAs<LayerType>();
    }

    static void destroyLayer(LayerPtrType& layer)
    {
        layer.reset();
    }
};

int OcclusionTrackerTestImplThreadTypes::nextLayerImplId = 1;

template<typename Types>
class OcclusionTrackerTest : public testing::Test {
protected:
    OcclusionTrackerTest(bool opaqueLayers)
        : m_hostImpl(&m_proxy)
        , m_opaqueLayers(opaqueLayers)
    {
    }

    virtual void runMyTest() = 0;

    virtual void TearDown()
    {
        Types::destroyLayer(m_root);
        m_renderSurfaceLayerList.clear();
        m_renderSurfaceLayerListImpl.clear();
        m_replicaLayers.clear();
        m_maskLayers.clear();
    }

    typename Types::HostType* getHost();

    typename Types::ContentLayerType* createRoot(const gfx::Transform& transform, const gfx::PointF& position, const gfx::Size& bounds)
    {
        typename Types::ContentLayerPtrType layer(Types::createContentLayer(getHost()));
        typename Types::ContentLayerType* layerPtr = layer.get();
        setProperties(layerPtr, transform, position, bounds);

        DCHECK(!m_root);
        m_root = Types::passLayerPtr(layer);
        return layerPtr;
    }

    typename Types::LayerType* createLayer(typename Types::LayerType* parent, const gfx::Transform& transform, const gfx::PointF& position, const gfx::Size& bounds)
    {
        typename Types::LayerPtrType layer(Types::createLayer(getHost()));
        typename Types::LayerType* layerPtr = layer.get();
        setProperties(layerPtr, transform, position, bounds);
        parent->AddChild(Types::passLayerPtr(layer));
        return layerPtr;
    }

    typename Types::LayerType* createSurface(typename Types::LayerType* parent, const gfx::Transform& transform, const gfx::PointF& position, const gfx::Size& bounds)
    {
        typename Types::LayerType* layer = createLayer(parent, transform, position, bounds);
        WebKit::WebFilterOperations filters;
        filters.append(WebKit::WebFilterOperation::createGrayscaleFilter(0.5));
        layer->SetFilters(filters);
        return layer;
    }

    typename Types::ContentLayerType* createDrawingLayer(typename Types::LayerType* parent, const gfx::Transform& transform, const gfx::PointF& position, const gfx::Size& bounds, bool opaque)
    {
        typename Types::ContentLayerPtrType layer(Types::createContentLayer(getHost()));
        typename Types::ContentLayerType* layerPtr = layer.get();
        setProperties(layerPtr, transform, position, bounds);

        if (m_opaqueLayers)
            layerPtr->SetContentsOpaque(opaque);
        else {
            layerPtr->SetContentsOpaque(false);
            if (opaque)
                layerPtr->setOpaqueContentsRect(gfx::Rect(gfx::Point(), bounds));
            else
                layerPtr->setOpaqueContentsRect(gfx::Rect());
        }

        parent->AddChild(Types::passLayerPtr(layer));
        return layerPtr;
    }

    typename Types::LayerType* createReplicaLayer(typename Types::LayerType* owningLayer, const gfx::Transform& transform, const gfx::PointF& position, const gfx::Size& bounds)
    {
        typename Types::ContentLayerPtrType layer(Types::createContentLayer(getHost()));
        typename Types::ContentLayerType* layerPtr = layer.get();
        setProperties(layerPtr, transform, position, bounds);
        setReplica(owningLayer, Types::passLayerPtr(layer));
        return layerPtr;
    }

    typename Types::LayerType* createMaskLayer(typename Types::LayerType* owningLayer, const gfx::Size& bounds)
    {
        typename Types::ContentLayerPtrType layer(Types::createContentLayer(getHost()));
        typename Types::ContentLayerType* layerPtr = layer.get();
        setProperties(layerPtr, identityMatrix, gfx::PointF(), bounds);
        setMask(owningLayer, Types::passLayerPtr(layer));
        return layerPtr;
    }

    typename Types::ContentLayerType* createDrawingSurface(typename Types::LayerType* parent, const gfx::Transform& transform, const gfx::PointF& position, const gfx::Size& bounds, bool opaque)
    {
        typename Types::ContentLayerType* layer = createDrawingLayer(parent, transform, position, bounds, opaque);
        WebKit::WebFilterOperations filters;
        filters.append(WebKit::WebFilterOperation::createGrayscaleFilter(0.5));
        layer->SetFilters(filters);
        return layer;
    }

    void calcDrawEtc(TestContentLayerImpl* root)
    {
        DCHECK(root == m_root.get());
        int dummyMaxTextureSize = 512;

        DCHECK(!root->render_surface());

        LayerTreeHostCommon::calculateDrawProperties(root, root->bounds(), 1, 1, dummyMaxTextureSize, false, m_renderSurfaceLayerListImpl, false);

        m_layerIterator = m_layerIteratorBegin = Types::TestLayerIterator::Begin(&m_renderSurfaceLayerListImpl);
    }

    void calcDrawEtc(TestContentLayer* root)
    {
        DCHECK(root == m_root.get());
        int dummyMaxTextureSize = 512;

        DCHECK(!root->render_surface());

        LayerTreeHostCommon::calculateDrawProperties(root, root->bounds(), 1, 1, dummyMaxTextureSize, false, m_renderSurfaceLayerList);

        m_layerIterator = m_layerIteratorBegin = Types::TestLayerIterator::Begin(&m_renderSurfaceLayerList);
    }

    void EnterLayer(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        ASSERT_EQ(layer, *m_layerIterator);
        ASSERT_TRUE(m_layerIterator.represents_itself());
        occlusion.EnterLayer(m_layerIterator);
    }

    void LeaveLayer(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        ASSERT_EQ(layer, *m_layerIterator);
        ASSERT_TRUE(m_layerIterator.represents_itself());
        occlusion.LeaveLayer(m_layerIterator);
        ++m_layerIterator;
    }

    void visitLayer(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        EnterLayer(layer, occlusion);
        LeaveLayer(layer, occlusion);
    }

    void enterContributingSurface(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        ASSERT_EQ(layer, *m_layerIterator);
        ASSERT_TRUE(m_layerIterator.represents_target_render_surface());
        occlusion.EnterLayer(m_layerIterator);
        occlusion.LeaveLayer(m_layerIterator);
        ++m_layerIterator;
        ASSERT_TRUE(m_layerIterator.represents_contributing_render_surface());
        occlusion.EnterLayer(m_layerIterator);
    }

    void leaveContributingSurface(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        ASSERT_EQ(layer, *m_layerIterator);
        ASSERT_TRUE(m_layerIterator.represents_contributing_render_surface());
        occlusion.LeaveLayer(m_layerIterator);
        ++m_layerIterator;
    }

    void visitContributingSurface(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        enterContributingSurface(layer, occlusion);
        leaveContributingSurface(layer, occlusion);
    }

    void resetLayerIterator()
    {
        m_layerIterator = m_layerIteratorBegin;
    }

    const gfx::Transform identityMatrix;

private:
    void setBaseProperties(typename Types::LayerType* layer, const gfx::Transform& transform, const gfx::PointF& position, const gfx::Size& bounds)
    {
        layer->SetTransform(transform);
        layer->SetSublayerTransform(gfx::Transform());
        layer->SetAnchorPoint(gfx::PointF(0, 0));
        layer->SetPosition(position);
        layer->SetBounds(bounds);
    }

    void setProperties(Layer* layer, const gfx::Transform& transform, const gfx::PointF& position, const gfx::Size& bounds)
    {
        setBaseProperties(layer, transform, position, bounds);
    }

    void setProperties(LayerImpl* layer, const gfx::Transform& transform, const gfx::PointF& position, const gfx::Size& bounds)
    {
        setBaseProperties(layer, transform, position, bounds);

        layer->SetContentBounds(layer->bounds());
    }

    void setReplica(Layer* owningLayer, scoped_refptr<Layer> layer)
    {
        owningLayer->SetReplicaLayer(layer.get());
        m_replicaLayers.push_back(layer);
    }

    void setReplica(LayerImpl* owningLayer, scoped_ptr<LayerImpl> layer)
    {
        owningLayer->SetReplicaLayer(layer.Pass());
    }

    void setMask(Layer* owningLayer, scoped_refptr<Layer> layer)
    {
        owningLayer->SetMaskLayer(layer.get());
        m_maskLayers.push_back(layer);
    }

    void setMask(LayerImpl* owningLayer, scoped_ptr<LayerImpl> layer)
    {
        owningLayer->SetMaskLayer(layer.Pass());
    }

    FakeImplProxy m_proxy;
    FakeLayerTreeHostImpl m_hostImpl;
    bool m_opaqueLayers;
    // These hold ownership of the layers for the duration of the test.
    typename Types::LayerPtrType m_root;
    std::vector<scoped_refptr<Layer> > m_renderSurfaceLayerList;
    std::vector<LayerImpl*> m_renderSurfaceLayerListImpl;
    typename Types::TestLayerIterator m_layerIteratorBegin;
    typename Types::TestLayerIterator m_layerIterator;
    typename Types::LayerType* m_lastLayerVisited;
    std::vector<scoped_refptr<Layer> > m_replicaLayers;
    std::vector<scoped_refptr<Layer> > m_maskLayers;
};

template<>
LayerTreeHost* OcclusionTrackerTest<OcclusionTrackerTestMainThreadTypes>::getHost()
{
    return NULL;
}

template<>
LayerTreeImpl* OcclusionTrackerTest<OcclusionTrackerTestImplThreadTypes>::getHost()
{
  return m_hostImpl.active_tree();
}

#define RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName) \
    class ClassName##MainThreadOpaqueLayers : public ClassName<OcclusionTrackerTestMainThreadTypes> { \
    public: \
        ClassName##MainThreadOpaqueLayers() : ClassName<OcclusionTrackerTestMainThreadTypes>(true) { } \
    }; \
    TEST_F(ClassName##MainThreadOpaqueLayers, runTest) { runMyTest(); }
#define RUN_TEST_MAIN_THREAD_OPAQUE_PAINTS(ClassName) \
    class ClassName##MainThreadOpaquePaints : public ClassName<OcclusionTrackerTestMainThreadTypes> { \
    public: \
        ClassName##MainThreadOpaquePaints() : ClassName<OcclusionTrackerTestMainThreadTypes>(false) { } \
    }; \
    TEST_F(ClassName##MainThreadOpaquePaints, runTest) { runMyTest(); }

#define RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName) \
    class ClassName##ImplThreadOpaqueLayers : public ClassName<OcclusionTrackerTestImplThreadTypes> { \
    public: \
        ClassName##ImplThreadOpaqueLayers() : ClassName<OcclusionTrackerTestImplThreadTypes>(true) { } \
    }; \
    TEST_F(ClassName##ImplThreadOpaqueLayers, runTest) { runMyTest(); }
#define RUN_TEST_IMPL_THREAD_OPAQUE_PAINTS(ClassName) \
    class ClassName##ImplThreadOpaquePaints : public ClassName<OcclusionTrackerTestImplThreadTypes> { \
    public: \
        ClassName##ImplThreadOpaquePaints() : ClassName<OcclusionTrackerTestImplThreadTypes>(false) { } \
    }; \
    TEST_F(ClassName##ImplThreadOpaquePaints, runTest) { runMyTest(); }

#define ALL_OCCLUSIONTRACKER_TEST(ClassName) \
    RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName) \
    RUN_TEST_MAIN_THREAD_OPAQUE_PAINTS(ClassName) \
    RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName) \
    RUN_TEST_IMPL_THREAD_OPAQUE_PAINTS(ClassName)

#define MAIN_THREAD_TEST(ClassName) \
    RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName)

#define IMPL_THREAD_TEST(ClassName) \
    RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)

#define MAIN_AND_IMPL_THREAD_TEST(ClassName) \
    RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName) \
    RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)

template<class Types>
class OcclusionTrackerTestIdentityTransforms : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestIdentityTransforms(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}

    void runMyTest()
    {
        typename Types::ContentLayerType* root = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200));
        typename Types::ContentLayerType* parent = this->createDrawingLayer(root, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(30, 30), gfx::Size(500, 500), true);
        parent->SetMasksToBounds(true);
        this->calcDrawEtc(root);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(layer, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(29, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(30, 29, 70, 70)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(31, 30, 70, 70)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 31, 70, 70)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 30, 70, 70)).IsEmpty());
        EXPECT_RECT_EQ(gfx::Rect(29, 30, 1, 70), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(29, 30, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(29, 29, 70, 70), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(29, 29, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(30, 29, 70, 1), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 29, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(31, 29, 69, 1), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(31, 29, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(31, 30, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(31, 31, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 31, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(29, 31, 1, 69), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(29, 31, 70, 70)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestIdentityTransforms);

template<class Types>
class OcclusionTrackerTestQuadsMismatchLayer : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestQuadsMismatchLayer(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform layerTransform;
        layerTransform.Translate(10, 10);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::Point(0, 0), gfx::Size(100, 100));
        typename Types::ContentLayerType* layer1 = this->createDrawingLayer(parent, layerTransform, gfx::PointF(0, 0), gfx::Size(90, 90), true);
        typename Types::ContentLayerType* layer2 = this->createDrawingLayer(layer1, layerTransform, gfx::PointF(0, 0), gfx::Size(50, 50), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(layer2, occlusion);
        this->EnterLayer(layer1, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(20, 20, 50, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());

        // This checks cases where the quads don't match their "containing"
        // layers, e.g. in terms of transforms or clip rect. This is typical for
        // DelegatedRendererLayer.

        gfx::Transform quadTransform;
        quadTransform.Translate(30, 30);
        gfx::Rect clipRectInTarget(0, 0, 100, 100);

        EXPECT_TRUE(occlusion.UnoccludedContentRect(parent, gfx::Rect(0, 0, 10, 10), quadTransform, false, true, clipRectInTarget, NULL).IsEmpty());
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 10, 10), occlusion.UnoccludedContentRect(parent, gfx::Rect(0, 0, 10, 10), quadTransform, true, true, clipRectInTarget, NULL));
        EXPECT_RECT_EQ(gfx::Rect(40, 40, 10, 10), occlusion.UnoccludedContentRect(parent, gfx::Rect(40, 40, 10, 10), quadTransform, false, true, clipRectInTarget, NULL));
        EXPECT_RECT_EQ(gfx::Rect(40, 30, 5, 10), occlusion.UnoccludedContentRect(parent, gfx::Rect(35, 30, 10, 10), quadTransform, false, true, clipRectInTarget, NULL));
        EXPECT_RECT_EQ(gfx::Rect(40, 40, 5, 5), occlusion.UnoccludedContentRect(parent, gfx::Rect(40, 40, 10, 10), quadTransform, false, true, gfx::Rect(0, 0, 75, 75), NULL));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestQuadsMismatchLayer);

template<class Types>
class OcclusionTrackerTestRotatedChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestRotatedChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform layerTransform;
        layerTransform.Translate(250, 250);
        layerTransform.Rotate(90);
        layerTransform.Translate(-250, -250);

        typename Types::ContentLayerType* root = this->createRoot(this->identityMatrix, gfx::Point(0, 0), gfx::Size(200, 200));
        typename Types::ContentLayerType* parent = this->createDrawingLayer(root, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, layerTransform, gfx::PointF(30, 30), gfx::Size(500, 500), true);
        parent->SetMasksToBounds(true);
        this->calcDrawEtc(root);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(layer, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(29, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(30, 29, 70, 70)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(31, 30, 70, 70)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 31, 70, 70)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 30, 70, 70)).IsEmpty());
        EXPECT_RECT_EQ(gfx::Rect(29, 30, 1, 70), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(29, 30, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(29, 29, 70, 70), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(29, 29, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(30, 29, 70, 1), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 29, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(31, 29, 69, 1), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(31, 29, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(31, 30, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(31, 31, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 31, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(29, 31, 1, 69), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(29, 31, 70, 70)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestRotatedChild);

template<class Types>
class OcclusionTrackerTestTranslatedChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestTranslatedChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform layerTransform;
        layerTransform.Translate(20, 20);

        typename Types::ContentLayerType* root = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200));
        typename Types::ContentLayerType* parent = this->createDrawingLayer(root, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, layerTransform, gfx::PointF(30, 30), gfx::Size(500, 500), true);
        parent->SetMasksToBounds(true);
        this->calcDrawEtc(root);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(layer, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(50, 50, 50, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(50, 50, 50, 50)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(49, 50, 50, 50)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(50, 49, 50, 50)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(51, 50, 50, 50)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(50, 51, 50, 50)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(50, 50, 50, 50)).IsEmpty());
        EXPECT_RECT_EQ(gfx::Rect(49, 50, 1, 50), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(49, 50, 50, 50)));
        EXPECT_RECT_EQ(gfx::Rect(49, 49, 50, 50), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(49, 49, 50, 50)));
        EXPECT_RECT_EQ(gfx::Rect(50, 49, 50, 1), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(50, 49, 50, 50)));
        EXPECT_RECT_EQ(gfx::Rect(51, 49, 49, 1), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(51, 49, 50, 50)));
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(51, 50, 50, 50)).IsEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(51, 51, 50, 50)).IsEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(50, 51, 50, 50)).IsEmpty());
        EXPECT_RECT_EQ(gfx::Rect(49, 51, 1, 49), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(49, 51, 50, 50)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestTranslatedChild);

template<class Types>
class OcclusionTrackerTestChildInRotatedChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestChildInRotatedChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform childTransform;
        childTransform.Translate(250, 250);
        childTransform.Rotate(90);
        childTransform.Translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100));
        parent->SetMasksToBounds(true);
        typename Types::LayerType* child = this->createLayer(parent, childTransform, gfx::PointF(30, 30), gfx::Size(500, 500));
        child->SetMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(child, this->identityMatrix, gfx::PointF(10, 10), gfx::Size(500, 500), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(layer, occlusion);
        this->enterContributingSurface(child, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->leaveContributingSurface(child, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(30, 40, 70, 60).ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(29, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(30, 39, 70, 60)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(31, 40, 70, 60)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 41, 70, 60)));

        /* Justification for the above occlusion from |layer|:
                   100
          +---------------------+                                      +---------------------+
          |                     |                                      |                     |30  Visible region of |layer|: /////
          |    30               |           rotate(90)                 |                     |
          | 30 + ---------------------------------+                    |     +---------------------------------+
      100 |    |  10            |                 |            ==>     |     |               |10               |
          |    |10+---------------------------------+                  |  +---------------------------------+  |
          |    |  |             |                 | |                  |  |  |///////////////|     420      |  |
          |    |  |             |                 | |                  |  |  |///////////////|60            |  |
          |    |  |             |                 | |                  |  |  |///////////////|              |  |
          +----|--|-------------+                 | |                  +--|--|---------------+              |  |
               |  |                               | |                   20|10|     70                       |  |
               |  |                               | |                     |  |                              |  |
               |  |                               | |500                  |  |                              |  |
               |  |                               | |                     |  |                              |  |
               |  |                               | |                     |  |                              |  |
               |  |                               | |                     |  |                              |  |
               |  |                               | |                     |  |                              |10|
               +--|-------------------------------+ |                     |  +------------------------------|--+
                  |                                 |                     |                 490             |
                  +---------------------------------+                     +---------------------------------+
                                 500                                                     500
         */
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestChildInRotatedChild);

template<class Types>
class OcclusionTrackerTestScaledRenderSurface : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestScaledRenderSurface(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}

    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200));

        gfx::Transform layer1Matrix;
        layer1Matrix.Scale(2, 2);
        typename Types::ContentLayerType* layer1 = this->createDrawingLayer(parent, layer1Matrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        layer1->SetForceRenderSurface(true);

        gfx::Transform layer2Matrix;
        layer2Matrix.Translate(25, 25);
        typename Types::ContentLayerType* layer2 = this->createDrawingLayer(layer1, layer2Matrix, gfx::PointF(0, 0), gfx::Size(50, 50), true);
        typename Types::ContentLayerType* occluder = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(100, 100), gfx::Size(500, 500), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(occluder, occlusion);
        this->EnterLayer(layer2, occlusion);

        EXPECT_EQ(gfx::Rect(100, 100, 100, 100).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_RECT_EQ(gfx::Rect(0, 0, 25, 25), occlusion.unoccludedLayerContentRect(layer2, gfx::Rect(0, 0, 25, 25)));
        EXPECT_RECT_EQ(gfx::Rect(10, 25, 15, 25), occlusion.unoccludedLayerContentRect(layer2, gfx::Rect(10, 25, 25, 25)));
        EXPECT_RECT_EQ(gfx::Rect(25, 10, 25, 15), occlusion.unoccludedLayerContentRect(layer2, gfx::Rect(25, 10, 25, 25)));
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(layer2, gfx::Rect(25, 25, 25, 25)).IsEmpty());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledRenderSurface);

template<class Types>
class OcclusionTrackerTestVisitTargetTwoTimes : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestVisitTargetTwoTimes(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform childTransform;
        childTransform.Translate(250, 250);
        childTransform.Rotate(90);
        childTransform.Translate(-250, -250);

        typename Types::ContentLayerType* root = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200));
        typename Types::ContentLayerType* parent = this->createDrawingLayer(root, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        parent->SetMasksToBounds(true);
        typename Types::LayerType* child = this->createLayer(parent, childTransform, gfx::PointF(30, 30), gfx::Size(500, 500));
        child->SetMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(child, this->identityMatrix, gfx::PointF(10, 10), gfx::Size(500, 500), true);
        // |child2| makes |parent|'s surface get considered by OcclusionTracker first, instead of |child|'s. This exercises different code in
        // leaveToTargetRenderSurface, as the target surface has already been seen.
        typename Types::ContentLayerType* child2 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(30, 30), gfx::Size(60, 20), true);
        this->calcDrawEtc(root);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(child2, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(30, 30, 60, 20).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->visitLayer(layer, occlusion);

        EXPECT_EQ(gfx::Rect(0, 440, 20, 60).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->enterContributingSurface(child, occlusion);

        EXPECT_EQ(gfx::Rect(0, 440, 20, 60).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(), occlusion.occlusion_from_inside_target().ToString());

        // Occlusion in |child2| should get merged with the |child| surface we are leaving now.
        this->leaveContributingSurface(child, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(UnionRegions(gfx::Rect(30, 30, 60, 10), gfx::Rect(30, 40, 70, 60)).ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(30, 30, 70, 70)));
        EXPECT_RECT_EQ(gfx::Rect(90, 30, 10, 10), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 30, 70, 70)));

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 30, 60, 10)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(29, 30, 60, 10)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(30, 29, 60, 10)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(31, 30, 60, 10)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 31, 60, 10)));

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(29, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(30, 39, 70, 60)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 30, 60, 10)).IsEmpty());
        EXPECT_RECT_EQ(gfx::Rect(29, 30, 1, 10), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(29, 30, 60, 10)));
        EXPECT_RECT_EQ(gfx::Rect(30, 29, 60, 1), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 29, 60, 10)));
        EXPECT_RECT_EQ(gfx::Rect(90, 30, 1, 10), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(31, 30, 60, 10)));
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 31, 60, 10)).IsEmpty());

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 40, 70, 60)).IsEmpty());
        EXPECT_RECT_EQ(gfx::Rect(29, 40, 1, 60), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(29, 40, 70, 60)));
        // This rect is mostly occluded by |child2|.
        EXPECT_RECT_EQ(gfx::Rect(90, 39, 10, 1), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 39, 70, 60)));
        // This rect extends past top/right ends of |child2|.
        EXPECT_RECT_EQ(gfx::Rect(30, 29, 70, 11), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 29, 70, 70)));
        // This rect extends past left/right ends of |child2|.
        EXPECT_RECT_EQ(gfx::Rect(20, 39, 80, 60), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(20, 39, 80, 60)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(31, 40, 70, 60)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 41, 70, 60)));

        /* Justification for the above occlusion from |layer|:
                   100
          +---------------------+                                      +---------------------+
          |                     |                                      |                     |30  Visible region of |layer|: /////
          |    30               |           rotate(90)                 |     30   60         |    |child2|: \\\\\
          | 30 + ------------+--------------------+                    |  30 +------------+--------------------+
      100 |    |  10         |  |                 |            ==>     |     |\\\\\\\\\\\\|  |10               |
          |    |10+----------|----------------------+                  |  +--|\\\\\\\\\\\\|-----------------+  |
          |    + ------------+  |                 | |                  |  |  +------------+//|     420      |  |
          |    |  |             |                 | |                  |  |  |///////////////|60            |  |
          |    |  |             |                 | |                  |  |  |///////////////|              |  |
          +----|--|-------------+                 | |                  +--|--|---------------+              |  |
               |  |                               | |                   20|10|     70                       |  |
               |  |                               | |                     |  |                              |  |
               |  |                               | |500                  |  |                              |  |
               |  |                               | |                     |  |                              |  |
               |  |                               | |                     |  |                              |  |
               |  |                               | |                     |  |                              |  |
               |  |                               | |                     |  |                              |10|
               +--|-------------------------------+ |                     |  +------------------------------|--+
                  |                                 |                     |                 490             |
                  +---------------------------------+                     +---------------------------------+
                                 500                                                     500
         */
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestVisitTargetTwoTimes);

template<class Types>
class OcclusionTrackerTestSurfaceRotatedOffAxis : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceRotatedOffAxis(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform childTransform;
        childTransform.Translate(250, 250);
        childTransform.Rotate(95);
        childTransform.Translate(-250, -250);

        gfx::Transform layerTransform;
        layerTransform.Translate(10, 10);

        typename Types::ContentLayerType* root = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(1000, 1000));
        typename Types::ContentLayerType* parent = this->createDrawingLayer(root, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        typename Types::LayerType* child = this->createLayer(parent, childTransform, gfx::PointF(30, 30), gfx::Size(500, 500));
        child->SetMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(child, layerTransform, gfx::PointF(0, 0), gfx::Size(500, 500), true);
        this->calcDrawEtc(root);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        gfx::Rect clippedLayerInChild = MathUtil::MapClippedRect(layerTransform, layer->visible_content_rect());

        this->visitLayer(layer, occlusion);
        this->enterContributingSurface(child, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(clippedLayerInChild.ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->leaveContributingSurface(child, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(75, 55, 1, 1)));
        EXPECT_RECT_EQ(gfx::Rect(75, 55, 1, 1), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(75, 55, 1, 1)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceRotatedOffAxis);

template<class Types>
class OcclusionTrackerTestSurfaceWithTwoOpaqueChildren : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceWithTwoOpaqueChildren(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform childTransform;
        childTransform.Translate(250, 250);
        childTransform.Rotate(90);
        childTransform.Translate(-250, -250);

        typename Types::ContentLayerType* root = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(1000, 1000));
        typename Types::ContentLayerType* parent = this->createDrawingLayer(root, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        parent->SetMasksToBounds(true);
        typename Types::ContentLayerType* child = this->createDrawingLayer(parent, childTransform, gfx::PointF(30, 30), gfx::Size(500, 500), false);
        child->SetMasksToBounds(true);
        typename Types::ContentLayerType* layer1 = this->createDrawingLayer(child, this->identityMatrix, gfx::PointF(10, 10), gfx::Size(500, 500), true);
        typename Types::ContentLayerType* layer2 = this->createDrawingLayer(child, this->identityMatrix, gfx::PointF(10, 450), gfx::Size(500, 60), true);
        this->calcDrawEtc(root);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(layer2, occlusion);
        this->visitLayer(layer1, occlusion);
        this->visitLayer(child, occlusion);
        this->enterContributingSurface(child, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_TRUE(occlusion.occludedLayer(child, gfx::Rect(10, 430, 60, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(child, gfx::Rect(9, 430, 60, 70)));
        // These rects are occluded except for the part outside the bounds of the target surface.
        EXPECT_TRUE(occlusion.occludedLayer(child, gfx::Rect(10, 429, 60, 70)));
        EXPECT_TRUE(occlusion.occludedLayer(child, gfx::Rect(11, 430, 60, 70)));
        EXPECT_TRUE(occlusion.occludedLayer(child, gfx::Rect(10, 431, 60, 70)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(child, gfx::Rect(10, 430, 60, 70)).IsEmpty());
        EXPECT_RECT_EQ(gfx::Rect(9, 430, 1, 70), occlusion.unoccludedLayerContentRect(child, gfx::Rect(9, 430, 60, 70)));
        // These rects are occluded except for the part outside the bounds of the target surface.
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(child, gfx::Rect(10, 429, 60, 70)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(child, gfx::Rect(11, 430, 60, 70)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(child, gfx::Rect(10, 431, 60, 70)));

        this->leaveContributingSurface(child, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(30, 40, 70, 60).ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(29, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(30, 39, 70, 60)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 40, 70, 60)).IsEmpty());
        EXPECT_RECT_EQ(gfx::Rect(29, 40, 1, 60), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(29, 40, 70, 60)));
        EXPECT_RECT_EQ(gfx::Rect(30, 39, 70, 1), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 39, 70, 60)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(31, 40, 70, 60)));
        EXPECT_RECT_EQ(gfx::Rect(), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(30, 41, 70, 60)));

        /* Justification for the above occlusion from |layer1| and |layer2|:

           +---------------------+
           |                     |30  Visible region of |layer1|: /////
           |                     |    Visible region of |layer2|: \\\\\
           |     +---------------------------------+
           |     |               |10               |
           |  +---------------+-----------------+  |
           |  |  |\\\\\\\\\\\\|//|     420      |  |
           |  |  |\\\\\\\\\\\\|//|60            |  |
           |  |  |\\\\\\\\\\\\|//|              |  |
           +--|--|------------|--+              |  |
            20|10|     70     |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |10|
              |  +------------|-----------------|--+
              |               | 490             |
              +---------------+-----------------+
                     60               440
         */
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceWithTwoOpaqueChildren);

template<class Types>
class OcclusionTrackerTestOverlappingSurfaceSiblings : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestOverlappingSurfaceSiblings(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform childTransform;
        childTransform.Translate(250, 250);
        childTransform.Rotate(90);
        childTransform.Translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100));
        parent->SetMasksToBounds(true);
        typename Types::LayerType* child1 = this->createSurface(parent, childTransform, gfx::PointF(30, 30), gfx::Size(10, 10));
        typename Types::LayerType* child2 = this->createSurface(parent, childTransform, gfx::PointF(20, 40), gfx::Size(10, 10));
        typename Types::ContentLayerType* layer1 = this->createDrawingLayer(child1, this->identityMatrix, gfx::PointF(-10, -10), gfx::Size(510, 510), true);
        typename Types::ContentLayerType* layer2 = this->createDrawingLayer(child2, this->identityMatrix, gfx::PointF(-10, -10), gfx::Size(510, 510), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(layer2, occlusion);
        this->enterContributingSurface(child2, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(-10, 420, 70, 80).ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_TRUE(occlusion.occludedLayer(child2, gfx::Rect(-10, 420, 70, 80)));
        EXPECT_TRUE(occlusion.occludedLayer(child2, gfx::Rect(-11, 420, 70, 80)));
        EXPECT_TRUE(occlusion.occludedLayer(child2, gfx::Rect(-10, 419, 70, 80)));
        EXPECT_TRUE(occlusion.occludedLayer(child2, gfx::Rect(-10, 420, 71, 80)));
        EXPECT_TRUE(occlusion.occludedLayer(child2, gfx::Rect(-10, 420, 70, 81)));

        // There is nothing above child2's surface in the z-order.
        EXPECT_RECT_EQ(gfx::Rect(-10, 420, 70, 80), occlusion.UnoccludedContributingSurfaceContentRect(child2, false, gfx::Rect(-10, 420, 70, 80), NULL));

        this->leaveContributingSurface(child2, occlusion);
        this->visitLayer(layer1, occlusion);
        this->enterContributingSurface(child1, occlusion);

        EXPECT_EQ(gfx::Rect(0, 430, 70, 80).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(-10, 430, 80, 70).ToString(), occlusion.occlusion_from_inside_target().ToString());

        // child2's contents will occlude child1 below it.
        EXPECT_RECT_EQ(gfx::Rect(-10, 430, 10, 70), occlusion.UnoccludedContributingSurfaceContentRect(child1, false, gfx::Rect(-10, 430, 80, 70), NULL));

        this->leaveContributingSurface(child1, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(UnionRegions(gfx::Rect(30, 20, 70, 10), gfx::Rect(20, 30, 80, 70)).ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(20, 20, 80, 80)));

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(30, 20, 70, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(29, 20, 70, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(30, 19, 70, 80)));

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(20, 30, 80, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(19, 30, 80, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(20, 29, 80, 70)));

        /* Justification for the above occlusion:
                   100
          +---------------------+
          |    20               |       layer1
          |  30+ ---------------------------------+
      100 |  30|                |     layer2      |
          |20+----------------------------------+ |
          |  | |                |               | |
          |  | |                |               | |
          |  | |                |               | |
          +--|-|----------------+               | |
             | |                                | | 510
             | |                                | |
             | |                                | |
             | |                                | |
             | |                                | |
             | |                                | |
             | |                                | |
             | +--------------------------------|-+
             |                                  |
             +----------------------------------+
                             510
         */
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestOverlappingSurfaceSiblings);

template<class Types>
class OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform child1Transform;
        child1Transform.Translate(250, 250);
        child1Transform.Rotate(-90);
        child1Transform.Translate(-250, -250);

        gfx::Transform child2Transform;
        child2Transform.Translate(250, 250);
        child2Transform.Rotate(90);
        child2Transform.Translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100));
        parent->SetMasksToBounds(true);
        typename Types::LayerType* child1 = this->createSurface(parent, child1Transform, gfx::PointF(30, 20), gfx::Size(10, 10));
        typename Types::LayerType* child2 = this->createDrawingSurface(parent, child2Transform, gfx::PointF(20, 40), gfx::Size(10, 10), false);
        typename Types::ContentLayerType* layer1 = this->createDrawingLayer(child1, this->identityMatrix, gfx::PointF(-10, -20), gfx::Size(510, 510), true);
        typename Types::ContentLayerType* layer2 = this->createDrawingLayer(child2, this->identityMatrix, gfx::PointF(-10, -10), gfx::Size(510, 510), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(layer2, occlusion);
        this->EnterLayer(child2, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(-10, 420, 70, 80).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->LeaveLayer(child2, occlusion);
        this->enterContributingSurface(child2, occlusion);

        // There is nothing above child2's surface in the z-order.
        EXPECT_RECT_EQ(gfx::Rect(-10, 420, 70, 80), occlusion.UnoccludedContributingSurfaceContentRect(child2, false, gfx::Rect(-10, 420, 70, 80), NULL));

        this->leaveContributingSurface(child2, occlusion);
        this->visitLayer(layer1, occlusion);
        this->enterContributingSurface(child1, occlusion);

        EXPECT_EQ(gfx::Rect(420, -10, 70, 80).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(420, -20, 80, 90).ToString(), occlusion.occlusion_from_inside_target().ToString());

        // child2's contents will occlude child1 below it.
        EXPECT_RECT_EQ(gfx::Rect(420, -20, 80, 90), occlusion.UnoccludedContributingSurfaceContentRect(child1, false, gfx::Rect(420, -20, 80, 90), NULL));
        EXPECT_RECT_EQ(gfx::Rect(490, -10, 10, 80), occlusion.UnoccludedContributingSurfaceContentRect(child1, false, gfx::Rect(420, -10, 80, 90), NULL));
        EXPECT_RECT_EQ(gfx::Rect(420, -20, 70, 10), occlusion.UnoccludedContributingSurfaceContentRect(child1, false, gfx::Rect(420, -20, 70, 90), NULL));

        this->leaveContributingSurface(child1, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(10, 20, 90, 80).ToString(), occlusion.occlusion_from_inside_target().ToString());

        /* Justification for the above occlusion:
                      100
            +---------------------+
            |20                   |       layer1
           10+----------------------------------+
        100 || 30                 |     layer2  |
            |20+----------------------------------+
            || |                  |             | |
            || |                  |             | |
            || |                  |             | |
            +|-|------------------+             | |
             | |                                | | 510
             | |                            510 | |
             | |                                | |
             | |                                | |
             | |                                | |
             | |                                | |
             | |                520             | |
             +----------------------------------+ |
               |                                  |
               +----------------------------------+
                               510
         */
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms);

template<class Types>
class OcclusionTrackerTestFilters : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestFilters(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform layerTransform;
        layerTransform.Translate(250, 250);
        layerTransform.Rotate(90);
        layerTransform.Translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100));
        parent->SetMasksToBounds(true);
        typename Types::ContentLayerType* blurLayer = this->createDrawingLayer(parent, layerTransform, gfx::PointF(30, 30), gfx::Size(500, 500), true);
        typename Types::ContentLayerType* opaqueLayer = this->createDrawingLayer(parent, layerTransform, gfx::PointF(30, 30), gfx::Size(500, 500), true);
        typename Types::ContentLayerType* opacityLayer = this->createDrawingLayer(parent, layerTransform, gfx::PointF(30, 30), gfx::Size(500, 500), true);

        WebKit::WebFilterOperations filters;
        filters.append(WebKit::WebFilterOperation::createBlurFilter(10));
        blurLayer->SetFilters(filters);

        filters.clear();
        filters.append(WebKit::WebFilterOperation::createGrayscaleFilter(0.5));
        opaqueLayer->SetFilters(filters);

        filters.clear();
        filters.append(WebKit::WebFilterOperation::createOpacityFilter(0.5));
        opacityLayer->SetFilters(filters);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        // Opacity layer won't contribute to occlusion.
        this->visitLayer(opacityLayer, occlusion);
        this->enterContributingSurface(opacityLayer, occlusion);

        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
        EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

        // And has nothing to contribute to its parent surface.
        this->leaveContributingSurface(opacityLayer, occlusion);
        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
        EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

        // Opaque layer will contribute to occlusion.
        this->visitLayer(opaqueLayer, occlusion);
        this->enterContributingSurface(opaqueLayer, occlusion);

        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
        EXPECT_EQ(gfx::Rect(0, 430, 70, 70).ToString(), occlusion.occlusion_from_inside_target().ToString());

        // And it gets translated to the parent surface.
        this->leaveContributingSurface(opaqueLayer, occlusion);
        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
        EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(), occlusion.occlusion_from_inside_target().ToString());

        // The blur layer needs to throw away any occlusion from outside its subtree.
        this->EnterLayer(blurLayer, occlusion);
        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
        EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

        // And it won't contribute to occlusion.
        this->LeaveLayer(blurLayer, occlusion);
        this->enterContributingSurface(blurLayer, occlusion);
        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
        EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

        // But the opaque layer's occlusion is preserved on the parent. 
        this->leaveContributingSurface(blurLayer, occlusion);
        this->EnterLayer(parent, occlusion);
        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
        EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(), occlusion.occlusion_from_inside_target().ToString());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestFilters);

template<class Types>
class OcclusionTrackerTestReplicaDoesOcclude : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestReplicaDoesOcclude(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 100), gfx::Size(50, 50), true);
        this->createReplicaLayer(surface, this->identityMatrix, gfx::PointF(50, 50), gfx::Size());
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(surface, occlusion);

        EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->visitContributingSurface(surface, occlusion);
        this->EnterLayer(parent, occlusion);

        // The surface and replica should both be occluding the parent.
        EXPECT_EQ(UnionRegions(gfx::Rect(0, 100, 50, 50), gfx::Rect(50, 150, 50, 50)).ToString(), occlusion.occlusion_from_inside_target().ToString());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaDoesOcclude);

template<class Types>
class OcclusionTrackerTestReplicaWithClipping : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestReplicaWithClipping(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 170));
        parent->SetMasksToBounds(true);
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 100), gfx::Size(50, 50), true);
        this->createReplicaLayer(surface, this->identityMatrix, gfx::PointF(50, 50), gfx::Size());
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(surface, occlusion);

        EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->visitContributingSurface(surface, occlusion);
        this->EnterLayer(parent, occlusion);

        // The surface and replica should both be occluding the parent.
        EXPECT_EQ(UnionRegions(gfx::Rect(0, 100, 50, 50), gfx::Rect(50, 150, 50, 20)).ToString(), occlusion.occlusion_from_inside_target().ToString());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaWithClipping);

template<class Types>
class OcclusionTrackerTestReplicaWithMask : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestReplicaWithMask(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 100), gfx::Size(50, 50), true);
        typename Types::LayerType* replica = this->createReplicaLayer(surface, this->identityMatrix, gfx::PointF(50, 50), gfx::Size());
        this->createMaskLayer(replica, gfx::Size(10, 10));
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(surface, occlusion);

        EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->visitContributingSurface(surface, occlusion);
        this->EnterLayer(parent, occlusion);

        // The replica should not be occluding the parent, since it has a mask applied to it.
        EXPECT_EQ(gfx::Rect(0, 100, 50, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaWithMask);

template<class Types>
class OcclusionTrackerTestLayerClipRectOutsideChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerClipRectOutsideChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* clip = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(200, 100), gfx::Size(100, 100), false);
        clip->SetMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(clip, this->identityMatrix, gfx::PointF(-200, -100), gfx::Size(200, 200), false);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->EnterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(100, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(200, 100, 100, 100)));

        this->LeaveLayer(layer, occlusion);
        this->EnterLayer(clip, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(-100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(0, -100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(clip, gfx::Rect(0, 0, 100, 100)));

        EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), occlusion.unoccludedLayerContentRect(clip, gfx::Rect(-100, -100, 300, 300)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestLayerClipRectOutsideChild);

template<class Types>
class OcclusionTrackerTestViewportRectOutsideChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestViewportRectOutsideChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(200, 100, 100, 100));

        this->EnterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(100, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(200, 100, 100, 100)));

        this->LeaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(200, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(200, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(200, 200, 100, 100)));

        EXPECT_RECT_EQ(gfx::Rect(200, 100, 100, 100), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 300)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestViewportRectOutsideChild);

template<class Types>
class OcclusionTrackerTestLayerClipRectOverChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerClipRectOverChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* clip = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(100, 100), gfx::Size(100, 100), false);
        clip->SetMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingSurface(clip, this->identityMatrix, gfx::PointF(-100, -100), gfx::Size(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->EnterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(100, 100, 100, 100)));

        this->LeaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);

        EXPECT_EQ(gfx::Rect(100, 100, 100, 100).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->EnterLayer(clip, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(100, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(200, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(200, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(0, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(100, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(clip, gfx::Rect(200, 200, 100, 100)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(clip, gfx::Rect(0, 0, 300, 300)).IsEmpty());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestLayerClipRectOverChild);

template<class Types>
class OcclusionTrackerTestViewportRectOverChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestViewportRectOverChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(100, 100, 100, 100));

        this->EnterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(100, 100, 100, 100)));

        this->LeaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(200, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(200, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(200, 200, 100, 100)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 300)).IsEmpty());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestViewportRectOverChild);

template<class Types>
class OcclusionTrackerTestLayerClipRectPartlyOverChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerClipRectPartlyOverChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* clip = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(50, 50), gfx::Size(200, 200), false);
        clip->SetMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingSurface(clip, this->identityMatrix, gfx::PointF(-50, -50), gfx::Size(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->EnterLayer(layer, occlusion);

        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(100, 100, 100, 100)));

        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 100, 50)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 50, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(100, 0, 100, 50)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 100, 50, 100)));

        this->LeaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->EnterLayer(clip, occlusion);

        EXPECT_EQ(gfx::Rect(50, 50, 150, 150).ToString(), occlusion.occlusion_from_inside_target().ToString());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestLayerClipRectPartlyOverChild);

template<class Types>
class OcclusionTrackerTestViewportRectPartlyOverChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestViewportRectPartlyOverChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(50, 50, 200, 200));

        this->EnterLayer(layer, occlusion);

        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(100, 100, 100, 100)));

        this->LeaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(200, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(200, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(0, 200, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(100, 200, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(200, 200, 100, 100)));

        EXPECT_RECT_EQ(gfx::Rect(50, 50, 200, 200), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 300)));
        EXPECT_RECT_EQ(gfx::Rect(200, 50, 50, 50), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 100)));
        EXPECT_RECT_EQ(gfx::Rect(200, 100, 50, 100), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 100, 300, 100)));
        EXPECT_RECT_EQ(gfx::Rect(200, 100, 50, 100), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(200, 100, 100, 100)));
        EXPECT_RECT_EQ(gfx::Rect(100, 200, 100, 50), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(100, 200, 100, 100)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestViewportRectPartlyOverChild);

template<class Types>
class OcclusionTrackerTestViewportRectOverNothing : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestViewportRectOverNothing(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(500, 500, 100, 100));

        this->EnterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(100, 100, 100, 100)));

        this->LeaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(200, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(200, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(0, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(200, 200, 100, 100)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 300)).IsEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 100)).IsEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 100, 300, 100)).IsEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(200, 100, 100, 100)).IsEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, gfx::Rect(100, 200, 100, 100)).IsEmpty());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestViewportRectOverNothing);

template<class Types>
class OcclusionTrackerTestLayerClipRectForLayerOffOrigin : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerClipRectForLayerOffOrigin(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));
        this->EnterLayer(layer, occlusion);

        // This layer is translated when drawn into its target. So if the clip rect given from the target surface
        // is not in that target space, then after translating these query rects into the target, they will fall outside
        // the clip and be considered occluded.
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(100, 100, 100, 100)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestLayerClipRectForLayerOffOrigin);

template<class Types>
class OcclusionTrackerTestOpaqueContentsRegionEmpty : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestOpaqueContentsRegionEmpty(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200), false);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));
        this->EnterLayer(layer, occlusion);

        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, gfx::Rect(100, 100, 100, 100)));

        // Occluded since its outside the surface bounds.
        EXPECT_TRUE(occlusion.occludedLayer(layer, gfx::Rect(200, 100, 100, 100)));

        this->LeaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->EnterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestOpaqueContentsRegionEmpty);

template<class Types>
class OcclusionTrackerTestOpaqueContentsRegionNonEmpty : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestOpaqueContentsRegionNonEmpty(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(100, 100), gfx::Size(200, 200), false);
        this->calcDrawEtc(parent);

        {
            TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));
            layer->setOpaqueContentsRect(gfx::Rect(0, 0, 100, 100));

            this->resetLayerIterator();
            this->visitLayer(layer, occlusion);
            this->EnterLayer(parent, occlusion);

            EXPECT_EQ(gfx::Rect(100, 100, 100, 100).ToString(), occlusion.occlusion_from_inside_target().ToString());

            EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
            EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(100, 100, 100, 100)));
            EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(200, 200, 100, 100)));
        }

        {
            TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));
            layer->setOpaqueContentsRect(gfx::Rect(20, 20, 180, 180));

            this->resetLayerIterator();
            this->visitLayer(layer, occlusion);
            this->EnterLayer(parent, occlusion);

            EXPECT_EQ(gfx::Rect(120, 120, 180, 180).ToString(), occlusion.occlusion_from_inside_target().ToString());

            EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
            EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(100, 100, 100, 100)));
            EXPECT_TRUE(occlusion.occludedLayer(parent, gfx::Rect(200, 200, 100, 100)));
        }

        {
            TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));
            layer->setOpaqueContentsRect(gfx::Rect(150, 150, 100, 100));

            this->resetLayerIterator();
            this->visitLayer(layer, occlusion);
            this->EnterLayer(parent, occlusion);

            EXPECT_EQ(gfx::Rect(250, 250, 50, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());

            EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
            EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(100, 100, 100, 100)));
            EXPECT_FALSE(occlusion.occludedLayer(parent, gfx::Rect(200, 200, 100, 100)));
        }
    }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestOpaqueContentsRegionNonEmpty);

template<class Types>
class OcclusionTrackerTest3dTransform : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTest3dTransform(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform transform;
        transform.RotateAboutYAxis(30);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::LayerType* container = this->createLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(container, transform, gfx::PointF(100, 100), gfx::Size(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));
        this->EnterLayer(layer, occlusion);

        // The layer is rotated in 3d but without preserving 3d, so it only gets resized.
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 200, 200), occlusion.unoccludedLayerContentRect(layer, gfx::Rect(0, 0, 200, 200)));
    }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTest3dTransform);

template<class Types>
class OcclusionTrackerTestUnsorted3dLayers : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestUnsorted3dLayers(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        // Currently, the main thread layer iterator does not iterate over 3d items in
        // sorted order, because layer sorting is not performed on the main thread.
        // Because of this, the occlusion tracker cannot assume that a 3d layer occludes
        // other layers that have not yet been iterated over. For now, the expected
        // behavior is that a 3d layer simply does not add any occlusion to the occlusion
        // tracker.

        gfx::Transform translationToFront;
        translationToFront.Translate3d(0, 0, -10);
        gfx::Transform translationToBack;
        translationToFront.Translate3d(0, 0, -100);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* child1 = this->createDrawingLayer(parent, translationToBack, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        typename Types::ContentLayerType* child2 = this->createDrawingLayer(parent, translationToFront, gfx::PointF(50, 50), gfx::Size(100, 100), true);
        parent->SetPreserves3d(true);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));
        this->visitLayer(child2, occlusion);
        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
        EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

        this->visitLayer(child1, occlusion);
        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
        EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
    }
};

// This test will have different layer ordering on the impl thread; the test will only work on the main thread.
MAIN_THREAD_TEST(OcclusionTrackerTestUnsorted3dLayers);

template<class Types>
class OcclusionTrackerTestPerspectiveTransform : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestPerspectiveTransform(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform transform;
        transform.Translate(150, 150);
        transform.ApplyPerspectiveDepth(400);
        transform.RotateAboutXAxis(-30);
        transform.Translate(-150, -150);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::LayerType* container = this->createLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(container, transform, gfx::PointF(100, 100), gfx::Size(200, 200), true);
        container->SetPreserves3d(true);
        layer->SetPreserves3d(true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));
        this->EnterLayer(layer, occlusion);

        EXPECT_RECT_EQ(gfx::Rect(0, 0, 200, 200), occlusion.unoccludedLayerContentRect(layer, gfx::Rect(0, 0, 200, 200)));
    }
};

// This test requires accumulating occlusion of 3d layers, which are skipped by the occlusion tracker on the main thread. So this test should run on the impl thread.
IMPL_THREAD_TEST(OcclusionTrackerTestPerspectiveTransform);

template<class Types>
class OcclusionTrackerTestPerspectiveTransformBehindCamera : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestPerspectiveTransformBehindCamera(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        // This test is based on the platform/chromium/compositing/3d-corners.html layout test.
        gfx::Transform transform;
        transform.Translate(250, 50);
        transform.ApplyPerspectiveDepth(10);
        transform.Translate(-250, -50);
        transform.Translate(250, 50);
        transform.RotateAboutXAxis(-167);
        transform.Translate(-250, -50);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(500, 100));
        typename Types::LayerType* container = this->createLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(500, 500));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(container, transform, gfx::PointF(0, 0), gfx::Size(500, 500), true);
        container->SetPreserves3d(true);
        layer->SetPreserves3d(true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));
        this->EnterLayer(layer, occlusion);

        // The bottom 11 pixel rows of this layer remain visible inside the container, after translation to the target surface. When translated back,
        // this will include many more pixels but must include at least the bottom 11 rows.
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(layer, gfx::Rect(0, 0, 500, 500)).Contains(gfx::Rect(0, 489, 500, 11)));
    }
};

// This test requires accumulating occlusion of 3d layers, which are skipped by the occlusion tracker on the main thread. So this test should run on the impl thread.
IMPL_THREAD_TEST(OcclusionTrackerTestPerspectiveTransformBehindCamera);

template<class Types>
class OcclusionTrackerTestLayerBehindCameraDoesNotOcclude : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerBehindCameraDoesNotOcclude(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform transform;
        transform.Translate(50, 50);
        transform.ApplyPerspectiveDepth(100);
        transform.Translate3d(0, 0, 110);
        transform.Translate(-50, -50);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, transform, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        parent->SetPreserves3d(true);
        layer->SetPreserves3d(true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        // The |layer| is entirely behind the camera and should not occlude.
        this->visitLayer(layer, occlusion);
        this->EnterLayer(parent, occlusion);
        EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
        EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    }
};

// This test requires accumulating occlusion of 3d layers, which are skipped by the occlusion tracker on the main thread. So this test should run on the impl thread.
IMPL_THREAD_TEST(OcclusionTrackerTestLayerBehindCameraDoesNotOcclude);

template<class Types>
class OcclusionTrackerTestLargePixelsOccludeInsideClipRect : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLargePixelsOccludeInsideClipRect(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform transform;
        transform.Translate(50, 50);
        transform.ApplyPerspectiveDepth(100);
        transform.Translate3d(0, 0, 99);
        transform.Translate(-50, -50);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100));
        parent->SetMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, transform, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        parent->SetPreserves3d(true);
        layer->SetPreserves3d(true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        // This is very close to the camera, so pixels in its visibleContentRect will actually go outside of the layer's clipRect.
        // Ensure that those pixels don't occlude things outside the clipRect.
        this->visitLayer(layer, occlusion);
        this->EnterLayer(parent, occlusion);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
    }
};

// This test requires accumulating occlusion of 3d layers, which are skipped by the occlusion tracker on the main thread. So this test should run on the impl thread.
IMPL_THREAD_TEST(OcclusionTrackerTestLargePixelsOccludeInsideClipRect);

template<class Types>
class OcclusionTrackerTestAnimationOpacity1OnMainThread : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestAnimationOpacity1OnMainThread(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        // parent
        // +--layer
        // +--surface
        // |  +--surfaceChild
        // |  +--surfaceChild2
        // +--parent2
        // +--topmost

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300), true);
        typename Types::ContentLayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300), true);
        typename Types::ContentLayerType* surfaceChild = this->createDrawingLayer(surface, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 300), true);
        typename Types::ContentLayerType* surfaceChild2 = this->createDrawingLayer(surface, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 300), true);
        typename Types::ContentLayerType* parent2 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300), false);
        typename Types::ContentLayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(250, 0), gfx::Size(50, 300), true);

        addOpacityTransitionToController(*layer->layer_animation_controller(), 10, 0, 1, false);
        addOpacityTransitionToController(*surface->layer_animation_controller(), 10, 0, 1, false);
        this->calcDrawEtc(parent);

        EXPECT_TRUE(layer->draw_opacity_is_animating());
        EXPECT_FALSE(surface->draw_opacity_is_animating());
        EXPECT_TRUE(surface->render_surface()->draw_opacity_is_animating());

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(topmost, occlusion);
        this->EnterLayer(parent2, occlusion);
        // This occlusion will affect all surfaces.
        EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 250, 300).ToString(), occlusion.unoccludedLayerContentRect(parent2, gfx::Rect(0, 0, 300, 300)).ToString());
        this->LeaveLayer(parent2, occlusion);

        this->visitLayer(surfaceChild2, occlusion);
        this->EnterLayer(surfaceChild, occlusion);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_RECT_EQ(gfx::Rect(100, 0, 100, 300), occlusion.unoccludedLayerContentRect(surfaceChild, gfx::Rect(0, 0, 200, 300)));
        this->LeaveLayer(surfaceChild, occlusion);
        this->EnterLayer(surface, occlusion);
        EXPECT_EQ(gfx::Rect(0, 0, 200, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_RECT_EQ(gfx::Rect(200, 0, 50, 300), occlusion.unoccludedLayerContentRect(surface, gfx::Rect(0, 0, 300, 300)));
        this->LeaveLayer(surface, occlusion);

        this->enterContributingSurface(surface, occlusion);
        // Occlusion within the surface is lost when leaving the animating surface.
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 250, 300), occlusion.UnoccludedContributingSurfaceContentRect(surface, false, gfx::Rect(0, 0, 300, 300), NULL));
        this->leaveContributingSurface(surface, occlusion);

        // Occlusion from outside the animating surface still exists.
        EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());

        this->visitLayer(layer, occlusion);
        this->EnterLayer(parent, occlusion);

        // Occlusion is not added for the animating |layer|.
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 300)));
    }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationOpacity1OnMainThread);

template<class Types>
class OcclusionTrackerTestAnimationOpacity0OnMainThread : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestAnimationOpacity0OnMainThread(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300), true);
        typename Types::ContentLayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300), true);
        typename Types::ContentLayerType* surfaceChild = this->createDrawingLayer(surface, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 300), true);
        typename Types::ContentLayerType* surfaceChild2 = this->createDrawingLayer(surface, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 300), true);
        typename Types::ContentLayerType* parent2 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300), false);
        typename Types::ContentLayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(250, 0), gfx::Size(50, 300), true);

        addOpacityTransitionToController(*layer->layer_animation_controller(), 10, 1, 0, false);
        addOpacityTransitionToController(*surface->layer_animation_controller(), 10, 1, 0, false);
        this->calcDrawEtc(parent);

        EXPECT_TRUE(layer->draw_opacity_is_animating());
        EXPECT_FALSE(surface->draw_opacity_is_animating());
        EXPECT_TRUE(surface->render_surface()->draw_opacity_is_animating());

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(topmost, occlusion);
        this->EnterLayer(parent2, occlusion);
        // This occlusion will affect all surfaces.
        EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 300)));
        this->LeaveLayer(parent2, occlusion);

        this->visitLayer(surfaceChild2, occlusion);
        this->EnterLayer(surfaceChild, occlusion);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_RECT_EQ(gfx::Rect(100, 0, 100, 300), occlusion.unoccludedLayerContentRect(surfaceChild, gfx::Rect(0, 0, 200, 300)));
        this->LeaveLayer(surfaceChild, occlusion);
        this->EnterLayer(surface, occlusion);
        EXPECT_EQ(gfx::Rect(0, 0, 200, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_RECT_EQ(gfx::Rect(200, 0, 50, 300), occlusion.unoccludedLayerContentRect(surface, gfx::Rect(0, 0, 300, 300)));
        this->LeaveLayer(surface, occlusion);

        this->enterContributingSurface(surface, occlusion);
        // Occlusion within the surface is lost when leaving the animating surface.
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 250, 300), occlusion.UnoccludedContributingSurfaceContentRect(surface, false, gfx::Rect(0, 0, 300, 300), NULL));
        this->leaveContributingSurface(surface, occlusion);

        // Occlusion from outside the animating surface still exists.
        EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());

        this->visitLayer(layer, occlusion);
        this->EnterLayer(parent, occlusion);

        // Occlusion is not added for the animating |layer|.
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 300)));
    }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationOpacity0OnMainThread);

template<class Types>
class OcclusionTrackerTestAnimationTranslateOnMainThread : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestAnimationTranslateOnMainThread(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300), true);
        typename Types::ContentLayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300), true);
        typename Types::ContentLayerType* surfaceChild = this->createDrawingLayer(surface, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 300), true);
        typename Types::ContentLayerType* surfaceChild2 = this->createDrawingLayer(surface, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 300), true);
        typename Types::ContentLayerType* surface2 = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(50, 300), true);

        addAnimatedTransformToController(*layer->layer_animation_controller(), 10, 30, 0);
        addAnimatedTransformToController(*surface->layer_animation_controller(), 10, 30, 0);
        addAnimatedTransformToController(*surfaceChild->layer_animation_controller(), 10, 30, 0);
        this->calcDrawEtc(parent);

        EXPECT_TRUE(layer->draw_transform_is_animating());
        EXPECT_TRUE(layer->screen_space_transform_is_animating());
        EXPECT_TRUE(surface->render_surface()->target_surface_transforms_are_animating());
        EXPECT_TRUE(surface->render_surface()->screen_space_transforms_are_animating());
        // The surface owning layer doesn't animate against its own surface.
        EXPECT_FALSE(surface->draw_transform_is_animating());
        EXPECT_TRUE(surface->screen_space_transform_is_animating());
        EXPECT_TRUE(surfaceChild->draw_transform_is_animating());
        EXPECT_TRUE(surfaceChild->screen_space_transform_is_animating());

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(surface2, occlusion);
        this->enterContributingSurface(surface2, occlusion);

        EXPECT_EQ(gfx::Rect(0, 0, 50, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->leaveContributingSurface(surface2, occlusion);
        this->EnterLayer(surfaceChild2, occlusion);

        // surfaceChild2 is moving in screen space but not relative to its target, so occlusion should happen in its target space only.
        // It also means that things occluding from outside the target (e.g. surface2) cannot occlude this layer.
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());

        EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 300), occlusion.unoccludedLayerContentRect(surfaceChild2, gfx::Rect(0, 0, 100, 300)));
        EXPECT_FALSE(occlusion.occludedLayer(surfaceChild, gfx::Rect(0, 0, 50, 300)));

        this->LeaveLayer(surfaceChild2, occlusion);
        this->EnterLayer(surfaceChild, occlusion);
        EXPECT_FALSE(occlusion.occludedLayer(surfaceChild, gfx::Rect(0, 0, 100, 300)));
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_RECT_EQ(gfx::Rect(100, 0, 200, 300), occlusion.unoccludedLayerContentRect(surface, gfx::Rect(0, 0, 300, 300)));

        // The surfaceChild is occluded by the surfaceChild2, but is moving relative its target, so it can't be occluded.
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 200, 300), occlusion.unoccludedLayerContentRect(surfaceChild, gfx::Rect(0, 0, 200, 300)));
        EXPECT_FALSE(occlusion.occludedLayer(surfaceChild, gfx::Rect(0, 0, 50, 300)));

        this->LeaveLayer(surfaceChild, occlusion);
        this->EnterLayer(surface, occlusion);
        // The surfaceChild is moving in screen space but not relative to its target, so occlusion should happen from within the target only.
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_RECT_EQ(gfx::Rect(100, 0, 200, 300), occlusion.unoccludedLayerContentRect(surface, gfx::Rect(0, 0, 300, 300)));

        this->LeaveLayer(surface, occlusion);
        // The surface's owning layer is moving in screen space but not relative to its target, so occlusion should happen within the target only.
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 300, 300).ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), occlusion.unoccludedLayerContentRect(surface, gfx::Rect(0, 0, 300, 300)));

        this->enterContributingSurface(surface, occlusion);
        // The contributing |surface| is animating so it can't be occluded.
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 300, 300), occlusion.UnoccludedContributingSurfaceContentRect(surface, false, gfx::Rect(0, 0, 300, 300), NULL));
        this->leaveContributingSurface(surface, occlusion);

        this->EnterLayer(layer, occlusion);
        // The |surface| is moving in the screen and in its target, so all occlusion within the surface is lost when leaving it.
        EXPECT_RECT_EQ(gfx::Rect(50, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 300)));
        this->LeaveLayer(layer, occlusion);

        this->EnterLayer(parent, occlusion);
        // The |layer| is animating in the screen and in its target, so no occlusion is added.
        EXPECT_RECT_EQ(gfx::Rect(50, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent, gfx::Rect(0, 0, 300, 300)));
    }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationTranslateOnMainThread);

template<class Types>
class OcclusionTrackerTestSurfaceOcclusionTranslatesToParent : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceOcclusionTranslatesToParent(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform surfaceTransform;
        surfaceTransform.Translate(300, 300);
        surfaceTransform.Scale(2, 2);
        surfaceTransform.Translate(-150, -150);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(500, 500));
        typename Types::ContentLayerType* surface = this->createDrawingSurface(parent, surfaceTransform, gfx::PointF(0, 0), gfx::Size(300, 300), false);
        typename Types::ContentLayerType* surface2 = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(50, 50), gfx::Size(300, 300), false);
        surface->setOpaqueContentsRect(gfx::Rect(0, 0, 200, 200));
        surface2->setOpaqueContentsRect(gfx::Rect(0, 0, 200, 200));
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(surface2, occlusion);
        this->visitContributingSurface(surface2, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(50, 50, 200, 200).ToString(), occlusion.occlusion_from_inside_target().ToString());

        // Clear any stored occlusion.
        occlusion.set_occlusion_from_outside_target(Region());
        occlusion.set_occlusion_from_inside_target(Region());

        this->visitLayer(surface, occlusion);
        this->visitContributingSurface(surface, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 400, 400).ToString(), occlusion.occlusion_from_inside_target().ToString());
    }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestSurfaceOcclusionTranslatesToParent);

template<class Types>
class OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 300));
        parent->SetMasksToBounds(true);
        typename Types::ContentLayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(500, 300), false);
        surface->setOpaqueContentsRect(gfx::Rect(0, 0, 400, 200));
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(surface, occlusion);
        this->visitContributingSurface(surface, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 300, 200).ToString(), occlusion.occlusion_from_inside_target().ToString());
    }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping);

template<class Types>
class OcclusionTrackerTestReplicaOccluded : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestReplicaOccluded(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        this->createReplicaLayer(surface, this->identityMatrix, gfx::PointF(0, 100), gfx::Size(100, 100));
        typename Types::LayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 100), gfx::Size(100, 100), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        // |topmost| occludes the replica, but not the surface itself.
        this->visitLayer(topmost, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 100, 100, 100).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->visitLayer(surface, occlusion);

        EXPECT_EQ(gfx::Rect(0, 100, 100, 100).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->enterContributingSurface(surface, occlusion);

        // Surface is not occluded so it shouldn't think it is.
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), occlusion.UnoccludedContributingSurfaceContentRect(surface, false, gfx::Rect(0, 0, 100, 100), NULL));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaOccluded);

template<class Types>
class OcclusionTrackerTestSurfaceWithReplicaUnoccluded : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceWithReplicaUnoccluded(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        this->createReplicaLayer(surface, this->identityMatrix, gfx::PointF(0, 100), gfx::Size(100, 100));
        typename Types::LayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 110), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        // |topmost| occludes the surface, but not the entire surface's replica.
        this->visitLayer(topmost, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 100, 110).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->visitLayer(surface, occlusion);

        EXPECT_EQ(gfx::Rect(0, 0, 100, 110).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->enterContributingSurface(surface, occlusion);

        // Surface is occluded, but only the top 10px of the replica.
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0), occlusion.UnoccludedContributingSurfaceContentRect(surface, false, gfx::Rect(0, 0, 100, 100), NULL));
        EXPECT_RECT_EQ(gfx::Rect(0, 10, 100, 90), occlusion.UnoccludedContributingSurfaceContentRect(surface, true, gfx::Rect(0, 0, 100, 100), NULL));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceWithReplicaUnoccluded);

template<class Types>
class OcclusionTrackerTestSurfaceAndReplicaOccludedDifferently : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceAndReplicaOccludedDifferently(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        this->createReplicaLayer(surface, this->identityMatrix, gfx::PointF(0, 100), gfx::Size(100, 100));
        typename Types::LayerType* overSurface = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(40, 100), true);
        typename Types::LayerType* overReplica = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 100), gfx::Size(50, 100), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        // These occlude the surface and replica differently, so we can test each one.
        this->visitLayer(overReplica, occlusion);
        this->visitLayer(overSurface, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(UnionRegions(gfx::Rect(0, 0, 40, 100), gfx::Rect(0, 100, 50, 100)).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->visitLayer(surface, occlusion);

        EXPECT_EQ(UnionRegions(gfx::Rect(0, 0, 40, 100), gfx::Rect(0, 100, 50, 100)).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->enterContributingSurface(surface, occlusion);

        // Surface and replica are occluded different amounts.
        EXPECT_RECT_EQ(gfx::Rect(40, 0, 60, 100), occlusion.UnoccludedContributingSurfaceContentRect(surface, false, gfx::Rect(0, 0, 100, 100), NULL));
        EXPECT_RECT_EQ(gfx::Rect(50, 0, 50, 100), occlusion.UnoccludedContributingSurfaceContentRect(surface, true, gfx::Rect(0, 0, 100, 100), NULL));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceAndReplicaOccludedDifferently);

template<class Types>
class OcclusionTrackerTestSurfaceChildOfSurface : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceChildOfSurface(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        // This test verifies that the surface cliprect does not end up empty and clip away the entire unoccluded rect.

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        typename Types::LayerType* surfaceChild = this->createDrawingSurface(surface, this->identityMatrix, gfx::PointF(0, 10), gfx::Size(100, 50), true);
        typename Types::LayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 50), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(-100, -100, 1000, 1000));

        // |topmost| occludes everything partially so we know occlusion is happening at all.
        this->visitLayer(topmost, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->visitLayer(surfaceChild, occlusion);

        // surfaceChild increases the occlusion in the screen by a narrow sliver.
        EXPECT_EQ(gfx::Rect(0, -10, 100, 50).ToString(), occlusion.occlusion_from_outside_target().ToString());
        // In its own surface, surfaceChild is at 0,0 as is its occlusion.
        EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());

        // The root layer always has a clipRect. So the parent of |surface| has a clipRect. However, the owning layer for |surface| does not
        // mask to bounds, so it doesn't have a clipRect of its own. Thus the parent of |surfaceChild| exercises different code paths
        // as its parent does not have a clipRect.

        this->enterContributingSurface(surfaceChild, occlusion);
        // The surfaceChild's parent does not have a clipRect as it owns a render surface. Make sure the unoccluded rect
        // does not get clipped away inappropriately.
        EXPECT_RECT_EQ(gfx::Rect(0, 40, 100, 10), occlusion.UnoccludedContributingSurfaceContentRect(surfaceChild, false, gfx::Rect(0, 0, 100, 50), NULL));
        this->leaveContributingSurface(surfaceChild, occlusion);

        // When the surfaceChild's occlusion is transformed up to its parent, make sure it is not clipped away inappropriately also.
        this->EnterLayer(surface, occlusion);
        EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 10, 100, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());
        this->LeaveLayer(surface, occlusion);

        this->enterContributingSurface(surface, occlusion);
        // The surface's parent does have a clipRect as it is the root layer.
        EXPECT_RECT_EQ(gfx::Rect(0, 50, 100, 50), occlusion.UnoccludedContributingSurfaceContentRect(surface, false, gfx::Rect(0, 0, 100, 100), NULL));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceChildOfSurface);

template<class Types>
class OcclusionTrackerTestTopmostSurfaceIsClippedToViewport : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestTopmostSurfaceIsClippedToViewport(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        // This test verifies that the top-most surface is considered occluded outside of its target's clipRect and outside the viewport rect.

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 300), true);
        this->calcDrawEtc(parent);

        {
            // Make a viewport rect that is larger than the root layer.
            TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

            this->visitLayer(surface, occlusion);

            // The root layer always has a clipRect. So the parent of |surface| has a clipRect giving the surface itself a clipRect.
            this->enterContributingSurface(surface, occlusion);
            // Make sure the parent's clipRect clips the unoccluded region of the child surface.
            EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 200), occlusion.UnoccludedContributingSurfaceContentRect(surface, false, gfx::Rect(0, 0, 100, 300), NULL));
        }
        this->resetLayerIterator();
        {
            // Make a viewport rect that is smaller than the root layer.
            TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 100, 100));

            this->visitLayer(surface, occlusion);

            // The root layer always has a clipRect. So the parent of |surface| has a clipRect giving the surface itself a clipRect.
            this->enterContributingSurface(surface, occlusion);
            // Make sure the viewport rect clips the unoccluded region of the child surface.
            EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), occlusion.UnoccludedContributingSurfaceContentRect(surface, false, gfx::Rect(0, 0, 100, 300), NULL));
        }
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestTopmostSurfaceIsClippedToViewport);

template<class Types>
class OcclusionTrackerTestSurfaceChildOfClippingSurface : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceChildOfClippingSurface(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        // This test verifies that the surface cliprect does not end up empty and clip away the entire unoccluded rect.

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(80, 200));
        parent->SetMasksToBounds(true);
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), true);
        typename Types::LayerType* surfaceChild = this->createDrawingSurface(surface, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), false);
        typename Types::LayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 50), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        // |topmost| occludes everything partially so we know occlusion is happening at all.
        this->visitLayer(topmost, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 80, 50).ToString(), occlusion.occlusion_from_inside_target().ToString());

        // surfaceChild is not opaque and does not occlude, so we have a non-empty unoccluded area on surface.
        this->visitLayer(surfaceChild, occlusion);

        EXPECT_EQ(gfx::Rect(0, 0, 80, 50).ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(), occlusion.occlusion_from_inside_target().ToString());

        // The root layer always has a clipRect. So the parent of |surface| has a clipRect. However, the owning layer for |surface| does not
        // mask to bounds, so it doesn't have a clipRect of its own. Thus the parent of |surfaceChild| exercises different code paths
        // as its parent does not have a clipRect.

        this->enterContributingSurface(surfaceChild, occlusion);
        // The surfaceChild's parent does not have a clipRect as it owns a render surface.
        EXPECT_EQ(gfx::Rect(0, 50, 80, 50).ToString(), occlusion.UnoccludedContributingSurfaceContentRect(surfaceChild, false, gfx::Rect(0, 0, 100, 100), NULL).ToString());
        this->leaveContributingSurface(surfaceChild, occlusion);

        this->visitLayer(surface, occlusion);
        this->enterContributingSurface(surface, occlusion);
        // The surface's parent does have a clipRect as it is the root layer.
        EXPECT_EQ(gfx::Rect(0, 50, 80, 50).ToString(), occlusion.UnoccludedContributingSurfaceContentRect(surface, false, gfx::Rect(0, 0, 100, 100), NULL).ToString());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceChildOfClippingSurface);

template<class Types>
class OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform scaleByHalf;
        scaleByHalf.Scale(0.5, 0.5);

        // Make a surface and its replica, each 50x50, that are completely surrounded by opaque layers which are above them in the z-order.
        // The surface is scaled to test that the pixel moving is done in the target space, where the background filter is applied, but the surface
        // appears at 50, 50 and the replica at 200, 50.
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 150));
        typename Types::LayerType* filteredSurface = this->createDrawingLayer(parent, scaleByHalf, gfx::PointF(50, 50), gfx::Size(100, 100), false);
        this->createReplicaLayer(filteredSurface, this->identityMatrix, gfx::PointF(300, 0), gfx::Size());
        typename Types::LayerType* occludingLayer1 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 50), true);
        typename Types::LayerType* occludingLayer2 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 100), gfx::Size(300, 50), true);
        typename Types::LayerType* occludingLayer3 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 50), gfx::Size(50, 50), true);
        typename Types::LayerType* occludingLayer4 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(100, 50), gfx::Size(100, 50), true);
        typename Types::LayerType* occludingLayer5 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(250, 50), gfx::Size(50, 50), true);

        // Filters make the layer own a surface.
        WebKit::WebFilterOperations filters;
        filters.append(WebKit::WebFilterOperation::createBlurFilter(10));
        filteredSurface->SetBackgroundFilters(filters);

        // Save the distance of influence for the blur effect.
        int outsetTop, outsetRight, outsetBottom, outsetLeft;
        filters.getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        // These layers occlude pixels directly beside the filteredSurface. Because filtered surface blends pixels in a radius, it will
        // need to see some of the pixels (up to radius far) underneath the occludingLayers.
        this->visitLayer(occludingLayer5, occlusion);
        this->visitLayer(occludingLayer4, occlusion);
        this->visitLayer(occludingLayer3, occlusion);
        this->visitLayer(occludingLayer2, occlusion);
        this->visitLayer(occludingLayer1, occlusion);

        Region expectedOcclusion;
        expectedOcclusion.Union(gfx::Rect(0, 0, 300, 50));
        expectedOcclusion.Union(gfx::Rect(0, 50, 50, 50));
        expectedOcclusion.Union(gfx::Rect(100, 50, 100, 50));
        expectedOcclusion.Union(gfx::Rect(250, 50, 50, 50));
        expectedOcclusion.Union(gfx::Rect(0, 100, 300, 50));

        EXPECT_EQ(expectedOcclusion.ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());

        this->visitLayer(filteredSurface, occlusion);

        // The filtered layer/replica does not occlude.
        Region expectedOcclusionOutsideSurface;
        expectedOcclusionOutsideSurface.Union(gfx::Rect(-50, -50, 300, 50));
        expectedOcclusionOutsideSurface.Union(gfx::Rect(-50, 0, 50, 50));
        expectedOcclusionOutsideSurface.Union(gfx::Rect(50, 0, 100, 50));
        expectedOcclusionOutsideSurface.Union(gfx::Rect(200, 0, 50, 50));
        expectedOcclusionOutsideSurface.Union(gfx::Rect(-50, 50, 300, 50));

        EXPECT_EQ(expectedOcclusionOutsideSurface.ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_inside_target().ToString());

        // The surface has a background blur, so it needs pixels that are currently considered occluded in order to be drawn. So the pixels
        // it needs should be removed some the occluded area so that when we get to the parent they are drawn.
        this->visitContributingSurface(filteredSurface, occlusion);

        this->EnterLayer(parent, occlusion);

        Region expectedBlurredOcclusion;
        expectedBlurredOcclusion.Union(gfx::Rect(0, 0, 300, 50 - outsetTop));
        expectedBlurredOcclusion.Union(gfx::Rect(0, 50 - outsetTop, 50 - outsetLeft, 50 + outsetTop + outsetBottom));
        expectedBlurredOcclusion.Union(gfx::Rect(100 + outsetRight, 50 - outsetTop, 100 - outsetRight - outsetLeft, 50 + outsetTop + outsetBottom));
        expectedBlurredOcclusion.Union(gfx::Rect(250 + outsetRight, 50 - outsetTop, 50 - outsetRight, 50 + outsetTop + outsetBottom));
        expectedBlurredOcclusion.Union(gfx::Rect(0, 100 + outsetBottom, 300, 50 - outsetBottom));

        EXPECT_EQ(expectedBlurredOcclusion.ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());

        gfx::Rect outsetRect;
        gfx::Rect testRect;

        // Nothing in the blur outsets for the filteredSurface is occluded.
        outsetRect = gfx::Rect(50 - outsetLeft, 50 - outsetTop, 50 + outsetLeft + outsetRight, 50 + outsetTop + outsetBottom);
        testRect = outsetRect;
        EXPECT_EQ(outsetRect.ToString(), occlusion.unoccludedLayerContentRect(parent, testRect).ToString());

        // Stuff outside the blur outsets is still occluded though.
        testRect = outsetRect;
        testRect.Inset(0, 0, -1, 0);
        EXPECT_EQ(outsetRect.ToString(), occlusion.unoccludedLayerContentRect(parent, testRect).ToString());
        testRect = outsetRect;
        testRect.Inset(0, 0, 0, -1);
        EXPECT_EQ(outsetRect.ToString(), occlusion.unoccludedLayerContentRect(parent, testRect).ToString());
        testRect = outsetRect;
        testRect.Inset(-1, 0, 0, 0);
        EXPECT_EQ(outsetRect.ToString(), occlusion.unoccludedLayerContentRect(parent, testRect).ToString());
        testRect = outsetRect;
        testRect.Inset(0, -1, 0, 0);
        EXPECT_EQ(outsetRect.ToString(), occlusion.unoccludedLayerContentRect(parent, testRect).ToString());

        // Nothing in the blur outsets for the filteredSurface's replica is occluded.
        outsetRect = gfx::Rect(200 - outsetLeft, 50 - outsetTop, 50 + outsetLeft + outsetRight, 50 + outsetTop + outsetBottom);
        testRect = outsetRect;
        EXPECT_EQ(outsetRect.ToString(), occlusion.unoccludedLayerContentRect(parent, testRect).ToString());

        // Stuff outside the blur outsets is still occluded though.
        testRect = outsetRect;
        testRect.Inset(0, 0, -1, 0);
        EXPECT_EQ(outsetRect.ToString(), occlusion.unoccludedLayerContentRect(parent, testRect).ToString());
        testRect = outsetRect;
        testRect.Inset(0, 0, 0, -1);
        EXPECT_EQ(outsetRect.ToString(), occlusion.unoccludedLayerContentRect(parent, testRect).ToString());
        testRect = outsetRect;
        testRect.Inset(-1, 0, 0, 0);
        EXPECT_EQ(outsetRect.ToString(), occlusion.unoccludedLayerContentRect(parent, testRect).ToString());
        testRect = outsetRect;
        testRect.Inset(0, -1, 0, 0);
        EXPECT_EQ(outsetRect.ToString(), occlusion.unoccludedLayerContentRect(parent, testRect).ToString());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter);

template<class Types>
class OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform scaleByHalf;
        scaleByHalf.Scale(0.5, 0.5);

        // Makes two surfaces that completely cover |parent|. The occlusion both above and below the filters will be reduced by each of them.
        typename Types::ContentLayerType* root = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(75, 75));
        typename Types::LayerType* parent = this->createSurface(root, scaleByHalf, gfx::PointF(0, 0), gfx::Size(150, 150));
        parent->SetMasksToBounds(true);
        typename Types::LayerType* filteredSurface1 = this->createDrawingLayer(parent, scaleByHalf, gfx::PointF(0, 0), gfx::Size(300, 300), false);
        typename Types::LayerType* filteredSurface2 = this->createDrawingLayer(parent, scaleByHalf, gfx::PointF(0, 0), gfx::Size(300, 300), false);
        typename Types::LayerType* occludingLayerAbove = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(100, 100), gfx::Size(50, 50), true);

        // Filters make the layers own surfaces.
        WebKit::WebFilterOperations filters;
        filters.append(WebKit::WebFilterOperation::createBlurFilter(1));
        filteredSurface1->SetBackgroundFilters(filters);
        filteredSurface2->SetBackgroundFilters(filters);

        // Save the distance of influence for the blur effect.
        int outsetTop, outsetRight, outsetBottom, outsetLeft;
        filters.getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

        this->calcDrawEtc(root);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(occludingLayerAbove, occlusion);
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(100 / 2, 100 / 2, 50 / 2, 50 / 2).ToString(), occlusion.occlusion_from_inside_target().ToString());

        this->visitLayer(filteredSurface2, occlusion);
        this->visitContributingSurface(filteredSurface2, occlusion);
        this->visitLayer(filteredSurface1, occlusion);
        this->visitContributingSurface(filteredSurface1, occlusion);

        // Test expectations in the target.
        gfx::Rect expectedOcclusion = gfx::Rect(100 / 2 + outsetRight * 2, 100 / 2 + outsetBottom * 2, 50 / 2 - (outsetLeft + outsetRight) * 2, 50 / 2 - (outsetTop + outsetBottom) * 2);
        EXPECT_EQ(expectedOcclusion.ToString(), occlusion.occlusion_from_inside_target().ToString());

        // Test expectations in the screen are the same as in the target, as the render surface is 1:1 with the screen.
        EXPECT_EQ(expectedOcclusion.ToString(), occlusion.occlusion_from_outside_target().ToString());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice);

template<class Types>
class OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilterWithClip : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilterWithClip(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        // Make a surface and its replica, each 50x50, that are completely surrounded by opaque layers which are above them in the z-order.
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 150));
        // We stick the filtered surface inside a clipping surface so that we can make sure the clip is honored when exposing pixels for
        // the background filter.
        typename Types::LayerType* clippingSurface = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 70), false);
        clippingSurface->SetMasksToBounds(true);
        typename Types::LayerType* filteredSurface = this->createDrawingLayer(clippingSurface, this->identityMatrix, gfx::PointF(50, 50), gfx::Size(50, 50), false);
        this->createReplicaLayer(filteredSurface, this->identityMatrix, gfx::PointF(150, 0), gfx::Size());
        typename Types::LayerType* occludingLayer1 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 50), true);
        typename Types::LayerType* occludingLayer2 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 100), gfx::Size(300, 50), true);
        typename Types::LayerType* occludingLayer3 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 50), gfx::Size(50, 50), true);
        typename Types::LayerType* occludingLayer4 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(100, 50), gfx::Size(100, 50), true);
        typename Types::LayerType* occludingLayer5 = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(250, 50), gfx::Size(50, 50), true);

        // Filters make the layer own a surface. This filter is large enough that it goes outside the bottom of the clippingSurface.
        WebKit::WebFilterOperations filters;
        filters.append(WebKit::WebFilterOperation::createBlurFilter(12));
        filteredSurface->SetBackgroundFilters(filters);

        // Save the distance of influence for the blur effect.
        int outsetTop, outsetRight, outsetBottom, outsetLeft;
        filters.getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        // These layers occlude pixels directly beside the filteredSurface. Because filtered surface blends pixels in a radius, it will
        // need to see some of the pixels (up to radius far) underneath the occludingLayers.
        this->visitLayer(occludingLayer5, occlusion);
        this->visitLayer(occludingLayer4, occlusion);
        this->visitLayer(occludingLayer3, occlusion);
        this->visitLayer(occludingLayer2, occlusion);
        this->visitLayer(occludingLayer1, occlusion);

        Region expectedOcclusion;
        expectedOcclusion.Union(gfx::Rect(0, 0, 300, 50));
        expectedOcclusion.Union(gfx::Rect(0, 50, 50, 50));
        expectedOcclusion.Union(gfx::Rect(100, 50, 100, 50));
        expectedOcclusion.Union(gfx::Rect(250, 50, 50, 50));
        expectedOcclusion.Union(gfx::Rect(0, 100, 300, 50));

        EXPECT_EQ(expectedOcclusion.ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());

        // Everything outside the surface/replica is occluded but the surface/replica itself is not.
        this->visitLayer(filteredSurface, occlusion);

        // The filtered layer/replica does not occlude.
        Region expectedOcclusionOutsideSurface;
        expectedOcclusionOutsideSurface.Union(gfx::Rect(-50, -50, 300, 50));
        expectedOcclusionOutsideSurface.Union(gfx::Rect(-50, 0, 50, 50));
        expectedOcclusionOutsideSurface.Union(gfx::Rect(50, 0, 100, 50));
        expectedOcclusionOutsideSurface.Union(gfx::Rect(200, 0, 50, 50));
        expectedOcclusionOutsideSurface.Union(gfx::Rect(-50, 50, 300, 50));

        EXPECT_EQ(expectedOcclusionOutsideSurface.ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_inside_target().ToString());

        // The surface has a background blur, so it needs pixels that are currently considered occluded in order to be drawn. So the pixels
        // it needs should be removed some the occluded area so that when we get to the parent they are drawn.
        this->visitContributingSurface(filteredSurface, occlusion);

        this->visitLayer(clippingSurface, occlusion);
        this->enterContributingSurface(clippingSurface, occlusion);

        Region expectedBlurredOcclusion;
        expectedBlurredOcclusion.Union(gfx::Rect(0, 0, 300, 50 - outsetTop));
        expectedBlurredOcclusion.Union(gfx::Rect(0, 50 - outsetTop, 50 - outsetLeft, 20 + outsetTop + outsetBottom));
        expectedBlurredOcclusion.Union(gfx::Rect(100 + outsetRight, 50 - outsetTop, 100 - outsetRight - outsetLeft, 20 + outsetTop + outsetBottom));
        expectedBlurredOcclusion.Union(gfx::Rect(250 + outsetRight, 50 - outsetTop, 50 - outsetRight, 20 + outsetTop + outsetBottom));
        expectedBlurredOcclusion.Union(gfx::Rect(0, 100 + 5, 300, 50 - 5));

        EXPECT_EQ(expectedBlurredOcclusion.ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_inside_target().ToString());

        gfx::Rect outsetRect;
        gfx::Rect clippedOutsetRect;
        gfx::Rect testRect;

        // Nothing in the (clipped) blur outsets for the filteredSurface is occluded.
        outsetRect = gfx::Rect(50 - outsetLeft, 50 - outsetTop, 50 + outsetLeft + outsetRight, 50 + outsetTop + outsetBottom);
        clippedOutsetRect = outsetRect;
        clippedOutsetRect.Intersect(gfx::Rect(0 - outsetLeft, 0 - outsetTop, 300 + outsetLeft + outsetRight, 70 + outsetTop + outsetBottom));
        clippedOutsetRect.Intersect(gfx::Rect(0, 0, 300, 70));
        testRect = outsetRect;
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));

        // Stuff outside the (clipped) blur outsets is still occluded though.
        testRect = outsetRect;
        testRect.Inset(0, 0, -1, 0);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.Inset(0, 0, 0, -1);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.Inset(-1, 0, 0, 0);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.Inset(0, -1, 0, 0);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));

        // Nothing in the (clipped) blur outsets for the filteredSurface's replica is occluded.
        outsetRect = gfx::Rect(200 - outsetLeft, 50 - outsetTop, 50 + outsetLeft + outsetRight, 50 + outsetTop + outsetBottom);
        clippedOutsetRect = outsetRect;
        clippedOutsetRect.Intersect(gfx::Rect(0 - outsetLeft, 0 - outsetTop, 300 + outsetLeft + outsetRight, 70 + outsetTop + outsetBottom));
        clippedOutsetRect.Intersect(gfx::Rect(0, 0, 300, 70));
        testRect = outsetRect;
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));

        // Stuff outside the (clipped) blur outsets is still occluded though.
        testRect = outsetRect;
        testRect.Inset(0, 0, -1, 0);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.Inset(0, 0, 0, -1);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.Inset(-1, 0, 0, 0);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.Inset(0, -1, 0, 0);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilterWithClip);

template<class Types>
class OcclusionTrackerTestDontReduceOcclusionBelowBackgroundFilter : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestDontReduceOcclusionBelowBackgroundFilter(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform scaleByHalf;
        scaleByHalf.Scale(0.5, 0.5);

        // Make a surface and its replica, each 50x50, with a smaller 30x30 layer centered below each.
        // The surface is scaled to test that the pixel moving is done in the target space, where the background filter is applied, but the surface
        // appears at 50, 50 and the replica at 200, 50.
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 150));
        typename Types::LayerType* behindSurfaceLayer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(60, 60), gfx::Size(30, 30), true);
        typename Types::LayerType* behindReplicaLayer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(210, 60), gfx::Size(30, 30), true);
        typename Types::LayerType* filteredSurface = this->createDrawingLayer(parent, scaleByHalf, gfx::PointF(50, 50), gfx::Size(100, 100), false);
        this->createReplicaLayer(filteredSurface, this->identityMatrix, gfx::PointF(300, 0), gfx::Size());

        // Filters make the layer own a surface.
        WebKit::WebFilterOperations filters;
        filters.append(WebKit::WebFilterOperation::createBlurFilter(3));
        filteredSurface->SetBackgroundFilters(filters);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        // The surface has a background blur, so it blurs non-opaque pixels below it.
        this->visitLayer(filteredSurface, occlusion);
        this->visitContributingSurface(filteredSurface, occlusion);

        this->visitLayer(behindReplicaLayer, occlusion);
        this->visitLayer(behindSurfaceLayer, occlusion);

        // The layers behind the surface are not blurred, and their occlusion does not change, until we leave the surface.
        // So it should not be modified by the filter here.
        gfx::Rect occlusionBehindSurface = gfx::Rect(60, 60, 30, 30);
        gfx::Rect occlusionBehindReplica = gfx::Rect(210, 60, 30, 30);

        Region expectedOpaqueBounds = UnionRegions(occlusionBehindSurface, occlusionBehindReplica);
        EXPECT_EQ(expectedOpaqueBounds.ToString(), occlusion.occlusion_from_inside_target().ToString());

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestDontReduceOcclusionBelowBackgroundFilter);

template<class Types>
class OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform scaleByHalf;
        scaleByHalf.Scale(0.5, 0.5);

        // Make a surface and its replica, each 50x50, that are completely occluded by opaque layers which are above them in the z-order.
        // The surface is scaled to test that the pixel moving is done in the target space, where the background filter is applied, but the surface
        // appears at 50, 50 and the replica at 200, 50.
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 150));
        typename Types::LayerType* filteredSurface = this->createDrawingLayer(parent, scaleByHalf, gfx::PointF(50, 50), gfx::Size(100, 100), false);
        this->createReplicaLayer(filteredSurface, this->identityMatrix, gfx::PointF(300, 0), gfx::Size());
        typename Types::LayerType* aboveSurfaceLayer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(50, 50), gfx::Size(50, 50), true);
        typename Types::LayerType* aboveReplicaLayer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(200, 50), gfx::Size(50, 50), true);

        // Filters make the layer own a surface.
        WebKit::WebFilterOperations filters;
        filters.append(WebKit::WebFilterOperation::createBlurFilter(3));
        filteredSurface->SetBackgroundFilters(filters);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(aboveReplicaLayer, occlusion);
        this->visitLayer(aboveSurfaceLayer, occlusion);

        this->visitLayer(filteredSurface, occlusion);

        {
            // The layers above the filtered surface occlude from outside.
            gfx::Rect occlusionAboveSurface = gfx::Rect(0, 0, 50, 50);
            gfx::Rect occlusionAboveReplica = gfx::Rect(150, 0, 50, 50);
            Region expectedOpaqueRegion = UnionRegions(occlusionAboveSurface, occlusionAboveReplica);

            EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_inside_target().ToString());
            EXPECT_EQ(expectedOpaqueRegion.ToString(), occlusion.occlusion_from_outside_target().ToString());
        }

        // The surface has a background blur, so it blurs non-opaque pixels below it.
        this->visitContributingSurface(filteredSurface, occlusion);

        {
            // The filter is completely occluded, so it should not blur anything and reduce any occlusion.
            gfx::Rect occlusionAboveSurface = gfx::Rect(50, 50, 50, 50);
            gfx::Rect occlusionAboveReplica = gfx::Rect(200, 50, 50, 50);
            Region expectedOpaqueRegion = UnionRegions(occlusionAboveSurface, occlusionAboveReplica);

            EXPECT_EQ(expectedOpaqueRegion.ToString(), occlusion.occlusion_from_inside_target().ToString());
            EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        }
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded);

template<class Types>
class OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Transform scaleByHalf;
        scaleByHalf.Scale(0.5, 0.5);

        // Make a surface and its replica, each 50x50, that are partially occluded by opaque layers which are above them in the z-order.
        // The surface is scaled to test that the pixel moving is done in the target space, where the background filter is applied, but the surface
        // appears at 50, 50 and the replica at 200, 50.
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(300, 150));
        typename Types::LayerType* filteredSurface = this->createDrawingLayer(parent, scaleByHalf, gfx::PointF(50, 50), gfx::Size(100, 100), false);
        this->createReplicaLayer(filteredSurface, this->identityMatrix, gfx::PointF(300, 0), gfx::Size());
        typename Types::LayerType* aboveSurfaceLayer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(70, 50), gfx::Size(30, 50), true);
        typename Types::LayerType* aboveReplicaLayer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(200, 50), gfx::Size(30, 50), true);
        typename Types::LayerType* besideSurfaceLayer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(90, 40), gfx::Size(10, 10), true);
        typename Types::LayerType* besideReplicaLayer = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(200, 40), gfx::Size(10, 10), true);

        // Filters make the layer own a surface.
        WebKit::WebFilterOperations filters;
        filters.append(WebKit::WebFilterOperation::createBlurFilter(3));
        filteredSurface->SetBackgroundFilters(filters);

        // Save the distance of influence for the blur effect.
        int outsetTop, outsetRight, outsetBottom, outsetLeft;
        filters.getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->visitLayer(besideReplicaLayer, occlusion);
        this->visitLayer(besideSurfaceLayer, occlusion);
        this->visitLayer(aboveReplicaLayer, occlusion);
        this->visitLayer(aboveSurfaceLayer, occlusion);

        // The surface has a background blur, so it blurs non-opaque pixels below it.
        this->visitLayer(filteredSurface, occlusion);
        this->visitContributingSurface(filteredSurface, occlusion);

        // The filter in the surface and replica are partially unoccluded. Only the unoccluded parts should reduce occlusion.
        // This means it will push back the occlusion that touches the unoccluded part (occlusionAbove___), but it will not
        // touch occlusionBeside____ since that is not beside the unoccluded part of the surface, even though it is beside
        // the occluded part of the surface.
        gfx::Rect occlusionAboveSurface = gfx::Rect(70 + outsetRight, 50, 30 - outsetRight, 50);
        gfx::Rect occlusionAboveReplica = gfx::Rect(200, 50, 30 - outsetLeft, 50);
        gfx::Rect occlusionBesideSurface = gfx::Rect(90, 40, 10, 10);
        gfx::Rect occlusionBesideReplica = gfx::Rect(200, 40, 10, 10);

        Region expectedOcclusion;
        expectedOcclusion.Union(occlusionAboveSurface);
        expectedOcclusion.Union(occlusionAboveReplica);
        expectedOcclusion.Union(occlusionBesideSurface);
        expectedOcclusion.Union(occlusionBesideReplica);

        ASSERT_EQ(expectedOcclusion.ToString(), occlusion.occlusion_from_inside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());

        Region::Iterator expectedRects(expectedOcclusion);
        Region::Iterator targetSurfaceRects(occlusion.occlusion_from_inside_target());
        for (; expectedRects.has_rect(); expectedRects.next(), targetSurfaceRects.next()) {
            ASSERT_TRUE(targetSurfaceRects.has_rect());
            EXPECT_EQ(expectedRects.rect(), targetSurfaceRects.rect());
        }
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded);

template<class Types>
class OcclusionTrackerTestMinimumTrackingSize : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestMinimumTrackingSize(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        gfx::Size trackingSize(100, 100);
        gfx::Size belowTrackingSize(99, 99);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(400, 400));
        typename Types::LayerType* large = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), trackingSize, true);
        typename Types::LayerType* small = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), belowTrackingSize, true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));
        occlusion.set_minimum_tracking_size(trackingSize);

        // The small layer is not tracked because it is too small.
        this->visitLayer(small, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_inside_target().ToString());

        // The large layer is tracked as it is large enough.
        this->visitLayer(large, occlusion);

        EXPECT_EQ(gfx::Rect().ToString(), occlusion.occlusion_from_outside_target().ToString());
        EXPECT_EQ(gfx::Rect(gfx::Point(), trackingSize).ToString(), occlusion.occlusion_from_inside_target().ToString());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestMinimumTrackingSize);

template<class Types>
class OcclusionTrackerTestViewportClipIsExternalOcclusion : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestViewportClipIsExternalOcclusion(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(400, 400));
        typename Types::LayerType* small = this->createDrawingSurface(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200), false);
        typename Types::LayerType* large = this->createDrawingLayer(small, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(400, 400), false);
        small->SetMasksToBounds(true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 100, 100));

        this->EnterLayer(large, occlusion);

        bool hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), occlusion.unoccludedLayerContentRect(large, gfx::Rect(0, 0, 400, 400), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_TRUE(hasOcclusionFromOutsideTargetSurface);

        hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_FALSE(occlusion.occludedLayer(large, gfx::Rect(0, 0, 400, 400), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_TRUE(hasOcclusionFromOutsideTargetSurface);

        this->LeaveLayer(large, occlusion);
        this->visitLayer(small, occlusion);

        hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), occlusion.unoccludedLayerContentRect(small, gfx::Rect(0, 0, 200, 200), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_TRUE(hasOcclusionFromOutsideTargetSurface);

        hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_FALSE(occlusion.occludedLayer(small, gfx::Rect(0, 0, 200, 200), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_TRUE(hasOcclusionFromOutsideTargetSurface);

        this->enterContributingSurface(small, occlusion);

        hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), occlusion.UnoccludedContributingSurfaceContentRect(small, false, gfx::Rect(0, 0, 200, 200), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_TRUE(hasOcclusionFromOutsideTargetSurface);
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestViewportClipIsExternalOcclusion)

template<class Types>
class OcclusionTrackerTestLayerClipIsExternalOcclusion : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerClipIsExternalOcclusion(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, gfx::PointF(0, 0), gfx::Size(400, 400));
        typename Types::LayerType* smallest = this->createDrawingLayer(parent, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(50, 50), false);
        typename Types::LayerType* smaller = this->createDrawingSurface(smallest, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(100, 100), false);
        typename Types::LayerType* small = this->createDrawingSurface(smaller, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(200, 200), false);
        typename Types::LayerType* large = this->createDrawingLayer(small, this->identityMatrix, gfx::PointF(0, 0), gfx::Size(400, 400), false);
        smallest->SetMasksToBounds(true);
        smaller->SetMasksToBounds(true);
        small->SetMasksToBounds(true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(gfx::Rect(0, 0, 1000, 1000));

        this->EnterLayer(large, occlusion);

        // Clipping from the smaller layer is from outside the target surface.
        bool hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), occlusion.unoccludedLayerContentRect(large, gfx::Rect(0, 0, 400, 400), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_TRUE(hasOcclusionFromOutsideTargetSurface);

        hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_FALSE(occlusion.occludedLayer(large, gfx::Rect(0, 0, 400, 400), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_TRUE(hasOcclusionFromOutsideTargetSurface);

        this->LeaveLayer(large, occlusion);
        this->visitLayer(small, occlusion);

        // Clipping from the smaller layer is from outside the target surface.
        hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100), occlusion.unoccludedLayerContentRect(small, gfx::Rect(0, 0, 200, 200), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_TRUE(hasOcclusionFromOutsideTargetSurface);

        hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_FALSE(occlusion.occludedLayer(small, gfx::Rect(0, 0, 200, 200), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_TRUE(hasOcclusionFromOutsideTargetSurface);

        this->enterContributingSurface(small, occlusion);

        // The |small| surface is clipped from outside its target by |smallest|.
        hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), occlusion.UnoccludedContributingSurfaceContentRect(small, false, gfx::Rect(0, 0, 200, 200), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_TRUE(hasOcclusionFromOutsideTargetSurface);

        this->leaveContributingSurface(small, occlusion);
        this->visitLayer(smaller, occlusion);
        this->enterContributingSurface(smaller, occlusion);

        // The |smaller| surface is clipped from inside its target by |smallest|.
        hasOcclusionFromOutsideTargetSurface = false;
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 50, 50), occlusion.UnoccludedContributingSurfaceContentRect(smaller, false, gfx::Rect(0, 0, 100, 100), &hasOcclusionFromOutsideTargetSurface));
        EXPECT_FALSE(hasOcclusionFromOutsideTargetSurface);
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestLayerClipIsExternalOcclusion)

}  // namespace
}  // namespace cc
