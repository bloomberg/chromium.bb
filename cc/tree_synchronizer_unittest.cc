// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/tree_synchronizer.h"

#include "Region.h"
#include "cc/layer.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_impl.h"
#include "cc/proxy.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/animation_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;
using namespace WebKitTests;

namespace {

class MockLayerImpl : public LayerImpl {
public:
    static scoped_ptr<MockLayerImpl> create(int layerId)
    {
        return make_scoped_ptr(new MockLayerImpl(layerId));
    }
    virtual ~MockLayerImpl()
    {
        if (m_layerImplDestructionList)
            m_layerImplDestructionList->append(id());
    }

    void setLayerImplDestructionList(Vector<int>* list) { m_layerImplDestructionList = list; }

private:
    MockLayerImpl(int layerId)
        : LayerImpl(layerId)
        , m_layerImplDestructionList(0)
    {
    }

    Vector<int>* m_layerImplDestructionList;
};

class MockLayer : public Layer {
public:
    static scoped_refptr<MockLayer> create(Vector<int>* layerImplDestructionList)
    {
        return make_scoped_refptr(new MockLayer(layerImplDestructionList));
    }

    virtual scoped_ptr<LayerImpl> createLayerImpl() OVERRIDE
    {
        return MockLayerImpl::create(m_layerId).PassAs<LayerImpl>();
    }

    virtual void pushPropertiesTo(LayerImpl* layerImpl) OVERRIDE
    {
        Layer::pushPropertiesTo(layerImpl);

        MockLayerImpl* mockLayerImpl = static_cast<MockLayerImpl*>(layerImpl);
        mockLayerImpl->setLayerImplDestructionList(m_layerImplDestructionList);
    }

private:
    MockLayer(Vector<int>* layerImplDestructionList)
        : Layer()
        , m_layerImplDestructionList(layerImplDestructionList)
    {
    }
    virtual ~MockLayer() { }

    Vector<int>* m_layerImplDestructionList;
};

class FakeLayerAnimationController : public LayerAnimationController {
public:
    static scoped_ptr<FakeLayerAnimationController> create(LayerAnimationControllerClient* client)
    {
        return make_scoped_ptr(new FakeLayerAnimationController(client));
    }

    bool synchronizedAnimations() const { return m_synchronizedAnimations; }

private:
    explicit FakeLayerAnimationController(LayerAnimationControllerClient* client)
        : LayerAnimationController(client)
        , m_synchronizedAnimations(false)
    {
    }

    virtual void pushAnimationUpdatesTo(LayerAnimationController* controllerImpl)
    {
        LayerAnimationController::pushAnimationUpdatesTo(controllerImpl);
        m_synchronizedAnimations = true;
    }

    bool m_synchronizedAnimations;
};

void expectTreesAreIdentical(Layer* layer, LayerImpl* layerImpl, LayerTreeHostImpl* hostImpl)
{
    ASSERT_TRUE(layer);
    ASSERT_TRUE(layerImpl);

    EXPECT_EQ(layer->id(), layerImpl->id());
    EXPECT_EQ(layerImpl->layerTreeHostImpl(), hostImpl);

    EXPECT_EQ(layer->nonFastScrollableRegion(), layerImpl->nonFastScrollableRegion());

    ASSERT_EQ(!!layer->maskLayer(), !!layerImpl->maskLayer());
    if (layer->maskLayer())
        expectTreesAreIdentical(layer->maskLayer(), layerImpl->maskLayer(), hostImpl);

    ASSERT_EQ(!!layer->replicaLayer(), !!layerImpl->replicaLayer());
    if (layer->replicaLayer())
        expectTreesAreIdentical(layer->replicaLayer(), layerImpl->replicaLayer(), hostImpl);

    const std::vector<scoped_refptr<Layer> >& layerChildren = layer->children();
    const ScopedPtrVector<LayerImpl>& layerImplChildren = layerImpl->children();

    ASSERT_EQ(layerChildren.size(), layerImplChildren.size());

    for (size_t i = 0; i < layerChildren.size(); ++i)
        expectTreesAreIdentical(layerChildren[i].get(), layerImplChildren[i], hostImpl);
}

// Attempts to synchronizes a null tree. This should not crash, and should
// return a null tree.
TEST(TreeSynchronizerTest, syncNullTree)
{
    DebugScopedSetImplThread impl;

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(0, scoped_ptr<LayerImpl>(), 0);

    EXPECT_TRUE(!layerImplTreeRoot.get());
}

// Constructs a very simple tree and synchronizes it without trying to reuse any preexisting layers.
TEST(TreeSynchronizerTest, syncSimpleTreeFromEmpty)
{
    DebugScopedSetImplThread impl;

    LayerTreeSettings settings;
    scoped_ptr<LayerTreeHostImpl> hostImpl = LayerTreeHostImpl::create(settings, 0);

    scoped_refptr<Layer> layerTreeRoot = Layer::create();
    layerTreeRoot->addChild(Layer::create());
    layerTreeRoot->addChild(Layer::create());

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.get());

    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());
}

