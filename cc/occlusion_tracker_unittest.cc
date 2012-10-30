// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/occlusion_tracker.h"

#include "Region.h"
#include "cc/layer.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_impl.h"
#include "cc/layer_tree_host_common.h"
#include "cc/math_util.h"
#include "cc/overdraw_metrics.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>
#include <public/WebTransformationMatrix.h>

using namespace cc;
using namespace WebKit;
using namespace WebKitTests;

namespace {

class TestContentLayer : public Layer {
public:
    TestContentLayer()
        : Layer()
        , m_overrideOpaqueContentsRect(false)
    {
    }

    virtual bool drawsContent() const OVERRIDE { return true; }
    virtual Region visibleContentOpaqueRegion() const OVERRIDE
    {
        if (m_overrideOpaqueContentsRect)
            return intersection(m_opaqueContentsRect, visibleContentRect());
        return Layer::visibleContentOpaqueRegion();
    }
    void setOpaqueContentsRect(const IntRect& opaqueContentsRect)
    {
        m_overrideOpaqueContentsRect = true;
        m_opaqueContentsRect = opaqueContentsRect;
    }

private:
    virtual ~TestContentLayer()
    {
    }

    bool m_overrideOpaqueContentsRect;
    IntRect m_opaqueContentsRect;
};

class TestContentLayerImpl : public LayerImpl {
public:
    TestContentLayerImpl(int id)
        : LayerImpl(id)
        , m_overrideOpaqueContentsRect(false)
    {
        setDrawsContent(true);
    }

    virtual Region visibleContentOpaqueRegion() const OVERRIDE
    {
        if (m_overrideOpaqueContentsRect)
            return intersection(m_opaqueContentsRect, visibleContentRect());
        return LayerImpl::visibleContentOpaqueRegion();
    }
    void setOpaqueContentsRect(const IntRect& opaqueContentsRect)
    {
        m_overrideOpaqueContentsRect = true;
        m_opaqueContentsRect = opaqueContentsRect;
    }

private:
    bool m_overrideOpaqueContentsRect;
    IntRect m_opaqueContentsRect;
};

static inline bool layerImplDrawTransformIsUnknown(const Layer* layer) { return layer->drawTransformIsAnimating(); }
static inline bool layerImplDrawTransformIsUnknown(const LayerImpl*) { return false; }

template<typename LayerType, typename RenderSurfaceType>
class TestOcclusionTrackerWithClip : public TestOcclusionTrackerBase<LayerType, RenderSurfaceType> {
public:
    TestOcclusionTrackerWithClip(IntRect viewportRect, bool recordMetricsForFrame = false)
        : TestOcclusionTrackerBase<LayerType, RenderSurfaceType>(viewportRect, recordMetricsForFrame)
        , m_overrideLayerClipRect(false)
    {
    }

    void setLayerClipRect(const IntRect& rect) { m_overrideLayerClipRect = true; m_layerClipRect = rect;}
    void useDefaultLayerClipRect() { m_overrideLayerClipRect = false; }
    // Returns true if the given rect in content space for the layer is fully occluded in either screen space or the layer's target surface.
    bool occludedLayer(const LayerType* layer, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface = 0) const
    {
        return this->occluded(layer->renderTarget(), contentRect, layer->drawTransform(), layerImplDrawTransformIsUnknown(layer), layerClipRectInTarget(layer), hasOcclusionFromOutsideTargetSurface);
    }
    // Gives an unoccluded sub-rect of |contentRect| in the content space of the layer. Simple wrapper around unoccludedContentRect.
    IntRect unoccludedLayerContentRect(const LayerType* layer, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface = 0) const
    {
        return this->unoccludedContentRect(layer->renderTarget(), contentRect, layer->drawTransform(), layerImplDrawTransformIsUnknown(layer), layerClipRectInTarget(layer), hasOcclusionFromOutsideTargetSurface);
    }


protected:
    virtual IntRect layerClipRectInTarget(const LayerType* layer) const { return m_overrideLayerClipRect ? m_layerClipRect : OcclusionTrackerBase<LayerType, RenderSurfaceType>::layerClipRectInTarget(layer); }

private:
    bool m_overrideLayerClipRect;
    IntRect m_layerClipRect;
};

struct OcclusionTrackerTestMainThreadTypes {
    typedef Layer LayerType;
    typedef RenderSurface RenderSurfaceType;
    typedef TestContentLayer ContentLayerType;
    typedef scoped_refptr<Layer> LayerPtrType;
    typedef scoped_refptr<ContentLayerType> ContentLayerPtrType;
    typedef LayerIterator<Layer, std::vector<scoped_refptr<Layer> >, RenderSurface, LayerIteratorActions::FrontToBack> TestLayerIterator;
    typedef OcclusionTracker OcclusionTrackerType;

    static LayerPtrType createLayer()
    {
        return Layer::create();
    }
    static ContentLayerPtrType createContentLayer() { return make_scoped_refptr(new ContentLayerType()); }

    static LayerPtrType passLayerPtr(ContentLayerPtrType& layer)
    {
        return layer.release();
    }

    static LayerPtrType passLayerPtr(LayerPtrType& layer)
    {
        return layer.release();
    }

    static void destroyLayer(LayerPtrType& layer)
    {
        layer = NULL;
    }
};

struct OcclusionTrackerTestImplThreadTypes {
    typedef LayerImpl LayerType;
    typedef RenderSurfaceImpl RenderSurfaceType;
    typedef TestContentLayerImpl ContentLayerType;
    typedef scoped_ptr<LayerImpl> LayerPtrType;
    typedef scoped_ptr<ContentLayerType> ContentLayerPtrType;
    typedef LayerIterator<LayerImpl, std::vector<LayerImpl*>, RenderSurfaceImpl, LayerIteratorActions::FrontToBack> TestLayerIterator;
    typedef OcclusionTrackerImpl OcclusionTrackerType;

    static LayerPtrType createLayer() { return LayerImpl::create(nextLayerImplId++); }
    static ContentLayerPtrType createContentLayer() { return make_scoped_ptr(new ContentLayerType(nextLayerImplId++)); }
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
        : m_opaqueLayers(opaqueLayers)
    { }

    virtual void runMyTest() = 0;

    virtual void TearDown()
    {
        Types::destroyLayer(m_root);
        m_renderSurfaceLayerList.clear();
        m_renderSurfaceLayerListImpl.clear();
        m_replicaLayers.clear();
        m_maskLayers.clear();
        LayerTreeHost::setNeedsFilterContext(false);
    }

    typename Types::ContentLayerType* createRoot(const WebTransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds)
    {
        typename Types::ContentLayerPtrType layer(Types::createContentLayer());
        typename Types::ContentLayerType* layerPtr = layer.get();
        setProperties(layerPtr, transform, position, bounds);

        DCHECK(!m_root);
        m_root = Types::passLayerPtr(layer);
        return layerPtr;
    }

    typename Types::LayerType* createLayer(typename Types::LayerType* parent, const WebTransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds)
    {
        typename Types::LayerPtrType layer(Types::createLayer());
        typename Types::LayerType* layerPtr = layer.get();
        setProperties(layerPtr, transform, position, bounds);
        parent->addChild(Types::passLayerPtr(layer));
        return layerPtr;
    }

    typename Types::LayerType* createSurface(typename Types::LayerType* parent, const WebTransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds)
    {
        typename Types::LayerType* layer = createLayer(parent, transform, position, bounds);
        WebFilterOperations filters;
        filters.append(WebFilterOperation::createGrayscaleFilter(0.5));
        layer->setFilters(filters);
        return layer;
    }

    typename Types::ContentLayerType* createDrawingLayer(typename Types::LayerType* parent, const WebTransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds, bool opaque)
    {
        typename Types::ContentLayerPtrType layer(Types::createContentLayer());
        typename Types::ContentLayerType* layerPtr = layer.get();
        setProperties(layerPtr, transform, position, bounds);

        if (m_opaqueLayers)
            layerPtr->setContentsOpaque(opaque);
        else {
            layerPtr->setContentsOpaque(false);
            if (opaque)
                layerPtr->setOpaqueContentsRect(IntRect(IntPoint(), bounds));
            else
                layerPtr->setOpaqueContentsRect(IntRect());
        }

        parent->addChild(Types::passLayerPtr(layer));
        return layerPtr;
    }

    typename Types::LayerType* createReplicaLayer(typename Types::LayerType* owningLayer, const WebTransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds)
    {
        typename Types::ContentLayerPtrType layer(Types::createContentLayer());
        typename Types::ContentLayerType* layerPtr = layer.get();
        setProperties(layerPtr, transform, position, bounds);
        setReplica(owningLayer, Types::passLayerPtr(layer));
        return layerPtr;
    }

    typename Types::LayerType* createMaskLayer(typename Types::LayerType* owningLayer, const IntSize& bounds)
    {
        typename Types::ContentLayerPtrType layer(Types::createContentLayer());
        typename Types::ContentLayerType* layerPtr = layer.get();
        setProperties(layerPtr, identityMatrix, FloatPoint(), bounds);
        setMask(owningLayer, Types::passLayerPtr(layer));
        return layerPtr;
    }

    typename Types::ContentLayerType* createDrawingSurface(typename Types::LayerType* parent, const WebTransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds, bool opaque)
    {
        typename Types::ContentLayerType* layer = createDrawingLayer(parent, transform, position, bounds, opaque);
        WebFilterOperations filters;
        filters.append(WebFilterOperation::createGrayscaleFilter(0.5));
        layer->setFilters(filters);
        return layer;
    }

    void calcDrawEtc(TestContentLayerImpl* root)
    {
        DCHECK(root == m_root.get());
        int dummyMaxTextureSize = 512;
        LayerSorter layerSorter;

        DCHECK(!root->renderSurface());

        LayerTreeHostCommon::calculateDrawTransforms(root, root->bounds(), 1, 1, &layerSorter, dummyMaxTextureSize, m_renderSurfaceLayerListImpl);

        m_layerIterator = m_layerIteratorBegin = Types::TestLayerIterator::begin(&m_renderSurfaceLayerListImpl);
    }

    void calcDrawEtc(TestContentLayer* root)
    {
        DCHECK(root == m_root.get());
        int dummyMaxTextureSize = 512;

        DCHECK(!root->renderSurface());

        LayerTreeHostCommon::calculateDrawTransforms(root, root->bounds(), 1, 1, dummyMaxTextureSize, m_renderSurfaceLayerList);

        m_layerIterator = m_layerIteratorBegin = Types::TestLayerIterator::begin(&m_renderSurfaceLayerList);
    }

    void enterLayer(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        ASSERT_EQ(layer, *m_layerIterator);
        ASSERT_TRUE(m_layerIterator.representsItself());
        occlusion.enterLayer(m_layerIterator);
    }

    void leaveLayer(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        ASSERT_EQ(layer, *m_layerIterator);
        ASSERT_TRUE(m_layerIterator.representsItself());
        occlusion.leaveLayer(m_layerIterator);
        ++m_layerIterator;
    }

    void visitLayer(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        enterLayer(layer, occlusion);
        leaveLayer(layer, occlusion);
    }

    void enterContributingSurface(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        ASSERT_EQ(layer, *m_layerIterator);
        ASSERT_TRUE(m_layerIterator.representsTargetRenderSurface());
        occlusion.enterLayer(m_layerIterator);
        occlusion.leaveLayer(m_layerIterator);
        ++m_layerIterator;
        ASSERT_TRUE(m_layerIterator.representsContributingRenderSurface());
        occlusion.enterLayer(m_layerIterator);
    }