// Constructs a very simple tree and synchronizes it attempting to reuse some layers
TEST(TreeSynchronizerTest, syncSimpleTreeReusingLayers)
{
    DebugScopedSetImplThread impl;
    Vector<int> layerImplDestructionList;

    LayerTreeSettings settings;
    scoped_ptr<LayerTreeHostImpl> hostImpl = LayerTreeHostImpl::create(settings, 0);

    scoped_refptr<Layer> layerTreeRoot = MockLayer::create(&layerImplDestructionList);
    layerTreeRoot->addChild(MockLayer::create(&layerImplDestructionList));
    layerTreeRoot->addChild(MockLayer::create(&layerImplDestructionList));

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    // Add a new layer to the Layer side
    layerTreeRoot->children()[0]->addChild(MockLayer::create(&layerImplDestructionList));
    // Remove one.
    layerTreeRoot->children()[1]->removeFromParent();
    int secondLayerImplId = layerImplTreeRoot->children()[1]->id();

    // Synchronize again. After the sync the trees should be equivalent and we should have created and destroyed one LayerImpl.
    layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    ASSERT_EQ(1u, layerImplDestructionList.size());
    EXPECT_EQ(secondLayerImplId, layerImplDestructionList[0]);
}

// Constructs a very simple tree and checks that a stacking-order change is tracked properly.
TEST(TreeSynchronizerTest, syncSimpleTreeAndTrackStackingOrderChange)
{
    DebugScopedSetImplThread impl;
    Vector<int> layerImplDestructionList;

    LayerTreeSettings settings;
    scoped_ptr<LayerTreeHostImpl> hostImpl = LayerTreeHostImpl::create(settings, 0);

    // Set up the tree and sync once. child2 needs to be synced here, too, even though we
    // remove it to set up the intended scenario.
    scoped_refptr<Layer> layerTreeRoot = MockLayer::create(&layerImplDestructionList);
    scoped_refptr<Layer> child2 = MockLayer::create(&layerImplDestructionList);
    layerTreeRoot->addChild(MockLayer::create(&layerImplDestructionList));
    layerTreeRoot->addChild(child2);
    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());
    layerImplTreeRoot->resetAllChangeTrackingForSubtree();

    // re-insert the layer and sync again.
    child2->removeFromParent();
    layerTreeRoot->addChild(child2);
    layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    // Check that the impl thread properly tracked the change.
    EXPECT_FALSE(layerImplTreeRoot->layerPropertyChanged());
    EXPECT_FALSE(layerImplTreeRoot->children()[0]->layerPropertyChanged());
    EXPECT_TRUE(layerImplTreeRoot->children()[1]->layerPropertyChanged());
}

TEST(TreeSynchronizerTest, syncSimpleTreeAndProperties)
{
    DebugScopedSetImplThread impl;

    LayerTreeSettings settings;
    scoped_ptr<LayerTreeHostImpl> hostImpl = LayerTreeHostImpl::create(settings, 0);

    scoped_refptr<Layer> layerTreeRoot = Layer::create();
    layerTreeRoot->addChild(Layer::create());
    layerTreeRoot->addChild(Layer::create());

    // Pick some random properties to set. The values are not important, we're just testing that at least some properties are making it through.
    FloatPoint rootPosition = FloatPoint(2.3f, 7.4f);
    layerTreeRoot->setPosition(rootPosition);

    float firstChildOpacity = 0.25f;
    layerTreeRoot->children()[0]->setOpacity(firstChildOpacity);

    IntSize secondChildBounds = IntSize(25, 53);
    layerTreeRoot->children()[1]->setBounds(secondChildBounds);

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    // Check that the property values we set on the Layer tree are reflected in the LayerImpl tree.
    FloatPoint rootLayerImplPosition = layerImplTreeRoot->position();
    EXPECT_EQ(rootPosition.x(), rootLayerImplPosition.x());
    EXPECT_EQ(rootPosition.y(), rootLayerImplPosition.y());

    EXPECT_EQ(firstChildOpacity, layerImplTreeRoot->children()[0]->opacity());

    IntSize secondLayerImplChildBounds = layerImplTreeRoot->children()[1]->bounds();
    EXPECT_EQ(secondChildBounds.width(), secondLayerImplChildBounds.width());
    EXPECT_EQ(secondChildBounds.height(), secondLayerImplChildBounds.height());
}

TEST(TreeSynchronizerTest, reuseLayerImplsAfterStructuralChange)
{
    DebugScopedSetImplThread impl;
    Vector<int> layerImplDestructionList;

    LayerTreeSettings settings;
    scoped_ptr<LayerTreeHostImpl> hostImpl = LayerTreeHostImpl::create(settings, 0);

    // Set up a tree with this sort of structure:
    // root --- A --- B ---+--- C
    //                     |
    //                     +--- D
    scoped_refptr<Layer> layerTreeRoot = MockLayer::create(&layerImplDestructionList);
    layerTreeRoot->addChild(MockLayer::create(&layerImplDestructionList));

    scoped_refptr<Layer> layerA = layerTreeRoot->children()[0].get();
    layerA->addChild(MockLayer::create(&layerImplDestructionList));

    scoped_refptr<Layer> layerB = layerA->children()[0].get();
    layerB->addChild(MockLayer::create(&layerImplDestructionList));

    scoped_refptr<Layer> layerC = layerB->children()[0].get();
    layerB->addChild(MockLayer::create(&layerImplDestructionList));
    scoped_refptr<Layer> layerD = layerB->children()[1].get();

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    // Now restructure the tree to look like this:
    // root --- D ---+--- A
    //               |
    //               +--- C --- B
    layerTreeRoot->removeAllChildren();
    layerD->removeAllChildren();
    layerTreeRoot->addChild(layerD);
    layerA->removeAllChildren();
    layerD->addChild(layerA);
    layerC->removeAllChildren();
    layerD->addChild(layerC);
    layerB->removeAllChildren();
    layerC->addChild(layerB);

    // After another synchronize our trees should match and we should not have destroyed any LayerImpls
    layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    EXPECT_EQ(0u, layerImplDestructionList.size());
}