    void leaveContributingSurface(typename Types::LayerType* layer, typename Types::OcclusionTrackerType& occlusion)
    {
        ASSERT_EQ(layer, *m_layerIterator);
        ASSERT_TRUE(m_layerIterator.representsContributingRenderSurface());
        occlusion.leaveLayer(m_layerIterator);
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

    const WebTransformationMatrix identityMatrix;

private:
    void setBaseProperties(typename Types::LayerType* layer, const WebTransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds)
    {
        layer->setTransform(transform);
        layer->setSublayerTransform(WebTransformationMatrix());
        layer->setAnchorPoint(FloatPoint(0, 0));
        layer->setPosition(position);
        layer->setBounds(bounds);
    }

    void setProperties(Layer* layer, const WebTransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds)
    {
        setBaseProperties(layer, transform, position, bounds);
    }

    void setProperties(LayerImpl* layer, const WebTransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds)
    {
        setBaseProperties(layer, transform, position, bounds);

        layer->setContentBounds(layer->bounds());
    }

    void setReplica(Layer* owningLayer, scoped_refptr<Layer> layer)
    {
        owningLayer->setReplicaLayer(layer.get());
        m_replicaLayers.push_back(layer);
    }

    void setReplica(LayerImpl* owningLayer, scoped_ptr<LayerImpl> layer)
    {
        owningLayer->setReplicaLayer(layer.Pass());
    }

    void setMask(Layer* owningLayer, scoped_refptr<Layer> layer)
    {
        owningLayer->setMaskLayer(layer.get());
        m_maskLayers.push_back(layer);
    }

    void setMask(LayerImpl* owningLayer, scoped_ptr<LayerImpl> layer)
    {
        owningLayer->setMaskLayer(layer.Pass());
    }

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
        DebugScopedSetImplThread impl; \
    public: \
        ClassName##ImplThreadOpaqueLayers() : ClassName<OcclusionTrackerTestImplThreadTypes>(true) { } \
    }; \
    TEST_F(ClassName##ImplThreadOpaqueLayers, runTest) { runMyTest(); }
#define RUN_TEST_IMPL_THREAD_OPAQUE_PAINTS(ClassName) \
    class ClassName##ImplThreadOpaquePaints : public ClassName<OcclusionTrackerTestImplThreadTypes> { \
        DebugScopedSetImplThread impl; \
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
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(30, 30), IntSize(500, 500), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->visitLayer(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(29, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 29, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(31, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 31, 70, 70)));

        occlusion.useDefaultLayerClipRect();
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(29, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 29, 70, 70)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(31, 30, 70, 70)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 31, 70, 70)));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(30, 30, 70, 70)).isEmpty());
        EXPECT_RECT_EQ(IntRect(29, 30, 1, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(29, 30, 70, 70)));
        EXPECT_RECT_EQ(IntRect(29, 29, 70, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(29, 29, 70, 70)));
        EXPECT_RECT_EQ(IntRect(30, 29, 70, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 29, 70, 70)));
        EXPECT_RECT_EQ(IntRect(31, 29, 70, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(31, 29, 70, 70)));
        EXPECT_RECT_EQ(IntRect(100, 30, 1, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(31, 30, 70, 70)));
        EXPECT_RECT_EQ(IntRect(31, 31, 70, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(31, 31, 70, 70)));
        EXPECT_RECT_EQ(IntRect(30, 100, 70, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 31, 70, 70)));
        EXPECT_RECT_EQ(IntRect(29, 31, 70, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(29, 31, 70, 70)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestIdentityTransforms);

template<class Types>
class OcclusionTrackerTestQuadsMismatchLayer : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestQuadsMismatchLayer(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix layerTransform;
        layerTransform.translate(10, 10);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        typename Types::ContentLayerType* layer1 = this->createDrawingLayer(parent, layerTransform, FloatPoint(0, 0), IntSize(90, 90), true);
        typename Types::ContentLayerType* layer2 = this->createDrawingLayer(layer1, layerTransform, FloatPoint(0, 0), IntSize(50, 50), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));

        this->visitLayer(layer2, occlusion);
        this->enterLayer(layer1, occlusion);

        EXPECT_RECT_EQ(IntRect(20, 20, 50, 50), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(20, 20, 50, 50), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        // This checks cases where the quads don't match their "containing"
        // layers, e.g. in terms of transforms or clip rect. This is typical for
        // DelegatedRendererLayer.

        WebTransformationMatrix quadTransform;
        quadTransform.translate(30, 30);
        IntRect clipRectInTarget(0, 0, 100, 100);

        EXPECT_TRUE(occlusion.unoccludedContentRect(parent, IntRect(0, 0, 10, 10), quadTransform, false, clipRectInTarget).isEmpty());
        EXPECT_RECT_EQ(IntRect(0, 0, 10, 10), occlusion.unoccludedContentRect(parent, IntRect(0, 0, 10, 10), quadTransform, true, clipRectInTarget));
        EXPECT_RECT_EQ(IntRect(40, 40, 10, 10), occlusion.unoccludedContentRect(parent, IntRect(40, 40, 10, 10), quadTransform, false, clipRectInTarget));
        EXPECT_RECT_EQ(IntRect(40, 30, 5, 10), occlusion.unoccludedContentRect(parent, IntRect(35, 30, 10, 10), quadTransform, false, clipRectInTarget));
        EXPECT_RECT_EQ(IntRect(40, 40, 5, 5), occlusion.unoccludedContentRect(parent, IntRect(40, 40, 10, 10), quadTransform, false, IntRect(0, 0, 75, 75)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestQuadsMismatchLayer);

template<class Types>
class OcclusionTrackerTestRotatedChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestRotatedChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix layerTransform;
        layerTransform.translate(250, 250);
        layerTransform.rotate(90);
        layerTransform.translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, layerTransform, FloatPoint(30, 30), IntSize(500, 500), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->visitLayer(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(29, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 29, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(31, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 31, 70, 70)));

        occlusion.useDefaultLayerClipRect();
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(29, 30, 70, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 29, 70, 70)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(31, 30, 70, 70)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 31, 70, 70)));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(30, 30, 70, 70)).isEmpty());
        EXPECT_RECT_EQ(IntRect(29, 30, 1, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(29, 30, 70, 70)));
        EXPECT_RECT_EQ(IntRect(29, 29, 70, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(29, 29, 70, 70)));
        EXPECT_RECT_EQ(IntRect(30, 29, 70, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 29, 70, 70)));
        EXPECT_RECT_EQ(IntRect(31, 29, 70, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(31, 29, 70, 70)));
        EXPECT_RECT_EQ(IntRect(100, 30, 1, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(31, 30, 70, 70)));
        EXPECT_RECT_EQ(IntRect(31, 31, 70, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(31, 31, 70, 70)));
        EXPECT_RECT_EQ(IntRect(30, 100, 70, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 31, 70, 70)));
        EXPECT_RECT_EQ(IntRect(29, 31, 70, 70), occlusion.unoccludedLayerContentRect(parent, IntRect(29, 31, 70, 70)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestRotatedChild);

template<class Types>
class OcclusionTrackerTestTranslatedChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestTranslatedChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix layerTransform;
        layerTransform.translate(20, 20);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, layerTransform, FloatPoint(30, 30), IntSize(500, 500), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->visitLayer(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_RECT_EQ(IntRect(50, 50, 50, 50), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(50, 50, 50, 50), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(50, 50, 50, 50)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(49, 50, 50, 50)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(50, 49, 50, 50)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(51, 50, 50, 50)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(50, 51, 50, 50)));

        occlusion.useDefaultLayerClipRect();
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(50, 50, 50, 50)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(49, 50, 50, 50)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(50, 49, 50, 50)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(51, 50, 50, 50)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(50, 51, 50, 50)));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(50, 50, 50, 50)).isEmpty());
        EXPECT_RECT_EQ(IntRect(49, 50, 1, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(49, 50, 50, 50)));
        EXPECT_RECT_EQ(IntRect(49, 49, 50, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(49, 49, 50, 50)));
        EXPECT_RECT_EQ(IntRect(50, 49, 50, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(50, 49, 50, 50)));
        EXPECT_RECT_EQ(IntRect(51, 49, 50, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(51, 49, 50, 50)));
        EXPECT_RECT_EQ(IntRect(100, 50, 1, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(51, 50, 50, 50)));
        EXPECT_RECT_EQ(IntRect(51, 51, 50, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(51, 51, 50, 50)));
        EXPECT_RECT_EQ(IntRect(50, 100, 50, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(50, 51, 50, 50)));
        EXPECT_RECT_EQ(IntRect(49, 51, 50, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(49, 51, 50, 50)));

        occlusion.useDefaultLayerClipRect();
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(50, 50, 50, 50)).isEmpty());
        EXPECT_RECT_EQ(IntRect(49, 50, 1, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(49, 50, 50, 50)));
        EXPECT_RECT_EQ(IntRect(49, 49, 50, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(49, 49, 50, 50)));
        EXPECT_RECT_EQ(IntRect(50, 49, 50, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(50, 49, 50, 50)));
        EXPECT_RECT_EQ(IntRect(51, 49, 49, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(51, 49, 50, 50)));
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(51, 50, 50, 50)).isEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(51, 51, 50, 50)).isEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(50, 51, 50, 50)).isEmpty());
        EXPECT_RECT_EQ(IntRect(49, 51, 1, 49), occlusion.unoccludedLayerContentRect(parent, IntRect(49, 51, 50, 50)));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestTranslatedChild);

template<class Types>
class OcclusionTrackerTestChildInRotatedChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestChildInRotatedChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix childTransform;
        childTransform.translate(250, 250);
        childTransform.rotate(90);
        childTransform.translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        parent->setMasksToBounds(true);
        typename Types::LayerType* child = this->createLayer(parent, childTransform, FloatPoint(30, 30), IntSize(500, 500));
        child->setMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(child, this->identityMatrix, FloatPoint(10, 10), IntSize(500, 500), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->visitLayer(layer, occlusion);
        this->enterContributingSurface(child, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 40, 70, 60), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 430, 60, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->leaveContributingSurface(child, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 40, 70, 60), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 40, 70, 60), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(29, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 39, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(31, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 41, 70, 60)));

        occlusion.useDefaultLayerClipRect();
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(29, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 39, 70, 60)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(31, 40, 70, 60)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 41, 70, 60)));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));


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
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200));

        WebTransformationMatrix layer1Matrix;
        layer1Matrix.scale(2);
        typename Types::ContentLayerType* layer1 = this->createDrawingLayer(parent, layer1Matrix, FloatPoint(0, 0), IntSize(100, 100), true);
        layer1->setForceRenderSurface(true);

        WebTransformationMatrix layer2Matrix;
        layer2Matrix.translate(25, 25);
        typename Types::ContentLayerType* layer2 = this->createDrawingLayer(layer1, layer2Matrix, FloatPoint(0, 0), IntSize(50, 50), true);
        typename Types::ContentLayerType* occluder = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(100, 100), IntSize(500, 500), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));

        this->visitLayer(occluder, occlusion);
        this->enterLayer(layer2, occlusion);

        EXPECT_RECT_EQ(IntRect(100, 100, 100, 100), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());
        EXPECT_EQ(0u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_RECT_EQ(IntRect(0, 0, 25, 25), occlusion.unoccludedLayerContentRect(layer2, IntRect(0, 0, 25, 25)));
        EXPECT_RECT_EQ(IntRect(10, 25, 15, 25), occlusion.unoccludedLayerContentRect(layer2, IntRect(10, 25, 25, 25)));
        EXPECT_RECT_EQ(IntRect(25, 10, 25, 15), occlusion.unoccludedLayerContentRect(layer2, IntRect(25, 10, 25, 25)));
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(layer2, IntRect(25, 25, 25, 25)).isEmpty());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledRenderSurface);

template<class Types>
class OcclusionTrackerTestVisitTargetTwoTimes : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestVisitTargetTwoTimes(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix childTransform;
        childTransform.translate(250, 250);
        childTransform.rotate(90);
        childTransform.translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        parent->setMasksToBounds(true);
        typename Types::LayerType* child = this->createLayer(parent, childTransform, FloatPoint(30, 30), IntSize(500, 500));
        child->setMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(child, this->identityMatrix, FloatPoint(10, 10), IntSize(500, 500), true);
        // |child2| makes |parent|'s surface get considered by OcclusionTracker first, instead of |child|'s. This exercises different code in
        // leaveToTargetRenderSurface, as the target surface has already been seen.
        typename Types::ContentLayerType* child2 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(30, 30), IntSize(60, 20), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(-10, -10, 1000, 1000));

        this->visitLayer(child2, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 30, 60, 20), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 30, 60, 20), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->visitLayer(layer, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 430, 60, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->enterContributingSurface(child, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 430, 60, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        // Occlusion in |child2| should get merged with the |child| surface we are leaving now.
        this->leaveContributingSurface(child, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 30, 70, 70)));
        EXPECT_RECT_EQ(IntRect(90, 30, 10, 10), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 30, 70, 70)));

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 30, 60, 10)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(29, 30, 60, 10)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 29, 60, 10)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(31, 30, 60, 10)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 31, 60, 10)));

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(29, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 39, 70, 60)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(30, 30, 60, 10)).isEmpty());
        EXPECT_RECT_EQ(IntRect(29, 30, 1, 10), occlusion.unoccludedLayerContentRect(parent, IntRect(29, 30, 60, 10)));
        EXPECT_RECT_EQ(IntRect(30, 29, 60, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 29, 60, 10)));
        EXPECT_RECT_EQ(IntRect(90, 30, 1, 10), occlusion.unoccludedLayerContentRect(parent, IntRect(31, 30, 60, 10)));
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(30, 31, 60, 10)).isEmpty());

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(30, 40, 70, 60)).isEmpty());
        EXPECT_RECT_EQ(IntRect(29, 40, 1, 60), occlusion.unoccludedLayerContentRect(parent, IntRect(29, 40, 70, 60)));
        // This rect is mostly occluded by |child2|.
        EXPECT_RECT_EQ(IntRect(90, 39, 10, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 39, 70, 60)));
        // This rect extends past top/right ends of |child2|.
        EXPECT_RECT_EQ(IntRect(30, 29, 70, 11), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 29, 70, 70)));
        // This rect extends past left/right ends of |child2|.
        EXPECT_RECT_EQ(IntRect(20, 39, 80, 60), occlusion.unoccludedLayerContentRect(parent, IntRect(20, 39, 80, 60)));
        EXPECT_RECT_EQ(IntRect(100, 40, 1, 60), occlusion.unoccludedLayerContentRect(parent, IntRect(31, 40, 70, 60)));
        EXPECT_RECT_EQ(IntRect(30, 100, 70, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 41, 70, 60)));

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
        WebTransformationMatrix childTransform;
        childTransform.translate(250, 250);
        childTransform.rotate(95);
        childTransform.translate(-250, -250);

        WebTransformationMatrix layerTransform;
        layerTransform.translate(10, 10);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        typename Types::LayerType* child = this->createLayer(parent, childTransform, FloatPoint(30, 30), IntSize(500, 500));
        child->setMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(child, layerTransform, FloatPoint(0, 0), IntSize(500, 500), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        IntRect clippedLayerInChild = MathUtil::mapClippedRect(layerTransform, layer->visibleContentRect());

        this->visitLayer(layer, occlusion);
        this->enterContributingSurface(child, occlusion);

        EXPECT_RECT_EQ(IntRect(), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(0u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(clippedLayerInChild, occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(child, clippedLayerInChild));
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(child, clippedLayerInChild).isEmpty());
        clippedLayerInChild.move(-1, 0);
        EXPECT_FALSE(occlusion.occludedLayer(child, clippedLayerInChild));
        EXPECT_FALSE(occlusion.unoccludedLayerContentRect(child, clippedLayerInChild).isEmpty());
        clippedLayerInChild.move(1, 0);
        clippedLayerInChild.move(1, 0);
        EXPECT_FALSE(occlusion.occludedLayer(child, clippedLayerInChild));
        EXPECT_FALSE(occlusion.unoccludedLayerContentRect(child, clippedLayerInChild).isEmpty());
        clippedLayerInChild.move(-1, 0);
        clippedLayerInChild.move(0, -1);
        EXPECT_FALSE(occlusion.occludedLayer(child, clippedLayerInChild));
        EXPECT_FALSE(occlusion.unoccludedLayerContentRect(child, clippedLayerInChild).isEmpty());
        clippedLayerInChild.move(0, 1);
        clippedLayerInChild.move(0, 1);
        EXPECT_FALSE(occlusion.occludedLayer(child, clippedLayerInChild));
        EXPECT_FALSE(occlusion.unoccludedLayerContentRect(child, clippedLayerInChild).isEmpty());
        clippedLayerInChild.move(0, -1);

        this->leaveContributingSurface(child, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_RECT_EQ(IntRect(), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(0u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(0u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(75, 55, 1, 1)));
        EXPECT_RECT_EQ(IntRect(75, 55, 1, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(75, 55, 1, 1)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceRotatedOffAxis);

template<class Types>
class OcclusionTrackerTestSurfaceWithTwoOpaqueChildren : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceWithTwoOpaqueChildren(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix childTransform;
        childTransform.translate(250, 250);
        childTransform.rotate(90);
        childTransform.translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        parent->setMasksToBounds(true);
        typename Types::LayerType* child = this->createLayer(parent, childTransform, FloatPoint(30, 30), IntSize(500, 500));
        child->setMasksToBounds(true);
        typename Types::ContentLayerType* layer1 = this->createDrawingLayer(child, this->identityMatrix, FloatPoint(10, 10), IntSize(500, 500), true);
        typename Types::ContentLayerType* layer2 = this->createDrawingLayer(child, this->identityMatrix, FloatPoint(10, 450), IntSize(500, 60), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->visitLayer(layer2, occlusion);
        this->visitLayer(layer1, occlusion);
        this->enterContributingSurface(child, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 40, 70, 60), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 430, 60, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(child, IntRect(10, 430, 60, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(child, IntRect(9, 430, 60, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(child, IntRect(10, 429, 60, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(child, IntRect(11, 430, 60, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(child, IntRect(10, 431, 60, 70)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(child, IntRect(10, 430, 60, 70)).isEmpty());
        EXPECT_RECT_EQ(IntRect(9, 430, 1, 70), occlusion.unoccludedLayerContentRect(child, IntRect(9, 430, 60, 70)));
        EXPECT_RECT_EQ(IntRect(10, 429, 60, 1), occlusion.unoccludedLayerContentRect(child, IntRect(10, 429, 60, 70)));
        EXPECT_RECT_EQ(IntRect(70, 430, 1, 70), occlusion.unoccludedLayerContentRect(child, IntRect(11, 430, 60, 70)));
        EXPECT_RECT_EQ(IntRect(10, 500, 60, 1), occlusion.unoccludedLayerContentRect(child, IntRect(10, 431, 60, 70)));

        this->leaveContributingSurface(child, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 40, 70, 60), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 40, 70, 60), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(29, 40, 70, 60)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 39, 70, 60)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(30, 40, 70, 60)).isEmpty());
        EXPECT_RECT_EQ(IntRect(29, 40, 1, 60), occlusion.unoccludedLayerContentRect(parent, IntRect(29, 40, 70, 60)));
        EXPECT_RECT_EQ(IntRect(30, 39, 70, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 39, 70, 60)));
        EXPECT_RECT_EQ(IntRect(100, 40, 1, 60), occlusion.unoccludedLayerContentRect(parent, IntRect(31, 40, 70, 60)));
        EXPECT_RECT_EQ(IntRect(30, 100, 70, 1), occlusion.unoccludedLayerContentRect(parent, IntRect(30, 41, 70, 60)));

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
        WebTransformationMatrix childTransform;
        childTransform.translate(250, 250);
        childTransform.rotate(90);
        childTransform.translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        parent->setMasksToBounds(true);
        typename Types::LayerType* child1 = this->createSurface(parent, childTransform, FloatPoint(30, 30), IntSize(10, 10));
        typename Types::LayerType* child2 = this->createSurface(parent, childTransform, FloatPoint(20, 40), IntSize(10, 10));
        typename Types::ContentLayerType* layer1 = this->createDrawingLayer(child1, this->identityMatrix, FloatPoint(-10, -10), IntSize(510, 510), true);
        typename Types::ContentLayerType* layer2 = this->createDrawingLayer(child2, this->identityMatrix, FloatPoint(-10, -10), IntSize(510, 510), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(-20, -20, 1000, 1000));

        this->visitLayer(layer2, occlusion);
        this->enterContributingSurface(child2, occlusion);

        EXPECT_RECT_EQ(IntRect(20, 30, 80, 70), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(-10, 420, 70, 80), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(child2, IntRect(-10, 420, 70, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(child2, IntRect(-11, 420, 70, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(child2, IntRect(-10, 419, 70, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(child2, IntRect(-10, 420, 71, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(child2, IntRect(-10, 420, 70, 81)));

        occlusion.useDefaultLayerClipRect();
        EXPECT_TRUE(occlusion.occludedLayer(child2, IntRect(-10, 420, 70, 80)));
        EXPECT_TRUE(occlusion.occludedLayer(child2, IntRect(-11, 420, 70, 80)));
        EXPECT_TRUE(occlusion.occludedLayer(child2, IntRect(-10, 419, 70, 80)));
        EXPECT_TRUE(occlusion.occludedLayer(child2, IntRect(-10, 420, 71, 80)));
        EXPECT_TRUE(occlusion.occludedLayer(child2, IntRect(-10, 420, 70, 81)));
        occlusion.setLayerClipRect(IntRect(-20, -20, 1000, 1000));

        // There is nothing above child2's surface in the z-order.
        EXPECT_RECT_EQ(IntRect(-10, 420, 70, 80), occlusion.unoccludedContributingSurfaceContentRect(child2, false, IntRect(-10, 420, 70, 80)));

        this->leaveContributingSurface(child2, occlusion);
        this->visitLayer(layer1, occlusion);
        this->enterContributingSurface(child1, occlusion);

        EXPECT_RECT_EQ(IntRect(20, 20, 80, 80), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(-10, 430, 80, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(child1, IntRect(-10, 430, 80, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(child1, IntRect(-11, 430, 80, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(child1, IntRect(-10, 429, 80, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(child1, IntRect(-10, 430, 81, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(child1, IntRect(-10, 430, 80, 71)));

        // child2's contents will occlude child1 below it.
        EXPECT_RECT_EQ(IntRect(-10, 430, 10, 70), occlusion.unoccludedContributingSurfaceContentRect(child1, false, IntRect(-10, 430, 80, 70)));

        this->leaveContributingSurface(child1, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_RECT_EQ(IntRect(20, 20, 80, 80), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(20, 20, 80, 80), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(20, 20, 80, 80)));

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(30, 20, 70, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(29, 20, 70, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(30, 19, 70, 80)));

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(20, 30, 80, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(19, 30, 80, 70)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(20, 29, 80, 70)));

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
        WebTransformationMatrix child1Transform;
        child1Transform.translate(250, 250);
        child1Transform.rotate(-90);
        child1Transform.translate(-250, -250);

        WebTransformationMatrix child2Transform;
        child2Transform.translate(250, 250);
        child2Transform.rotate(90);
        child2Transform.translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        parent->setMasksToBounds(true);
        typename Types::LayerType* child1 = this->createSurface(parent, child1Transform, FloatPoint(30, 20), IntSize(10, 10));
        typename Types::LayerType* child2 = this->createDrawingSurface(parent, child2Transform, FloatPoint(20, 40), IntSize(10, 10), false);
        typename Types::ContentLayerType* layer1 = this->createDrawingLayer(child1, this->identityMatrix, FloatPoint(-10, -20), IntSize(510, 510), true);
        typename Types::ContentLayerType* layer2 = this->createDrawingLayer(child2, this->identityMatrix, FloatPoint(-10, -10), IntSize(510, 510), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(-30, -30, 1000, 1000));

        this->visitLayer(layer2, occlusion);
        this->enterLayer(child2, occlusion);

        EXPECT_RECT_EQ(IntRect(20, 30, 80, 70), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(-10, 420, 70, 80), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(child2, IntRect(-10, 420, 70, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(child2, IntRect(-11, 420, 70, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(child2, IntRect(-10, 419, 70, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(child2, IntRect(-10, 420, 71, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(child2, IntRect(-10, 420, 70, 81)));

        this->leaveLayer(child2, occlusion);
        this->enterContributingSurface(child2, occlusion);

        // There is nothing above child2's surface in the z-order.
        EXPECT_RECT_EQ(IntRect(-10, 420, 70, 80), occlusion.unoccludedContributingSurfaceContentRect(child2, false, IntRect(-10, 420, 70, 80)));

        this->leaveContributingSurface(child2, occlusion);
        this->visitLayer(layer1, occlusion);
        this->enterContributingSurface(child1, occlusion);

        EXPECT_RECT_EQ(IntRect(10, 20, 90, 80), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(420, -20, 80, 90), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(child1, IntRect(420, -20, 80, 90)));
        EXPECT_FALSE(occlusion.occludedLayer(child1, IntRect(419, -20, 80, 90)));
        EXPECT_FALSE(occlusion.occludedLayer(child1, IntRect(420, -21, 80, 90)));
        EXPECT_FALSE(occlusion.occludedLayer(child1, IntRect(420, -19, 80, 90)));
        EXPECT_FALSE(occlusion.occludedLayer(child1, IntRect(421, -20, 80, 90)));

        // child2's contents will occlude child1 below it.
        EXPECT_RECT_EQ(IntRect(420, -20, 80, 90), occlusion.unoccludedContributingSurfaceContentRect(child1, false, IntRect(420, -20, 80, 90)));
        EXPECT_RECT_EQ(IntRect(490, -10, 10, 80), occlusion.unoccludedContributingSurfaceContentRect(child1, false, IntRect(420, -10, 80, 90)));
        EXPECT_RECT_EQ(IntRect(420, -20, 70, 10), occlusion.unoccludedContributingSurfaceContentRect(child1, false, IntRect(420, -20, 70, 90)));

        this->leaveContributingSurface(child1, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_RECT_EQ(IntRect(10, 20, 90, 80), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(10, 20, 90, 80), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(10, 20, 90, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(9, 20, 90, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(10, 19, 90, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(11, 20, 90, 80)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(10, 21, 90, 80)));

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
        WebTransformationMatrix layerTransform;
        layerTransform.translate(250, 250);
        layerTransform.rotate(90);
        layerTransform.translate(-250, -250);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        parent->setMasksToBounds(true);
        typename Types::ContentLayerType* blurLayer = this->createDrawingLayer(parent, layerTransform, FloatPoint(30, 30), IntSize(500, 500), true);
        typename Types::ContentLayerType* opaqueLayer = this->createDrawingLayer(parent, layerTransform, FloatPoint(30, 30), IntSize(500, 500), true);
        typename Types::ContentLayerType* opacityLayer = this->createDrawingLayer(parent, layerTransform, FloatPoint(30, 30), IntSize(500, 500), true);

        WebFilterOperations filters;
        filters.append(WebFilterOperation::createBlurFilter(10));
        blurLayer->setFilters(filters);

        filters.clear();
        filters.append(WebFilterOperation::createGrayscaleFilter(0.5));
        opaqueLayer->setFilters(filters);

        filters.clear();
        filters.append(WebFilterOperation::createOpacityFilter(0.5));
        opacityLayer->setFilters(filters);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        // Opacity layer won't contribute to occlusion.
        this->visitLayer(opacityLayer, occlusion);
        this->enterContributingSurface(opacityLayer, occlusion);

        EXPECT_TRUE(occlusion.occlusionInScreenSpace().isEmpty());
        EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());

        // And has nothing to contribute to its parent surface.
        this->leaveContributingSurface(opacityLayer, occlusion);
        EXPECT_TRUE(occlusion.occlusionInScreenSpace().isEmpty());
        EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());

        // Opaque layer will contribute to occlusion.
        this->visitLayer(opaqueLayer, occlusion);
        this->enterContributingSurface(opaqueLayer, occlusion);

        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 430, 70, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        // And it gets translated to the parent surface.
        this->leaveContributingSurface(opaqueLayer, occlusion);
        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        // The blur layer needs to throw away any occlusion from outside its subtree.
        this->enterLayer(blurLayer, occlusion);
        EXPECT_TRUE(occlusion.occlusionInScreenSpace().isEmpty());
        EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());

        // And it won't contribute to occlusion.
        this->leaveLayer(blurLayer, occlusion);
        this->enterContributingSurface(blurLayer, occlusion);
        EXPECT_TRUE(occlusion.occlusionInScreenSpace().isEmpty());
        EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());

        // But the opaque layer's occlusion is preserved on the parent. 
        this->leaveContributingSurface(blurLayer, occlusion);
        this->enterLayer(parent, occlusion);
        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(30, 30, 70, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestFilters);

template<class Types>
class OcclusionTrackerTestReplicaDoesOcclude : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestReplicaDoesOcclude(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 100), IntSize(50, 50), true);
        this->createReplicaLayer(surface, this->identityMatrix, FloatPoint(50, 50), IntSize());
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->visitLayer(surface, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 100, 50, 50), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 50, 50), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->visitContributingSurface(surface, occlusion);
        this->enterLayer(parent, occlusion);

        // The surface and replica should both be occluding the parent.
        EXPECT_RECT_EQ(IntRect(0, 100, 100, 100), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInTargetSurface().rects().size());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaDoesOcclude);

template<class Types>
class OcclusionTrackerTestReplicaWithClipping : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestReplicaWithClipping(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 170));
        parent->setMasksToBounds(true);
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 100), IntSize(50, 50), true);
        this->createReplicaLayer(surface, this->identityMatrix, FloatPoint(50, 50), IntSize());
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->visitLayer(surface, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 100, 50, 50), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 50, 50), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->visitContributingSurface(surface, occlusion);
        this->enterLayer(parent, occlusion);

        // The surface and replica should both be occluding the parent.
        EXPECT_RECT_EQ(IntRect(0, 100, 100, 70), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInTargetSurface().rects().size());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaWithClipping);

template<class Types>
class OcclusionTrackerTestReplicaWithMask : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestReplicaWithMask(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 100), IntSize(50, 50), true);
        typename Types::LayerType* replica = this->createReplicaLayer(surface, this->identityMatrix, FloatPoint(50, 50), IntSize());
        this->createMaskLayer(replica, IntSize(10, 10));
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->visitLayer(surface, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 100, 50, 50), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 50, 50), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->visitContributingSurface(surface, occlusion);
        this->enterLayer(parent, occlusion);

        // The replica should not be occluding the parent, since it has a mask applied to it.
        EXPECT_RECT_EQ(IntRect(0, 100, 50, 50), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaWithMask);

template<class Types>
class OcclusionTrackerTestLayerClipRectOutsideChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerClipRectOutsideChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(200, 100, 100, 100));

        this->enterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(100, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(200, 100, 100, 100)));

        occlusion.useDefaultLayerClipRect();
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(200, 100, 100, 100)));
        occlusion.setLayerClipRect(IntRect(200, 100, 100, 100));

        this->leaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(200, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));

        EXPECT_RECT_EQ(IntRect(200, 100, 100, 100), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestLayerClipRectOutsideChild);

template<class Types>
class OcclusionTrackerTestViewportRectOutsideChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestViewportRectOutsideChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(200, 100, 100, 100));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->enterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(100, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(200, 100, 100, 100)));

        occlusion.useDefaultLayerClipRect();
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(200, 100, 100, 100)));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->leaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(200, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));

        EXPECT_RECT_EQ(IntRect(200, 100, 100, 100), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestViewportRectOutsideChild);

template<class Types>
class OcclusionTrackerTestLayerClipRectOverChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerClipRectOverChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(100, 100, 100, 100));

        this->enterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(100, 100, 100, 100)));

        this->leaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)).isEmpty());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestLayerClipRectOverChild);

template<class Types>
class OcclusionTrackerTestViewportRectOverChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestViewportRectOverChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(100, 100, 100, 100));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->enterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(100, 100, 100, 100)));

        this->leaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)).isEmpty());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestViewportRectOverChild);