// Constructs a very simple tree, synchronizes it, then synchronizes to a totally new tree. All layers from the old tree should be deleted.
TEST(TreeSynchronizerTest, syncSimpleTreeThenDestroy)
{
    DebugScopedSetImplThread impl;
    Vector<int> layerImplDestructionList;

    LayerTreeSettings settings;
    scoped_ptr<LayerTreeHostImpl> hostImpl = LayerTreeHostImpl::create(settings, 0);

    scoped_refptr<Layer> oldLayerTreeRoot = MockLayer::create(&layerImplDestructionList);
    oldLayerTreeRoot->addChild(MockLayer::create(&layerImplDestructionList));
    oldLayerTreeRoot->addChild(MockLayer::create(&layerImplDestructionList));

    int oldTreeRootLayerId = oldLayerTreeRoot->id();
    int oldTreeFirstChildLayerId = oldLayerTreeRoot->children()[0]->id();
    int oldTreeSecondChildLayerId = oldLayerTreeRoot->children()[1]->id();

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(oldLayerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.get());
    expectTreesAreIdentical(oldLayerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    // Remove all children on the Layer side.
    oldLayerTreeRoot->removeAllChildren();

    // Synchronize again. After the sync all LayerImpls from the old tree should be deleted.
    scoped_refptr<Layer> newLayerTreeRoot = Layer::create();
    layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(newLayerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(newLayerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    ASSERT_EQ(3u, layerImplDestructionList.size());
    EXPECT_TRUE(layerImplDestructionList.contains(oldTreeRootLayerId));
    EXPECT_TRUE(layerImplDestructionList.contains(oldTreeFirstChildLayerId));
    EXPECT_TRUE(layerImplDestructionList.contains(oldTreeSecondChildLayerId));
}

// Constructs+syncs a tree with mask, replica, and replica mask layers.
TEST(TreeSynchronizerTest, syncMaskReplicaAndReplicaMaskLayers)
{
    DebugScopedSetImplThread impl;

    LayerTreeSettings settings;
    scoped_ptr<LayerTreeHostImpl> hostImpl = LayerTreeHostImpl::create(settings, 0);

    scoped_refptr<Layer> layerTreeRoot = Layer::create();
    layerTreeRoot->addChild(Layer::create());
    layerTreeRoot->addChild(Layer::create());
    layerTreeRoot->addChild(Layer::create());

    // First child gets a mask layer.
    scoped_refptr<Layer> maskLayer = Layer::create();
    layerTreeRoot->children()[0]->setMaskLayer(maskLayer.get());

    // Second child gets a replica layer.
    scoped_refptr<Layer> replicaLayer = Layer::create();
    layerTreeRoot->children()[1]->setReplicaLayer(replicaLayer.get());

    // Third child gets a replica layer with a mask layer.
    scoped_refptr<Layer> replicaLayerWithMask = Layer::create();
    scoped_refptr<Layer> replicaMaskLayer = Layer::create();
    replicaLayerWithMask->setMaskLayer(replicaMaskLayer.get());
    layerTreeRoot->children()[2]->setReplicaLayer(replicaLayerWithMask.get());

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.get());

    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    // Remove the mask layer.
    layerTreeRoot->children()[0]->setMaskLayer(0);
    layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    // Remove the replica layer.
    layerTreeRoot->children()[1]->setReplicaLayer(0);
    layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());

    // Remove the replica mask.
    replicaLayerWithMask->setMaskLayer(0);
    layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), layerImplTreeRoot.get(), hostImpl.get());
}

TEST(TreeSynchronizerTest, synchronizeAnimations)
{
    DebugScopedSetImplThread impl;

    LayerTreeSettings settings;
    scoped_ptr<LayerTreeHostImpl> hostImpl = LayerTreeHostImpl::create(settings, 0);

    scoped_refptr<Layer> layerTreeRoot = Layer::create();

    FakeLayerAnimationControllerClient dummy;
    layerTreeRoot->setLayerAnimationController(FakeLayerAnimationController::create(&dummy).PassAs<LayerAnimationController>());

    EXPECT_FALSE(static_cast<FakeLayerAnimationController*>(layerTreeRoot->layerAnimationController())->synchronizedAnimations());

    scoped_ptr<LayerImpl> layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<LayerImpl>(), hostImpl.get());
    layerImplTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), layerImplTreeRoot.Pass(), hostImpl.get());

    EXPECT_TRUE(static_cast<FakeLayerAnimationController*>(layerTreeRoot->layerAnimationController())->synchronizedAnimations());
}

} // namespace