template<class Types>
class OcclusionTrackerTestLayerClipRectPartlyOverChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerClipRectPartlyOverChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(50, 50, 200, 200));

        this->enterLayer(layer, occlusion);

        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(0, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(100, 100, 100, 100)));

        this->leaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(200, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(200, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(0, 200, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(100, 200, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));

        EXPECT_RECT_EQ(IntRect(50, 50, 200, 200), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)));
        EXPECT_RECT_EQ(IntRect(200, 50, 50, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 100)));
        EXPECT_RECT_EQ(IntRect(200, 100, 50, 100), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 100, 300, 100)));
        EXPECT_RECT_EQ(IntRect(200, 100, 50, 100), occlusion.unoccludedLayerContentRect(parent, IntRect(200, 100, 100, 100)));
        EXPECT_RECT_EQ(IntRect(100, 200, 100, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(100, 200, 100, 100)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestLayerClipRectPartlyOverChild);

template<class Types>
class OcclusionTrackerTestViewportRectPartlyOverChild : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestViewportRectPartlyOverChild(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(50, 50, 200, 200));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->enterLayer(layer, occlusion);

        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(0, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(100, 100, 100, 100)));

        this->leaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(200, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(200, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(0, 200, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(100, 200, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));

        EXPECT_RECT_EQ(IntRect(50, 50, 200, 200), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)));
        EXPECT_RECT_EQ(IntRect(200, 50, 50, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 100)));
        EXPECT_RECT_EQ(IntRect(200, 100, 50, 100), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 100, 300, 100)));
        EXPECT_RECT_EQ(IntRect(200, 100, 50, 100), occlusion.unoccludedLayerContentRect(parent, IntRect(200, 100, 100, 100)));
        EXPECT_RECT_EQ(IntRect(100, 200, 100, 50), occlusion.unoccludedLayerContentRect(parent, IntRect(100, 200, 100, 100)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestViewportRectPartlyOverChild);

template<class Types>
class OcclusionTrackerTestLayerClipRectOverNothing : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerClipRectOverNothing(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(500, 500, 100, 100));

        this->enterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(100, 100, 100, 100)));

        this->leaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)).isEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 100)).isEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(0, 100, 300, 100)).isEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(200, 100, 100, 100)).isEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(100, 200, 100, 100)).isEmpty());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestLayerClipRectOverNothing);

template<class Types>
class OcclusionTrackerTestViewportRectOverNothing : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestViewportRectOverNothing(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(500, 500, 100, 100));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->enterLayer(layer, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(100, 100, 100, 100)));

        this->leaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 0, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(0, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 200, 100, 100)));
        EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));

        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)).isEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 100)).isEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(0, 100, 300, 100)).isEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(200, 100, 100, 100)).isEmpty());
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(parent, IntRect(100, 200, 100, 100)).isEmpty());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestViewportRectOverNothing);

template<class Types>
class OcclusionTrackerTestLayerClipRectForLayerOffOrigin : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestLayerClipRectForLayerOffOrigin(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        this->enterLayer(layer, occlusion);

        // This layer is translated when drawn into its target. So if the clip rect given from the target surface
        // is not in that target space, then after translating these query rects into the target, they will fall outside
        // the clip and be considered occluded.
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(0, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(100, 100, 100, 100)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestLayerClipRectForLayerOffOrigin);

template<class Types>
class OcclusionTrackerTestOpaqueContentsRegionEmpty : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestOpaqueContentsRegionEmpty(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 200), false);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        this->enterLayer(layer, occlusion);

        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(0, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(100, 0, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(100, 100, 100, 100)));

        // Occluded since its outside the surface bounds.
        EXPECT_TRUE(occlusion.occludedLayer(layer, IntRect(200, 100, 100, 100)));

        // Test without any clip rect.
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));
        EXPECT_FALSE(occlusion.occludedLayer(layer, IntRect(200, 100, 100, 100)));
        occlusion.useDefaultLayerClipRect();

        this->leaveLayer(layer, occlusion);
        this->visitContributingSurface(layer, occlusion);
        this->enterLayer(parent, occlusion);

        EXPECT_TRUE(occlusion.occlusionInScreenSpace().bounds().isEmpty());
        EXPECT_EQ(0u, occlusion.occlusionInScreenSpace().rects().size());
    }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestOpaqueContentsRegionEmpty);

template<class Types>
class OcclusionTrackerTestOpaqueContentsRegionNonEmpty : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestOpaqueContentsRegionNonEmpty(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(100, 100), IntSize(200, 200), false);
        this->calcDrawEtc(parent);

        {
            TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
            layer->setOpaqueContentsRect(IntRect(0, 0, 100, 100));

            this->resetLayerIterator();
            this->visitLayer(layer, occlusion);
            this->enterLayer(parent, occlusion);

            EXPECT_RECT_EQ(IntRect(100, 100, 100, 100), occlusion.occlusionInScreenSpace().bounds());
            EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());

            EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
            EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(100, 100, 100, 100)));
            EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));
        }

        {
            TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
            layer->setOpaqueContentsRect(IntRect(20, 20, 180, 180));

            this->resetLayerIterator();
            this->visitLayer(layer, occlusion);
            this->enterLayer(parent, occlusion);

            EXPECT_RECT_EQ(IntRect(120, 120, 180, 180), occlusion.occlusionInScreenSpace().bounds());
            EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());

            EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
            EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(100, 100, 100, 100)));
            EXPECT_TRUE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));
        }

        {
            TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
            layer->setOpaqueContentsRect(IntRect(150, 150, 100, 100));

            this->resetLayerIterator();
            this->visitLayer(layer, occlusion);
            this->enterLayer(parent, occlusion);

            EXPECT_RECT_EQ(IntRect(250, 250, 50, 50), occlusion.occlusionInScreenSpace().bounds());
            EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());

            EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(0, 100, 100, 100)));
            EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(100, 100, 100, 100)));
            EXPECT_FALSE(occlusion.occludedLayer(parent, IntRect(200, 200, 100, 100)));
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
        WebTransformationMatrix transform;
        transform.rotate3d(0, 30, 0);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::LayerType* container = this->createLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(container, transform, FloatPoint(100, 100), IntSize(200, 200), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        this->enterLayer(layer, occlusion);

        // The layer is rotated in 3d but without preserving 3d, so it only gets resized.
        EXPECT_RECT_EQ(IntRect(0, 0, 200, 200), occlusion.unoccludedLayerContentRect(layer, IntRect(0, 0, 200, 200)));
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

        WebTransformationMatrix translationToFront;
        translationToFront.translate3d(0, 0, -10);
        WebTransformationMatrix translationToBack;
        translationToFront.translate3d(0, 0, -100);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* child1 = this->createDrawingLayer(parent, translationToBack, FloatPoint(0, 0), IntSize(100, 100), true);
        typename Types::ContentLayerType* child2 = this->createDrawingLayer(parent, translationToFront, FloatPoint(50, 50), IntSize(100, 100), true);
        parent->setPreserves3D(true);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        this->visitLayer(child2, occlusion);
        EXPECT_TRUE(occlusion.occlusionInScreenSpace().isEmpty());
        EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());

        this->visitLayer(child1, occlusion);
        EXPECT_TRUE(occlusion.occlusionInScreenSpace().isEmpty());
        EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());
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
        WebTransformationMatrix transform;
        transform.translate(150, 150);
        transform.applyPerspective(400);
        transform.rotate3d(1, 0, 0, -30);
        transform.translate(-150, -150);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::LayerType* container = this->createLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(container, transform, FloatPoint(100, 100), IntSize(200, 200), true);
        container->setPreserves3D(true);
        layer->setPreserves3D(true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        this->enterLayer(layer, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 200, 200), occlusion.unoccludedLayerContentRect(layer, IntRect(0, 0, 200, 200)));
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
        WebTransformationMatrix transform;
        transform.translate(250, 50);
        transform.applyPerspective(10);
        transform.translate(-250, -50);
        transform.translate(250, 50);
        transform.rotate3d(1, 0, 0, -167);
        transform.translate(-250, -50);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(500, 100));
        typename Types::LayerType* container = this->createLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(500, 500));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(container, transform, FloatPoint(0, 0), IntSize(500, 500), true);
        container->setPreserves3D(true);
        layer->setPreserves3D(true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        this->enterLayer(layer, occlusion);

        // The bottom 11 pixel rows of this layer remain visible inside the container, after translation to the target surface. When translated back,
        // this will include many more pixels but must include at least the bottom 11 rows.
        EXPECT_TRUE(occlusion.unoccludedLayerContentRect(layer, IntRect(0, 0, 500, 500)).contains(IntRect(0, 489, 500, 11)));
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
        WebTransformationMatrix transform;
        transform.translate(50, 50);
        transform.applyPerspective(100);
        transform.translate3d(0, 0, 110);
        transform.translate(-50, -50);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, transform, FloatPoint(0, 0), IntSize(100, 100), true);
        parent->setPreserves3D(true);
        layer->setPreserves3D(true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));

        // The |layer| is entirely behind the camera and should not occlude.
        this->visitLayer(layer, occlusion);
        this->enterLayer(parent, occlusion);
        EXPECT_EQ(0u, occlusion.occlusionInTargetSurface().rects().size());
        EXPECT_EQ(0u, occlusion.occlusionInScreenSpace().rects().size());
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
        WebTransformationMatrix transform;
        transform.translate(50, 50);
        transform.applyPerspective(100);
        transform.translate3d(0, 0, 99);
        transform.translate(-50, -50);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
        parent->setMasksToBounds(true);
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, transform, FloatPoint(0, 0), IntSize(100, 100), true);
        parent->setPreserves3D(true);
        layer->setPreserves3D(true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));

        // This is very close to the camera, so pixels in its visibleContentRect will actually go outside of the layer's clipRect.
        // Ensure that those pixels don't occlude things outside the clipRect.
        this->visitLayer(layer, occlusion);
        this->enterLayer(parent, occlusion);
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 100), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 100), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
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
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300), true);
        typename Types::ContentLayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300), true);
        typename Types::ContentLayerType* surfaceChild = this->createDrawingLayer(surface, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 300), true);
        typename Types::ContentLayerType* surfaceChild2 = this->createDrawingLayer(surface, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 300), true);
        typename Types::ContentLayerType* parent2 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false);
        typename Types::ContentLayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(250, 0), IntSize(50, 300), true);

        addOpacityTransitionToController(*layer->layerAnimationController(), 10, 0, 1, false);
        addOpacityTransitionToController(*surface->layerAnimationController(), 10, 0, 1, false);
        this->calcDrawEtc(parent);

        EXPECT_TRUE(layer->drawOpacityIsAnimating());
        EXPECT_FALSE(surface->drawOpacityIsAnimating());
        EXPECT_TRUE(surface->renderSurface()->drawOpacityIsAnimating());

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));

        this->visitLayer(topmost, occlusion);
        this->enterLayer(parent2, occlusion);
        // This occlusion will affect all surfaces.
        EXPECT_RECT_EQ(IntRect(0, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent2, IntRect(0, 0, 300, 300)));
        this->leaveLayer(parent2, occlusion);

        this->visitLayer(surfaceChild2, occlusion);
        this->enterLayer(surfaceChild, occlusion);
        EXPECT_RECT_EQ(IntRect(100, 0, 100, 300), occlusion.unoccludedLayerContentRect(surfaceChild, IntRect(0, 0, 300, 300)));
        this->leaveLayer(surfaceChild, occlusion);
        this->enterLayer(surface, occlusion);
        EXPECT_RECT_EQ(IntRect(200, 0, 50, 300), occlusion.unoccludedLayerContentRect(surface, IntRect(0, 0, 300, 300)));
        this->leaveLayer(surface, occlusion);

        this->enterContributingSurface(surface, occlusion);
        // Occlusion within the surface is lost when leaving the animating surface.
        EXPECT_RECT_EQ(IntRect(0, 0, 250, 300), occlusion.unoccludedContributingSurfaceContentRect(surface, false, IntRect(0, 0, 300, 300)));
        this->leaveContributingSurface(surface, occlusion);

        this->visitLayer(layer, occlusion);
        this->enterLayer(parent, occlusion);

        // Occlusion is not added for the animating |layer|.
        EXPECT_RECT_EQ(IntRect(0, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)));
    }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationOpacity1OnMainThread);

template<class Types>
class OcclusionTrackerTestAnimationOpacity0OnMainThread : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestAnimationOpacity0OnMainThread(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300), true);
        typename Types::ContentLayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300), true);
        typename Types::ContentLayerType* surfaceChild = this->createDrawingLayer(surface, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 300), true);
        typename Types::ContentLayerType* surfaceChild2 = this->createDrawingLayer(surface, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 300), true);
        typename Types::ContentLayerType* parent2 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false);
        typename Types::ContentLayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(250, 0), IntSize(50, 300), true);

        addOpacityTransitionToController(*layer->layerAnimationController(), 10, 1, 0, false);
        addOpacityTransitionToController(*surface->layerAnimationController(), 10, 1, 0, false);
        this->calcDrawEtc(parent);

        EXPECT_TRUE(layer->drawOpacityIsAnimating());
        EXPECT_FALSE(surface->drawOpacityIsAnimating());
        EXPECT_TRUE(surface->renderSurface()->drawOpacityIsAnimating());

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));

        this->visitLayer(topmost, occlusion);
        this->enterLayer(parent2, occlusion);
        // This occlusion will affect all surfaces.
        EXPECT_RECT_EQ(IntRect(0, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)));
        this->leaveLayer(parent2, occlusion);

        this->visitLayer(surfaceChild2, occlusion);
        this->enterLayer(surfaceChild, occlusion);
        EXPECT_RECT_EQ(IntRect(100, 0, 100, 300), occlusion.unoccludedLayerContentRect(surfaceChild, IntRect(0, 0, 300, 300)));
        this->leaveLayer(surfaceChild, occlusion);
        this->enterLayer(surface, occlusion);
        EXPECT_RECT_EQ(IntRect(200, 0, 50, 300), occlusion.unoccludedLayerContentRect(surface, IntRect(0, 0, 300, 300)));
        this->leaveLayer(surface, occlusion);

        this->enterContributingSurface(surface, occlusion);
        // Occlusion within the surface is lost when leaving the animating surface.
        EXPECT_RECT_EQ(IntRect(0, 0, 250, 300), occlusion.unoccludedContributingSurfaceContentRect(surface, false, IntRect(0, 0, 300, 300)));
        this->leaveContributingSurface(surface, occlusion);

        this->visitLayer(layer, occlusion);
        this->enterLayer(parent, occlusion);

        // Occlusion is not added for the animating |layer|.
        EXPECT_RECT_EQ(IntRect(0, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)));
    }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationOpacity0OnMainThread);

template<class Types>
class OcclusionTrackerTestAnimationTranslateOnMainThread : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestAnimationTranslateOnMainThread(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        typename Types::ContentLayerType* layer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300), true);
        typename Types::ContentLayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300), true);
        typename Types::ContentLayerType* surfaceChild = this->createDrawingLayer(surface, this->identityMatrix, FloatPoint(0, 0), IntSize(200, 300), true);
        typename Types::ContentLayerType* surfaceChild2 = this->createDrawingLayer(surface, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 300), true);
        typename Types::ContentLayerType* surface2 = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(50, 300), true);

        addAnimatedTransformToController(*layer->layerAnimationController(), 10, 30, 0);
        addAnimatedTransformToController(*surface->layerAnimationController(), 10, 30, 0);
        addAnimatedTransformToController(*surfaceChild->layerAnimationController(), 10, 30, 0);
        this->calcDrawEtc(parent);

        EXPECT_TRUE(layer->drawTransformIsAnimating());
        EXPECT_TRUE(layer->screenSpaceTransformIsAnimating());
        EXPECT_TRUE(surface->renderSurface()->targetSurfaceTransformsAreAnimating());
        EXPECT_TRUE(surface->renderSurface()->screenSpaceTransformsAreAnimating());
        // The surface owning layer doesn't animate against its own surface.
        EXPECT_FALSE(surface->drawTransformIsAnimating());
        EXPECT_TRUE(surface->screenSpaceTransformIsAnimating());
        EXPECT_TRUE(surfaceChild->drawTransformIsAnimating());
        EXPECT_TRUE(surfaceChild->screenSpaceTransformIsAnimating());

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));

        this->visitLayer(surface2, occlusion);
        this->enterContributingSurface(surface2, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 50, 300), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());

        this->leaveContributingSurface(surface2, occlusion);
        this->enterLayer(surfaceChild2, occlusion);

        // surfaceChild2 is moving in screen space but not relative to its target, so occlusion should happen in its target space only.
        // It also means that things occluding in screen space (e.g. surface2) cannot occlude this layer.
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 300), occlusion.unoccludedLayerContentRect(surfaceChild2, IntRect(0, 0, 100, 300)));
        EXPECT_FALSE(occlusion.occludedLayer(surfaceChild, IntRect(0, 0, 50, 300)));

        this->leaveLayer(surfaceChild2, occlusion);
        this->enterLayer(surfaceChild, occlusion);
        EXPECT_FALSE(occlusion.occludedLayer(surfaceChild, IntRect(0, 0, 100, 300)));
        EXPECT_RECT_EQ(IntRect(0, 0, 50, 300), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 300), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
        EXPECT_RECT_EQ(IntRect(100, 0, 200, 300), occlusion.unoccludedLayerContentRect(surface, IntRect(0, 0, 300, 300)));

        // The surfaceChild is occluded by the surfaceChild2, but is moving relative its target and the screen, so it
        // can't be occluded.
        EXPECT_RECT_EQ(IntRect(0, 0, 200, 300), occlusion.unoccludedLayerContentRect(surfaceChild, IntRect(0, 0, 200, 300)));
        EXPECT_FALSE(occlusion.occludedLayer(surfaceChild, IntRect(0, 0, 50, 300)));

        this->leaveLayer(surfaceChild, occlusion);
        this->enterLayer(surface, occlusion);
        // The surfaceChild is moving in screen space but not relative to its target, so occlusion should happen in its target space only.
        EXPECT_RECT_EQ(IntRect(0, 0, 50, 300), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 300), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
        EXPECT_RECT_EQ(IntRect(100, 0, 200, 300), occlusion.unoccludedLayerContentRect(surface, IntRect(0, 0, 300, 300)));

        this->leaveLayer(surface, occlusion);
        // The surface's owning layer is moving in screen space but not relative to its target, so occlusion should happen in its target space only.
        EXPECT_RECT_EQ(IntRect(0, 0, 50, 300), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 300, 300), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 0, 0), occlusion.unoccludedLayerContentRect(surface, IntRect(0, 0, 300, 300)));

        this->enterContributingSurface(surface, occlusion);
        // The contributing |surface| is animating so it can't be occluded.
        EXPECT_RECT_EQ(IntRect(0, 0, 300, 300), occlusion.unoccludedContributingSurfaceContentRect(surface, false, IntRect(0, 0, 300, 300)));
        this->leaveContributingSurface(surface, occlusion);

        this->enterLayer(layer, occlusion);
        // The |surface| is moving in the screen and in its target, so all occlusion within the surface is lost when leaving it.
        EXPECT_RECT_EQ(IntRect(50, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)));
        this->leaveLayer(layer, occlusion);

        this->enterLayer(parent, occlusion);
        // The |layer| is animating in the screen and in its target, so no occlusion is added.
        EXPECT_RECT_EQ(IntRect(50, 0, 250, 300), occlusion.unoccludedLayerContentRect(parent, IntRect(0, 0, 300, 300)));
    }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationTranslateOnMainThread);

template<class Types>
class OcclusionTrackerTestSurfaceOcclusionTranslatesToParent : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceOcclusionTranslatesToParent(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix surfaceTransform;
        surfaceTransform.translate(300, 300);
        surfaceTransform.scale(2);
        surfaceTransform.translate(-150, -150);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(500, 500));
        typename Types::ContentLayerType* surface = this->createDrawingSurface(parent, surfaceTransform, FloatPoint(0, 0), IntSize(300, 300), false);
        typename Types::ContentLayerType* surface2 = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(50, 50), IntSize(300, 300), false);
        surface->setOpaqueContentsRect(IntRect(0, 0, 200, 200));
        surface2->setOpaqueContentsRect(IntRect(0, 0, 200, 200));
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));

        this->visitLayer(surface2, occlusion);
        this->visitContributingSurface(surface2, occlusion);

        EXPECT_RECT_EQ(IntRect(50, 50, 200, 200), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(50, 50, 200, 200), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        // Clear any stored occlusion.
        occlusion.setOcclusionInScreenSpace(Region());
        occlusion.setOcclusionInTargetSurface(Region());

        this->visitLayer(surface, occlusion);
        this->visitContributingSurface(surface, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 400, 400), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 400, 400), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
    }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestSurfaceOcclusionTranslatesToParent);

template<class Types>
class OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
        parent->setMasksToBounds(true);
        typename Types::ContentLayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(500, 300), false);
        surface->setOpaqueContentsRect(IntRect(0, 0, 400, 200));
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));

        this->visitLayer(surface, occlusion);
        this->visitContributingSurface(surface, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 300, 200), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 300, 200), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
    }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping);

template<class Types>
class OcclusionTrackerTestReplicaOccluded : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestReplicaOccluded(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100), true);
        this->createReplicaLayer(surface, this->identityMatrix, FloatPoint(0, 100), IntSize(100, 100));
        typename Types::LayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 100), IntSize(100, 100), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        // |topmost| occludes the replica, but not the surface itself.
        this->visitLayer(topmost, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 100, 100, 100), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 100, 100, 100), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->visitLayer(surface, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 100, 200), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 100), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->enterContributingSurface(surface, occlusion);

        // Surface is not occluded so it shouldn't think it is.
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 100), occlusion.unoccludedContributingSurfaceContentRect(surface, false, IntRect(0, 0, 100, 100)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaOccluded);

template<class Types>
class OcclusionTrackerTestSurfaceWithReplicaUnoccluded : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceWithReplicaUnoccluded(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100), true);
        this->createReplicaLayer(surface, this->identityMatrix, FloatPoint(0, 100), IntSize(100, 100));
        typename Types::LayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 110), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        // |topmost| occludes the surface, but not the entire surface's replica.
        this->visitLayer(topmost, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 100, 110), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 110), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->visitLayer(surface, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 100, 110), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 100), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->enterContributingSurface(surface, occlusion);

        // Surface is occluded, but only the top 10px of the replica.
        EXPECT_RECT_EQ(IntRect(0, 0, 0, 0), occlusion.unoccludedContributingSurfaceContentRect(surface, false, IntRect(0, 0, 100, 100)));
        EXPECT_RECT_EQ(IntRect(0, 10, 100, 90), occlusion.unoccludedContributingSurfaceContentRect(surface, true, IntRect(0, 0, 100, 100)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceWithReplicaUnoccluded);

template<class Types>
class OcclusionTrackerTestSurfaceAndReplicaOccludedDifferently : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestSurfaceAndReplicaOccludedDifferently(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100), true);
        this->createReplicaLayer(surface, this->identityMatrix, FloatPoint(0, 100), IntSize(100, 100));
        typename Types::LayerType* overSurface = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(40, 100), true);
        typename Types::LayerType* overReplica = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 100), IntSize(50, 100), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        // These occlude the surface and replica differently, so we can test each one.
        this->visitLayer(overReplica, occlusion);
        this->visitLayer(overSurface, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 50, 200), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 50, 200), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInTargetSurface().rects().size());

        this->visitLayer(surface, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 100, 200), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 100), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->enterContributingSurface(surface, occlusion);

        // Surface and replica are occluded different amounts.
        EXPECT_RECT_EQ(IntRect(40, 0, 60, 100), occlusion.unoccludedContributingSurfaceContentRect(surface, false, IntRect(0, 0, 100, 100)));
        EXPECT_RECT_EQ(IntRect(50, 0, 50, 100), occlusion.unoccludedContributingSurfaceContentRect(surface, true, IntRect(0, 0, 100, 100)));
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

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100), true);
        typename Types::LayerType* surfaceChild = this->createDrawingSurface(surface, this->identityMatrix, FloatPoint(0, 10), IntSize(100, 50), true);
        typename Types::LayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 50), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(-100, -100, 1000, 1000));

        // |topmost| occludes everything partially so we know occlusion is happening at all.
        this->visitLayer(topmost, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 100, 50), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 50), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->visitLayer(surfaceChild, occlusion);

        // surfaceChild increases the occlusion in the screen by a narrow sliver.
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 60), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        // In its own surface, surfaceChild is at 0,0 as is its occlusion.
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 50), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        // The root layer always has a clipRect. So the parent of |surface| has a clipRect. However, the owning layer for |surface| does not
        // mask to bounds, so it doesn't have a clipRect of its own. Thus the parent of |surfaceChild| exercises different code paths
        // as its parent does not have a clipRect.

        this->enterContributingSurface(surfaceChild, occlusion);
        // The surfaceChild's parent does not have a clipRect as it owns a render surface. Make sure the unoccluded rect
        // does not get clipped away inappropriately.
        EXPECT_RECT_EQ(IntRect(0, 40, 100, 10), occlusion.unoccludedContributingSurfaceContentRect(surfaceChild, false, IntRect(0, 0, 100, 50)));
        this->leaveContributingSurface(surfaceChild, occlusion);

        // When the surfaceChild's occlusion is transformed up to its parent, make sure it is not clipped away inappropriately also.
        this->enterLayer(surface, occlusion);
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 60), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 10, 100, 50), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
        this->leaveLayer(surface, occlusion);

        this->enterContributingSurface(surface, occlusion);
        // The surface's parent does have a clipRect as it is the root layer.
        EXPECT_RECT_EQ(IntRect(0, 50, 100, 50), occlusion.unoccludedContributingSurfaceContentRect(surface, false, IntRect(0, 0, 100, 100)));
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

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(100, 200));
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 300), true);
        this->calcDrawEtc(parent);

        {
            // Make a viewport rect that is larger than the root layer.
            TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));

            this->visitLayer(surface, occlusion);

            // The root layer always has a clipRect. So the parent of |surface| has a clipRect giving the surface itself a clipRect.
            this->enterContributingSurface(surface, occlusion);
            // Make sure the parent's clipRect clips the unoccluded region of the child surface.
            EXPECT_RECT_EQ(IntRect(0, 0, 100, 200), occlusion.unoccludedContributingSurfaceContentRect(surface, false, IntRect(0, 0, 100, 300)));
        }
        this->resetLayerIterator();
        {
            // Make a viewport rect that is smaller than the root layer.
            TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 100, 100));

            this->visitLayer(surface, occlusion);

            // The root layer always has a clipRect. So the parent of |surface| has a clipRect giving the surface itself a clipRect.
            this->enterContributingSurface(surface, occlusion);
            // Make sure the viewport rect clips the unoccluded region of the child surface.
            EXPECT_RECT_EQ(IntRect(0, 0, 100, 100), occlusion.unoccludedContributingSurfaceContentRect(surface, false, IntRect(0, 0, 100, 300)));
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

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(80, 200));
        parent->setMasksToBounds(true);
        typename Types::LayerType* surface = this->createDrawingSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100), true);
        typename Types::LayerType* surfaceChild = this->createDrawingSurface(surface, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 100), false);
        typename Types::LayerType* topmost = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(100, 50), true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        // |topmost| occludes everything partially so we know occlusion is happening at all.
        this->visitLayer(topmost, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 80, 50), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 80, 50), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        // surfaceChild is not opaque and does not occlude, so we have a non-empty unoccluded area on surface.
        this->visitLayer(surfaceChild, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 80, 50), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 0, 0), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(0u, occlusion.occlusionInTargetSurface().rects().size());

        // The root layer always has a clipRect. So the parent of |surface| has a clipRect. However, the owning layer for |surface| does not
        // mask to bounds, so it doesn't have a clipRect of its own. Thus the parent of |surfaceChild| exercises different code paths
        // as its parent does not have a clipRect.

        this->enterContributingSurface(surfaceChild, occlusion);
        // The surfaceChild's parent does not have a clipRect as it owns a render surface.
        EXPECT_RECT_EQ(IntRect(0, 50, 80, 50), occlusion.unoccludedContributingSurfaceContentRect(surfaceChild, false, IntRect(0, 0, 100, 100)));
        this->leaveContributingSurface(surfaceChild, occlusion);

        this->visitLayer(surface, occlusion);
        this->enterContributingSurface(surface, occlusion);
        // The surface's parent does have a clipRect as it is the root layer.
        EXPECT_RECT_EQ(IntRect(0, 50, 80, 50), occlusion.unoccludedContributingSurfaceContentRect(surface, false, IntRect(0, 0, 100, 100)));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceChildOfClippingSurface);

template<class Types>
class OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix scaleByHalf;
        scaleByHalf.scale(0.5);

        // Make a surface and its replica, each 50x50, that are completely surrounded by opaque layers which are above them in the z-order.
        // The surface is scaled to test that the pixel moving is done in the target space, where the background filter is applied, but the surface
        // appears at 50, 50 and the replica at 200, 50.
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 150));
        typename Types::LayerType* filteredSurface = this->createDrawingLayer(parent, scaleByHalf, FloatPoint(50, 50), IntSize(100, 100), false);
        this->createReplicaLayer(filteredSurface, this->identityMatrix, FloatPoint(300, 0), IntSize());
        typename Types::LayerType* occludingLayer1 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 50), true);
        typename Types::LayerType* occludingLayer2 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 100), IntSize(300, 50), true);
        typename Types::LayerType* occludingLayer3 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 50), IntSize(50, 50), true);
        typename Types::LayerType* occludingLayer4 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(100, 50), IntSize(100, 50), true);
        typename Types::LayerType* occludingLayer5 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(250, 50), IntSize(50, 50), true);

        // Filters make the layer own a surface.
        WebFilterOperations filters;
        filters.append(WebFilterOperation::createBlurFilter(10));
        filteredSurface->setBackgroundFilters(filters);

        // Save the distance of influence for the blur effect.
        int outsetTop, outsetRight, outsetBottom, outsetLeft;
        filters.getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        // These layers occlude pixels directly beside the filteredSurface. Because filtered surface blends pixels in a radius, it will
        // need to see some of the pixels (up to radius far) underneath the occludingLayers.
        this->visitLayer(occludingLayer5, occlusion);
        this->visitLayer(occludingLayer4, occlusion);
        this->visitLayer(occludingLayer3, occlusion);
        this->visitLayer(occludingLayer2, occlusion);
        this->visitLayer(occludingLayer1, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 300, 150), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(5u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 300, 150), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(5u, occlusion.occlusionInTargetSurface().rects().size());

        // Everything outside the surface/replica is occluded but the surface/replica itself is not.
        this->enterLayer(filteredSurface, occlusion);
        EXPECT_RECT_EQ(IntRect(1, 0, 99, 100), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(1, 0, 100, 100)));
        EXPECT_RECT_EQ(IntRect(0, 1, 100, 99), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(0, 1, 100, 100)));
        EXPECT_RECT_EQ(IntRect(0, 0, 99, 100), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(-1, 0, 100, 100)));
        EXPECT_RECT_EQ(IntRect(0, 0, 100, 99), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(0, -1, 100, 100)));

        EXPECT_RECT_EQ(IntRect(300 + 1, 0, 99, 100), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(300 + 1, 0, 100, 100)));
        EXPECT_RECT_EQ(IntRect(300 + 0, 1, 100, 99), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(300 + 0, 1, 100, 100)));
        EXPECT_RECT_EQ(IntRect(300 + 0, 0, 99, 100), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(300 - 1, 0, 100, 100)));
        EXPECT_RECT_EQ(IntRect(300 + 0, 0, 100, 99), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(300 + 0, -1, 100, 100)));
        this->leaveLayer(filteredSurface, occlusion);

        // The filtered layer/replica does not occlude.
        EXPECT_RECT_EQ(IntRect(0, 0, 300, 150), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(5u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 0, 0), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(0u, occlusion.occlusionInTargetSurface().rects().size());

        // The surface has a background blur, so it needs pixels that are currently considered occluded in order to be drawn. So the pixels
        // it needs should be removed some the occluded area so that when we get to the parent they are drawn.
        this->visitContributingSurface(filteredSurface, occlusion);

        this->enterLayer(parent, occlusion);
        EXPECT_RECT_EQ(IntRect(0, 0, 300, 150), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(5u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 300, 150), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(5u, occlusion.occlusionInTargetSurface().rects().size());

        IntRect outsetRect;
        IntRect testRect;

        // Nothing in the blur outsets for the filteredSurface is occluded.
        outsetRect = IntRect(50 - outsetLeft, 50 - outsetTop, 50 + outsetLeft + outsetRight, 50 + outsetTop + outsetBottom);
        testRect = outsetRect;
        EXPECT_RECT_EQ(outsetRect, occlusion.unoccludedLayerContentRect(parent, testRect));

        // Stuff outside the blur outsets is still occluded though.
        testRect = outsetRect;
        testRect.expand(1, 0);
        EXPECT_RECT_EQ(outsetRect, occlusion.unoccludedLayerContentRect(parent, testRect));
        testRect = outsetRect;
        testRect.expand(0, 1);
        EXPECT_RECT_EQ(outsetRect, occlusion.unoccludedLayerContentRect(parent, testRect));
        testRect = outsetRect;
        testRect.move(-1, 0);
        testRect.expand(1, 0);
        EXPECT_RECT_EQ(outsetRect, occlusion.unoccludedLayerContentRect(parent, testRect));
        testRect = outsetRect;
        testRect.move(0, -1);
        testRect.expand(0, 1);
        EXPECT_RECT_EQ(outsetRect, occlusion.unoccludedLayerContentRect(parent, testRect));

        // Nothing in the blur outsets for the filteredSurface's replica is occluded.
        outsetRect = IntRect(200 - outsetLeft, 50 - outsetTop, 50 + outsetLeft + outsetRight, 50 + outsetTop + outsetBottom);
        testRect = outsetRect;
        EXPECT_RECT_EQ(outsetRect, occlusion.unoccludedLayerContentRect(parent, testRect));

        // Stuff outside the blur outsets is still occluded though.
        testRect = outsetRect;
        testRect.expand(1, 0);
        EXPECT_RECT_EQ(outsetRect, occlusion.unoccludedLayerContentRect(parent, testRect));
        testRect = outsetRect;
        testRect.expand(0, 1);
        EXPECT_RECT_EQ(outsetRect, occlusion.unoccludedLayerContentRect(parent, testRect));
        testRect = outsetRect;
        testRect.move(-1, 0);
        testRect.expand(1, 0);
        EXPECT_RECT_EQ(outsetRect, occlusion.unoccludedLayerContentRect(parent, testRect));
        testRect = outsetRect;
        testRect.move(0, -1);
        testRect.expand(0, 1);
        EXPECT_RECT_EQ(outsetRect, occlusion.unoccludedLayerContentRect(parent, testRect));
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter);

template<class Types>
class OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix scaleByHalf;
        scaleByHalf.scale(0.5);

        // Makes two surfaces that completely cover |parent|. The occlusion both above and below the filters will be reduced by each of them.
        typename Types::ContentLayerType* root = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(75, 75));
        typename Types::LayerType* parent = this->createSurface(root, scaleByHalf, FloatPoint(0, 0), IntSize(150, 150));
        parent->setMasksToBounds(true);
        typename Types::LayerType* filteredSurface1 = this->createDrawingLayer(parent, scaleByHalf, FloatPoint(0, 0), IntSize(300, 300), false);
        typename Types::LayerType* filteredSurface2 = this->createDrawingLayer(parent, scaleByHalf, FloatPoint(0, 0), IntSize(300, 300), false);
        typename Types::LayerType* occludingLayerAbove = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(100, 100), IntSize(50, 50), true);

        // Filters make the layers own surfaces.
        WebFilterOperations filters;
        filters.append(WebFilterOperation::createBlurFilter(1));
        filteredSurface1->setBackgroundFilters(filters);
        filteredSurface2->setBackgroundFilters(filters);

        // Save the distance of influence for the blur effect.
        int outsetTop, outsetRight, outsetBottom, outsetLeft;
        filters.getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

        this->calcDrawEtc(root);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->visitLayer(occludingLayerAbove, occlusion);
        EXPECT_RECT_EQ(IntRect(100 / 2, 100 / 2, 50 / 2, 50 / 2), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(100 / 2, 100 / 2, 50 / 2, 50 / 2), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        this->visitLayer(filteredSurface2, occlusion);
        this->visitContributingSurface(filteredSurface2, occlusion);
        this->visitLayer(filteredSurface1, occlusion);
        this->visitContributingSurface(filteredSurface1, occlusion);

        ASSERT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        ASSERT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

        // Test expectations in the target.
        IntRect expectedOcclusion = IntRect(100 / 2 + outsetRight * 2, 100 / 2 + outsetBottom * 2, 50 / 2 - (outsetLeft + outsetRight) * 2, 50 / 2 - (outsetTop + outsetBottom) * 2);
        EXPECT_RECT_EQ(expectedOcclusion, occlusion.occlusionInTargetSurface().rects()[0]);

        // Test expectations in the screen are the same as in the target, as the render surface is 1:1 with the screen.
        EXPECT_RECT_EQ(expectedOcclusion, occlusion.occlusionInScreenSpace().rects()[0]);
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
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 150));
        // We stick the filtered surface inside a clipping surface so that we can make sure the clip is honored when exposing pixels for
        // the background filter.
        typename Types::LayerType* clippingSurface = this->createSurface(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 70));
        clippingSurface->setMasksToBounds(true);
        typename Types::LayerType* filteredSurface = this->createDrawingLayer(clippingSurface, this->identityMatrix, FloatPoint(50, 50), IntSize(50, 50), false);
        this->createReplicaLayer(filteredSurface, this->identityMatrix, FloatPoint(150, 0), IntSize());
        typename Types::LayerType* occludingLayer1 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), IntSize(300, 50), true);
        typename Types::LayerType* occludingLayer2 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 100), IntSize(300, 50), true);
        typename Types::LayerType* occludingLayer3 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 50), IntSize(50, 50), true);
        typename Types::LayerType* occludingLayer4 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(100, 50), IntSize(100, 50), true);
        typename Types::LayerType* occludingLayer5 = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(250, 50), IntSize(50, 50), true);

        // Filters make the layer own a surface. This filter is large enough that it goes outside the bottom of the clippingSurface.
        WebFilterOperations filters;
        filters.append(WebFilterOperation::createBlurFilter(12));
        filteredSurface->setBackgroundFilters(filters);

        // Save the distance of influence for the blur effect.
        int outsetTop, outsetRight, outsetBottom, outsetLeft;
        filters.getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        // These layers occlude pixels directly beside the filteredSurface. Because filtered surface blends pixels in a radius, it will
        // need to see some of the pixels (up to radius far) underneath the occludingLayers.
        this->visitLayer(occludingLayer5, occlusion);
        this->visitLayer(occludingLayer4, occlusion);
        this->visitLayer(occludingLayer3, occlusion);
        this->visitLayer(occludingLayer2, occlusion);
        this->visitLayer(occludingLayer1, occlusion);

        EXPECT_RECT_EQ(IntRect(0, 0, 300, 150), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(5u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 300, 150), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(5u, occlusion.occlusionInTargetSurface().rects().size());

        // Everything outside the surface/replica is occluded but the surface/replica itself is not.
        this->enterLayer(filteredSurface, occlusion);
        EXPECT_RECT_EQ(IntRect(1, 0, 49, 50), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(1, 0, 50, 50)));
        EXPECT_RECT_EQ(IntRect(0, 1, 50, 49), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(0, 1, 50, 50)));
        EXPECT_RECT_EQ(IntRect(0, 0, 49, 50), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(-1, 0, 50, 50)));
        EXPECT_RECT_EQ(IntRect(0, 0, 50, 49), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(0, -1, 50, 50)));

        EXPECT_RECT_EQ(IntRect(150 + 1, 0, 49, 50), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(150 + 1, 0, 50, 50)));
        EXPECT_RECT_EQ(IntRect(150 + 0, 1, 50, 49), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(150 + 0, 1, 50, 50)));
        EXPECT_RECT_EQ(IntRect(150 + 0, 0, 49, 50), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(150 - 1, 0, 50, 50)));
        EXPECT_RECT_EQ(IntRect(150 + 0, 0, 50, 49), occlusion.unoccludedLayerContentRect(filteredSurface, IntRect(150 + 0, -1, 50, 50)));
        this->leaveLayer(filteredSurface, occlusion);

        // The filtered layer/replica does not occlude.
        EXPECT_RECT_EQ(IntRect(0, 0, 300, 150), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(5u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(0, 0, 0, 0), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(0u, occlusion.occlusionInTargetSurface().rects().size());

        // The surface has a background blur, so it needs pixels that are currently considered occluded in order to be drawn. So the pixels
        // it needs should be removed some the occluded area so that when we get to the parent they are drawn.
        this->visitContributingSurface(filteredSurface, occlusion);

        this->enterContributingSurface(clippingSurface, occlusion);
        EXPECT_RECT_EQ(IntRect(0, 0, 300, 150), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(5u, occlusion.occlusionInScreenSpace().rects().size());

        IntRect outsetRect;
        IntRect clippedOutsetRect;
        IntRect testRect;

        // Nothing in the (clipped) blur outsets for the filteredSurface is occluded.
        outsetRect = IntRect(50 - outsetLeft, 50 - outsetTop, 50 + outsetLeft + outsetRight, 50 + outsetTop + outsetBottom);
        clippedOutsetRect = intersection(outsetRect, IntRect(0 - outsetLeft, 0 - outsetTop, 300 + outsetLeft + outsetRight, 70 + outsetTop + outsetBottom));
        testRect = outsetRect;
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));

        // Stuff outside the (clipped) blur outsets is still occluded though.
        testRect = outsetRect;
        testRect.expand(1, 0);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.expand(0, 1);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.move(-1, 0);
        testRect.expand(1, 0);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.move(0, -1);
        testRect.expand(0, 1);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));

        // Nothing in the (clipped) blur outsets for the filteredSurface's replica is occluded.
        outsetRect = IntRect(200 - outsetLeft, 50 - outsetTop, 50 + outsetLeft + outsetRight, 50 + outsetTop + outsetBottom);
        clippedOutsetRect = intersection(outsetRect, IntRect(0 - outsetLeft, 0 - outsetTop, 300 + outsetLeft + outsetRight, 70 + outsetTop + outsetBottom));
        testRect = outsetRect;
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));

        // Stuff outside the (clipped) blur outsets is still occluded though.
        testRect = outsetRect;
        testRect.expand(1, 0);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.expand(0, 1);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.move(-1, 0);
        testRect.expand(1, 0);
        EXPECT_RECT_EQ(clippedOutsetRect, occlusion.unoccludedLayerContentRect(clippingSurface, testRect));
        testRect = outsetRect;
        testRect.move(0, -1);
        testRect.expand(0, 1);
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
        WebTransformationMatrix scaleByHalf;
        scaleByHalf.scale(0.5);

        // Make a surface and its replica, each 50x50, with a smaller 30x30 layer centered below each.
        // The surface is scaled to test that the pixel moving is done in the target space, where the background filter is applied, but the surface
        // appears at 50, 50 and the replica at 200, 50.
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 150));
        typename Types::LayerType* behindSurfaceLayer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(60, 60), IntSize(30, 30), true);
        typename Types::LayerType* behindReplicaLayer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(210, 60), IntSize(30, 30), true);
        typename Types::LayerType* filteredSurface = this->createDrawingLayer(parent, scaleByHalf, FloatPoint(50, 50), IntSize(100, 100), false);
        this->createReplicaLayer(filteredSurface, this->identityMatrix, FloatPoint(300, 0), IntSize());

        // Filters make the layer own a surface.
        WebFilterOperations filters;
        filters.append(WebFilterOperation::createBlurFilter(3));
        filteredSurface->setBackgroundFilters(filters);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        // The surface has a background blur, so it blurs non-opaque pixels below it.
        this->visitLayer(filteredSurface, occlusion);
        this->visitContributingSurface(filteredSurface, occlusion);

        this->visitLayer(behindReplicaLayer, occlusion);
        this->visitLayer(behindSurfaceLayer, occlusion);

        // The layers behind the surface are not blurred, and their occlusion does not change, until we leave the surface.
        // So it should not be modified by the filter here.
        IntRect occlusionBehindSurface = IntRect(60, 60, 30, 30);
        IntRect occlusionBehindReplica = IntRect(210, 60, 30, 30);

        IntRect expectedOpaqueBounds = unionRect(occlusionBehindSurface, occlusionBehindReplica);
        EXPECT_RECT_EQ(expectedOpaqueBounds, occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(expectedOpaqueBounds, occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInTargetSurface().rects().size());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestDontReduceOcclusionBelowBackgroundFilter);

template<class Types>
class OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix scaleByHalf;
        scaleByHalf.scale(0.5);

        // Make a surface and its replica, each 50x50, that are completely occluded by opaque layers which are above them in the z-order.
        // The surface is scaled to test that the pixel moving is done in the target space, where the background filter is applied, but the surface
        // appears at 50, 50 and the replica at 200, 50.
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 150));
        typename Types::LayerType* filteredSurface = this->createDrawingLayer(parent, scaleByHalf, FloatPoint(50, 50), IntSize(100, 100), false);
        this->createReplicaLayer(filteredSurface, this->identityMatrix, FloatPoint(300, 0), IntSize());
        typename Types::LayerType* aboveSurfaceLayer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(50, 50), IntSize(50, 50), true);
        typename Types::LayerType* aboveReplicaLayer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(200, 50), IntSize(50, 50), true);

        // Filters make the layer own a surface.
        WebFilterOperations filters;
        filters.append(WebFilterOperation::createBlurFilter(3));
        filteredSurface->setBackgroundFilters(filters);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

        this->visitLayer(aboveReplicaLayer, occlusion);
        this->visitLayer(aboveSurfaceLayer, occlusion);

        // The surface has a background blur, so it blurs non-opaque pixels below it.
        this->visitLayer(filteredSurface, occlusion);
        this->visitContributingSurface(filteredSurface, occlusion);

        // The filter is completely occluded, so it should not blur anything and reduce any occlusion.
        IntRect occlusionAboveSurface = IntRect(50, 50, 50, 50);
        IntRect occlusionAboveReplica = IntRect(200, 50, 50, 50);

        IntRect expectedOpaqueBounds = unionRect(occlusionAboveSurface, occlusionAboveReplica);
        EXPECT_RECT_EQ(expectedOpaqueBounds, occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(expectedOpaqueBounds, occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(2u, occlusion.occlusionInTargetSurface().rects().size());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded);

template<class Types>
class OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded : public OcclusionTrackerTest<Types> {
protected:
    OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded(bool opaqueLayers) : OcclusionTrackerTest<Types>(opaqueLayers) {}
    void runMyTest()
    {
        WebTransformationMatrix scaleByHalf;
        scaleByHalf.scale(0.5);

        // Make a surface and its replica, each 50x50, that are partially occluded by opaque layers which are above them in the z-order.
        // The surface is scaled to test that the pixel moving is done in the target space, where the background filter is applied, but the surface
        // appears at 50, 50 and the replica at 200, 50.
        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(300, 150));
        typename Types::LayerType* filteredSurface = this->createDrawingLayer(parent, scaleByHalf, FloatPoint(50, 50), IntSize(100, 100), false);
        this->createReplicaLayer(filteredSurface, this->identityMatrix, FloatPoint(300, 0), IntSize());
        typename Types::LayerType* aboveSurfaceLayer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(70, 50), IntSize(30, 50), true);
        typename Types::LayerType* aboveReplicaLayer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(200, 50), IntSize(30, 50), true);
        typename Types::LayerType* besideSurfaceLayer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(90, 40), IntSize(10, 10), true);
        typename Types::LayerType* besideReplicaLayer = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(200, 40), IntSize(10, 10), true);

        // Filters make the layer own a surface.
        WebFilterOperations filters;
        filters.append(WebFilterOperation::createBlurFilter(3));
        filteredSurface->setBackgroundFilters(filters);

        // Save the distance of influence for the blur effect.
        int outsetTop, outsetRight, outsetBottom, outsetLeft;
        filters.getOutsets(outsetTop, outsetRight, outsetBottom, outsetLeft);

        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));

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
        IntRect occlusionAboveSurface = IntRect(70 + outsetRight, 50, 30 - outsetRight, 50);
        IntRect occlusionAboveReplica = IntRect(200, 50, 30 - outsetLeft, 50);
        IntRect occlusionBesideSurface = IntRect(90, 40, 10, 10);
        IntRect occlusionBesideReplica = IntRect(200, 40, 10, 10);

        Region expectedOcclusion;
        expectedOcclusion.unite(occlusionAboveSurface);
        expectedOcclusion.unite(occlusionAboveReplica);
        expectedOcclusion.unite(occlusionBesideSurface);
        expectedOcclusion.unite(occlusionBesideReplica);

        ASSERT_EQ(expectedOcclusion.rects().size(), occlusion.occlusionInTargetSurface().rects().size());
        ASSERT_EQ(expectedOcclusion.rects().size(), occlusion.occlusionInScreenSpace().rects().size());

        for (size_t i = 0; i < expectedOcclusion.rects().size(); ++i) {
            IntRect expectedRect = expectedOcclusion.rects()[i];
            IntRect screenRect = occlusion.occlusionInScreenSpace().rects()[i];
            IntRect targetRect = occlusion.occlusionInTargetSurface().rects()[i];
            EXPECT_EQ(expectedRect, screenRect);
            EXPECT_EQ(expectedRect, targetRect);
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
        IntSize trackingSize(100, 100);
        IntSize belowTrackingSize(99, 99);

        typename Types::ContentLayerType* parent = this->createRoot(this->identityMatrix, FloatPoint(0, 0), IntSize(400, 400));
        typename Types::LayerType* large = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), trackingSize, true);
        typename Types::LayerType* small = this->createDrawingLayer(parent, this->identityMatrix, FloatPoint(0, 0), belowTrackingSize, true);
        this->calcDrawEtc(parent);

        TestOcclusionTrackerWithClip<typename Types::LayerType, typename Types::RenderSurfaceType> occlusion(IntRect(0, 0, 1000, 1000));
        occlusion.setLayerClipRect(IntRect(0, 0, 1000, 1000));
        occlusion.setMinimumTrackingSize(trackingSize);

        // The small layer is not tracked because it is too small.
        this->visitLayer(small, occlusion);

        EXPECT_RECT_EQ(IntRect(), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(0u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(0u, occlusion.occlusionInTargetSurface().rects().size());

        // The large layer is tracked as it is large enough.
        this->visitLayer(large, occlusion);

        EXPECT_RECT_EQ(IntRect(IntPoint(), trackingSize), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
        EXPECT_RECT_EQ(IntRect(IntPoint(), trackingSize), occlusion.occlusionInTargetSurface().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
    }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestMinimumTrackingSize);

} // namespace
