// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer.h"

#include "cc/keyframed_animation_curve.h"
#include "cc/layer_impl.h"
#include "cc/layer_painter.h"
#include "cc/layer_tree_host.h"
#include "cc/math_util.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::_;

#define EXPECT_SET_NEEDS_COMMIT(expect, codeToTest) do {                 \
        EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times((expect)); \
        codeToTest;                                                      \
        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());         \
    } while (0)

#define EXPECT_SET_NEEDS_FULL_TREE_SYNC(expect, codeToTest) do {               \
        EXPECT_CALL(*m_layerTreeHost, setNeedsFullTreeSync()).Times((expect)); \
        codeToTest;                                                            \
        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());               \
    } while (0)


namespace cc {
namespace {

class MockLayerImplTreeHost : public LayerTreeHost {
public:
    MockLayerImplTreeHost()
        : LayerTreeHost(&m_fakeClient, LayerTreeSettings())
    {
        initialize(scoped_ptr<Thread>(NULL));
    }

    MOCK_METHOD0(setNeedsCommit, void());
    MOCK_METHOD0(setNeedsFullTreeSync, void());

private:
    FakeLayerImplTreeHostClient m_fakeClient;
};

class MockLayerPainter : public LayerPainter {
public:
    virtual void paint(SkCanvas*, gfx::Rect, gfx::RectF&) OVERRIDE { }
};


class LayerTest : public testing::Test {
public:
    LayerTest()
        : m_hostImpl(&m_proxy)
    {
    }

protected:
    virtual void SetUp()
    {
        m_layerTreeHost.reset(new StrictMock<MockLayerImplTreeHost>);
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
        EXPECT_CALL(*m_layerTreeHost, setNeedsFullTreeSync()).Times(AnyNumber());
        m_parent = NULL;
        m_child1 = NULL;
        m_child2 = NULL;
        m_child3 = NULL;
        m_grandChild1 = NULL;
        m_grandChild2 = NULL;
        m_grandChild3 = NULL;

        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();
    }

    void verifyTestTreeInitialState() const
    {
        ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
        EXPECT_EQ(m_child1, m_parent->children()[0]);
        EXPECT_EQ(m_child2, m_parent->children()[1]);
        EXPECT_EQ(m_child3, m_parent->children()[2]);
        EXPECT_EQ(m_parent.get(), m_child1->parent());
        EXPECT_EQ(m_parent.get(), m_child2->parent());
        EXPECT_EQ(m_parent.get(), m_child3->parent());

        ASSERT_EQ(static_cast<size_t>(2), m_child1->children().size());
        EXPECT_EQ(m_grandChild1, m_child1->children()[0]);
        EXPECT_EQ(m_grandChild2, m_child1->children()[1]);
        EXPECT_EQ(m_child1.get(), m_grandChild1->parent());
        EXPECT_EQ(m_child1.get(), m_grandChild2->parent());

        ASSERT_EQ(static_cast<size_t>(1), m_child2->children().size());
        EXPECT_EQ(m_grandChild3, m_child2->children()[0]);
        EXPECT_EQ(m_child2.get(), m_grandChild3->parent());

        ASSERT_EQ(static_cast<size_t>(0), m_child3->children().size());
    }

    void createSimpleTestTree()
    {
        m_parent = Layer::create();
        m_child1 = Layer::create();
        m_child2 = Layer::create();
        m_child3 = Layer::create();
        m_grandChild1 = Layer::create();
        m_grandChild2 = Layer::create();
        m_grandChild3 = Layer::create();

        EXPECT_CALL(*m_layerTreeHost, setNeedsFullTreeSync()).Times(AnyNumber());
        m_layerTreeHost->setRootLayer(m_parent);

        m_parent->addChild(m_child1);
        m_parent->addChild(m_child2);
        m_parent->addChild(m_child3);
        m_child1->addChild(m_grandChild1);
        m_child1->addChild(m_grandChild2);
        m_child2->addChild(m_grandChild3);

        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

        verifyTestTreeInitialState();
    }

    FakeImplProxy m_proxy;
    FakeLayerTreeHostImpl m_hostImpl;

    scoped_ptr<StrictMock<MockLayerImplTreeHost> > m_layerTreeHost;
    scoped_refptr<Layer> m_parent;
    scoped_refptr<Layer> m_child1;
    scoped_refptr<Layer> m_child2;
    scoped_refptr<Layer> m_child3;
    scoped_refptr<Layer> m_grandChild1;
    scoped_refptr<Layer> m_grandChild2;
    scoped_refptr<Layer> m_grandChild3;
};

TEST_F(LayerTest, basicCreateAndDestroy)
{
    scoped_refptr<Layer> testLayer = Layer::create();
    ASSERT_TRUE(testLayer);

    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(0);
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
}

TEST_F(LayerTest, addAndRemoveChild)
{
    scoped_refptr<Layer> parent = Layer::create();
    scoped_refptr<Layer> child = Layer::create();

    // Upon creation, layers should not have children or parent.
    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());
    EXPECT_FALSE(child->parent());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, m_layerTreeHost->setRootLayer(parent));
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->addChild(child));

    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child.get(), parent->children()[0]);
    EXPECT_EQ(parent.get(), child->parent());
    EXPECT_EQ(parent.get(), child->rootLayer());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), child->removeFromParent());
}

TEST_F(LayerTest, addSameChildTwice)
{
    EXPECT_CALL(*m_layerTreeHost, setNeedsFullTreeSync()).Times(AtLeast(1));

    scoped_refptr<Layer> parent = Layer::create();
    scoped_refptr<Layer> child = Layer::create();

    m_layerTreeHost->setRootLayer(parent);

    ASSERT_EQ(0u, parent->children().size());

    parent->addChild(child);
    ASSERT_EQ(1u, parent->children().size());
    EXPECT_EQ(parent.get(), child->parent());

    parent->addChild(child);
    ASSERT_EQ(1u, parent->children().size());
    EXPECT_EQ(parent.get(), child->parent());
}

TEST_F(LayerTest, insertChild)
{
    scoped_refptr<Layer> parent = Layer::create();
    scoped_refptr<Layer> child1 = Layer::create();
    scoped_refptr<Layer> child2 = Layer::create();
    scoped_refptr<Layer> child3 = Layer::create();
    scoped_refptr<Layer> child4 = Layer::create();

    parent->setLayerTreeHost(m_layerTreeHost.get());

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    // Case 1: inserting to empty list.
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->insertChild(child3, 0));
    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child3, parent->children()[0]);
    EXPECT_EQ(parent.get(), child3->parent());

    // Case 2: inserting to beginning of list
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->insertChild(child1, 0));
    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child3, parent->children()[1]);
    EXPECT_EQ(parent.get(), child1->parent());

    // Case 3: inserting to middle of list
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->insertChild(child2, 1));
    ASSERT_EQ(static_cast<size_t>(3), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
    EXPECT_EQ(child3, parent->children()[2]);
    EXPECT_EQ(parent.get(), child2->parent());

    // Case 4: inserting to end of list
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->insertChild(child4, 3));

    ASSERT_EQ(static_cast<size_t>(4), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
    EXPECT_EQ(child3, parent->children()[2]);
    EXPECT_EQ(child4, parent->children()[3]);
    EXPECT_EQ(parent.get(), child4->parent());

    EXPECT_CALL(*m_layerTreeHost, setNeedsFullTreeSync()).Times(AtLeast(1));
}

TEST_F(LayerTest, insertChildPastEndOfList)
{
    scoped_refptr<Layer> parent = Layer::create();
    scoped_refptr<Layer> child1 = Layer::create();
    scoped_refptr<Layer> child2 = Layer::create();

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    // insert to an out-of-bounds index
    parent->insertChild(child1, 53);

    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);

    // insert another child to out-of-bounds, when list is not already empty.
    parent->insertChild(child2, 2459);

    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
}

TEST_F(LayerTest, insertSameChildTwice)
{
    scoped_refptr<Layer> parent = Layer::create();
    scoped_refptr<Layer> child1 = Layer::create();
    scoped_refptr<Layer> child2 = Layer::create();

    parent->setLayerTreeHost(m_layerTreeHost.get());

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->insertChild(child1, 0));
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->insertChild(child2, 1));

    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);

    // Inserting the same child again should cause the child to be removed and re-inserted at the new location.
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), parent->insertChild(child1, 1));

    // child1 should now be at the end of the list.
    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child2, parent->children()[0]);
    EXPECT_EQ(child1, parent->children()[1]);

    EXPECT_CALL(*m_layerTreeHost, setNeedsFullTreeSync()).Times(AtLeast(1));
}

TEST_F(LayerTest, replaceChildWithNewChild)
{
    createSimpleTestTree();
    scoped_refptr<Layer> child4 = Layer::create();

    EXPECT_FALSE(child4->parent());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), m_parent->replaceChild(m_child2.get(), child4));

    ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
    EXPECT_EQ(m_child1, m_parent->children()[0]);
    EXPECT_EQ(child4, m_parent->children()[1]);
    EXPECT_EQ(m_child3, m_parent->children()[2]);
    EXPECT_EQ(m_parent.get(), child4->parent());

    EXPECT_FALSE(m_child2->parent());
}

TEST_F(LayerTest, replaceChildWithNewChildThatHasOtherParent)
{
    createSimpleTestTree();

    // create another simple tree with testLayer and child4.
    scoped_refptr<Layer> testLayer = Layer::create();
    scoped_refptr<Layer> child4 = Layer::create();
    testLayer->addChild(child4);
    ASSERT_EQ(static_cast<size_t>(1), testLayer->children().size());
    EXPECT_EQ(child4, testLayer->children()[0]);
    EXPECT_EQ(testLayer.get(), child4->parent());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), m_parent->replaceChild(m_child2.get(), child4));

    ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
    EXPECT_EQ(m_child1, m_parent->children()[0]);
    EXPECT_EQ(child4, m_parent->children()[1]);
    EXPECT_EQ(m_child3, m_parent->children()[2]);
    EXPECT_EQ(m_parent.get(), child4->parent());

    // testLayer should no longer have child4,
    // and child2 should no longer have a parent.
    ASSERT_EQ(static_cast<size_t>(0), testLayer->children().size());
    EXPECT_FALSE(m_child2->parent());
}

TEST_F(LayerTest, replaceChildWithSameChild)
{
    createSimpleTestTree();

    // setNeedsFullTreeSync / setNeedsCommit should not be called because its the same child
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsFullTreeSync()).Times(0);
    m_parent->replaceChild(m_child2.get(), m_child2);

    verifyTestTreeInitialState();
}

TEST_F(LayerTest, removeAllChildren)
{
    createSimpleTestTree();

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(3), m_parent->removeAllChildren());

    ASSERT_EQ(static_cast<size_t>(0), m_parent->children().size());
    EXPECT_FALSE(m_child1->parent());
    EXPECT_FALSE(m_child2->parent());
    EXPECT_FALSE(m_child3->parent());
}

TEST_F(LayerTest, setChildren)
{
    scoped_refptr<Layer> oldParent = Layer::create();
    scoped_refptr<Layer> newParent = Layer::create();

    scoped_refptr<Layer> child1 = Layer::create();
    scoped_refptr<Layer> child2 = Layer::create();

    std::vector<scoped_refptr<Layer> > newChildren;
    newChildren.push_back(child1);
    newChildren.push_back(child2);

    // Set up and verify initial test conditions: child1 has a parent, child2 has no parent.
    oldParent->addChild(child1);
    ASSERT_EQ(static_cast<size_t>(0), newParent->children().size());
    EXPECT_EQ(oldParent.get(), child1->parent());
    EXPECT_FALSE(child2->parent());

    newParent->setLayerTreeHost(m_layerTreeHost.get());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), newParent->setChildren(newChildren));

    ASSERT_EQ(static_cast<size_t>(2), newParent->children().size());
    EXPECT_EQ(newParent.get(), child1->parent());
    EXPECT_EQ(newParent.get(), child2->parent());

    EXPECT_CALL(*m_layerTreeHost, setNeedsFullTreeSync()).Times(AtLeast(1));
}

TEST_F(LayerTest, getRootLayerAfterTreeManipulations)
{
    createSimpleTestTree();

    // For this test we don't care about setNeedsFullTreeSync calls.
    EXPECT_CALL(*m_layerTreeHost, setNeedsFullTreeSync()).Times(AnyNumber());

    scoped_refptr<Layer> child4 = Layer::create();

    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(child4.get(),   child4->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild3->rootLayer());

    m_child1->removeFromParent();

    // child1 and its children, grandChild1 and grandChild2 are now on a separate subtree.
    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_child1.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(child4.get(), child4->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild3->rootLayer());

    m_grandChild3->addChild(child4);

    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_child1.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(m_parent.get(), child4->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild3->rootLayer());

    m_child2->replaceChild(m_grandChild3.get(), m_child1);

    // grandChild3 gets orphaned and the child1 subtree gets planted back into the tree under child2.
    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(m_grandChild3.get(), child4->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_grandChild3.get(), m_grandChild3->rootLayer());
}

TEST_F(LayerTest, checkSetNeedsDisplayCausesCorrectBehavior)
{
    // The semantics for setNeedsDisplay which are tested here:
    //   1. sets needsDisplay flag appropriately.
    //   2. indirectly calls setNeedsCommit, exactly once for each call to setNeedsDisplay.

    scoped_refptr<Layer> testLayer = Layer::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setIsDrawable(true));

    gfx::Size testBounds = gfx::Size(501, 508);

    gfx::RectF dirty1 = gfx::RectF(10, 15, 1, 2);
    gfx::RectF dirty2 = gfx::RectF(20, 25, 3, 4);
    gfx::RectF emptyDirtyRect = gfx::RectF(40, 45, 0, 0);
    gfx::RectF outOfBoundsDirtyRect = gfx::RectF(400, 405, 500, 502);

    // Before anything, testLayer should not be dirty.
    EXPECT_FALSE(testLayer->needsDisplay());

    // This is just initialization, but setNeedsCommit behavior is verified anyway to avoid warnings.
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setBounds(testBounds));
    testLayer = Layer::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setIsDrawable(true));
    EXPECT_FALSE(testLayer->needsDisplay());

    // The real test begins here.

    // Case 1: needsDisplay flag should not change because of an empty dirty rect.
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setNeedsDisplayRect(emptyDirtyRect));
    EXPECT_FALSE(testLayer->needsDisplay());

    // Case 2: basic.
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setNeedsDisplayRect(dirty1));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 3: a second dirty rect.
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setNeedsDisplayRect(dirty2));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 4: Layer should accept dirty rects that go beyond its bounds.
    testLayer = Layer::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setIsDrawable(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setBounds(testBounds));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setNeedsDisplayRect(outOfBoundsDirtyRect));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 5: setNeedsDisplay() without the dirty rect arg.
    testLayer = Layer::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setIsDrawable(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setBounds(testBounds));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setNeedsDisplay());
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 6: setNeedsDisplay() with a non-drawable layer
    testLayer = Layer::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXPECT_SET_NEEDS_COMMIT(0, testLayer->setBounds(testBounds));
    EXPECT_SET_NEEDS_COMMIT(0, testLayer->setNeedsDisplayRect(dirty1));
    EXPECT_TRUE(testLayer->needsDisplay());
}

TEST_F(LayerTest, checkPropertyChangeCausesCorrectBehavior)
{
    scoped_refptr<Layer> testLayer = Layer::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setIsDrawable(true));

    scoped_refptr<Layer> dummyLayer = Layer::create(); // just a dummy layer for this test case.

    // sanity check of initial test condition
    EXPECT_FALSE(testLayer->needsDisplay());

    // Next, test properties that should call setNeedsCommit (but not setNeedsDisplay)
    // All properties need to be set to new values in order for setNeedsCommit to be called.
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setAnchorPoint(gfx::PointF(1.23f, 4.56f)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setAnchorPointZ(0.7f));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setBackgroundColor(SK_ColorLTGRAY));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setMasksToBounds(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setOpacity(0.5));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setContentsOpaque(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setPosition(gfx::PointF(4, 9)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setSublayerTransform(MathUtil::createGfxTransform(0, 0, 0, 0, 0, 0)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setScrollable(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setShouldScrollOnMainThread(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setNonFastScrollableRegion(gfx::Rect(1, 1, 2, 2)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setHaveWheelEventHandlers(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setTransform(MathUtil::createGfxTransform(0, 0, 0, 0, 0, 0)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setDoubleSided(false));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setDebugName("Test Layer"));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setDrawCheckerboardForMissingTiles(!testLayer->drawCheckerboardForMissingTiles()));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setForceRenderSurface(true));

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, testLayer->setMaskLayer(dummyLayer.get()));
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, testLayer->setReplicaLayer(dummyLayer.get()));
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, testLayer->setScrollOffset(gfx::Vector2d(10, 10)));

    // The above tests should not have caused a change to the needsDisplay flag.
    EXPECT_FALSE(testLayer->needsDisplay());

    // Test properties that should call setNeedsDisplay and setNeedsCommit
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->setBounds(gfx::Size(5, 10)));
    EXPECT_TRUE(testLayer->needsDisplay());
}

TEST_F(LayerTest, verifyPushPropertiesAccumulatesUpdateRect)
{
    scoped_refptr<Layer> testLayer = Layer::create();
    scoped_ptr<LayerImpl> implLayer = LayerImpl::create(m_hostImpl.activeTree(), 1);

    testLayer->setNeedsDisplayRect(gfx::RectF(gfx::PointF(), gfx::SizeF(5, 5)));
    testLayer->pushPropertiesTo(implLayer.get());
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(), gfx::SizeF(5, 5)), implLayer->updateRect());

    // The LayerImpl's updateRect should be accumulated here, since we did not do anything to clear it.
    testLayer->setNeedsDisplayRect(gfx::RectF(gfx::PointF(10, 10), gfx::SizeF(5, 5)));
    testLayer->pushPropertiesTo(implLayer.get());
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(), gfx::SizeF(15, 15)), implLayer->updateRect());

    // If we do clear the LayerImpl side, then the next updateRect should be fresh without accumulation.
    implLayer->resetAllChangeTrackingForSubtree();
    testLayer->setNeedsDisplayRect(gfx::RectF(gfx::PointF(10, 10), gfx::SizeF(5, 5)));
    testLayer->pushPropertiesTo(implLayer.get());
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(10, 10), gfx::SizeF(5, 5)), implLayer->updateRect());
}

TEST_F(LayerTest, verifyPushPropertiesCausesSurfacePropertyChangedForTransform)
{
    scoped_refptr<Layer> testLayer = Layer::create();
    scoped_ptr<LayerImpl> implLayer = LayerImpl::create(m_hostImpl.activeTree(), 1);

    gfx::Transform transform;
    transform.Rotate(45.0);
    testLayer->setTransform(transform);

    EXPECT_FALSE(implLayer->layerSurfacePropertyChanged());

    testLayer->pushPropertiesTo(implLayer.get());

    EXPECT_TRUE(implLayer->layerSurfacePropertyChanged());
}

TEST_F(LayerTest, verifyPushPropertiesCausesSurfacePropertyChangedForOpacity)
{
    scoped_refptr<Layer> testLayer = Layer::create();
    scoped_ptr<LayerImpl> implLayer = LayerImpl::create(m_hostImpl.activeTree(), 1);

    testLayer->setOpacity(0.5);

    EXPECT_FALSE(implLayer->layerSurfacePropertyChanged());

    testLayer->pushPropertiesTo(implLayer.get());

    EXPECT_TRUE(implLayer->layerSurfacePropertyChanged());
}

class FakeLayerImplTreeHost : public LayerTreeHost {
public:
    static scoped_ptr<FakeLayerImplTreeHost> create()
    {
        scoped_ptr<FakeLayerImplTreeHost> host(new FakeLayerImplTreeHost(LayerTreeSettings()));
        // The initialize call will fail, since our client doesn't provide a valid GraphicsContext3D, but it doesn't matter in the tests that use this fake so ignore the return value.
        host->initialize(scoped_ptr<Thread>(NULL));
        return host.Pass();
    }

private:
    FakeLayerImplTreeHost(const LayerTreeSettings& settings)
        : LayerTreeHost(&m_client, settings)
    {
    }

    FakeLayerImplTreeHostClient m_client;
};

void assertLayerTreeHostMatchesForSubtree(Layer* layer, LayerTreeHost* host)
{
    EXPECT_EQ(host, layer->layerTreeHost());

    for (size_t i = 0; i < layer->children().size(); ++i)
        assertLayerTreeHostMatchesForSubtree(layer->children()[i].get(), host);

    if (layer->maskLayer())
        assertLayerTreeHostMatchesForSubtree(layer->maskLayer(), host);

    if (layer->replicaLayer())
        assertLayerTreeHostMatchesForSubtree(layer->replicaLayer(), host);
}


TEST(LayerLayerTreeHostTest, enteringTree)
{
    scoped_refptr<Layer> parent = Layer::create();
    scoped_refptr<Layer> child = Layer::create();
    scoped_refptr<Layer> mask = Layer::create();
    scoped_refptr<Layer> replica = Layer::create();
    scoped_refptr<Layer> replicaMask = Layer::create();

    // Set up a detached tree of layers. The host pointer should be nil for these layers.
    parent->addChild(child);
    child->setMaskLayer(mask.get());
    child->setReplicaLayer(replica.get());
    replica->setMaskLayer(mask.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), 0);

    scoped_ptr<FakeLayerImplTreeHost> layerTreeHost(FakeLayerImplTreeHost::create());
    // Setting the root layer should set the host pointer for all layers in the tree.
    layerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    // Clearing the root layer should also clear out the host pointers for all layers in the tree.
    layerTreeHost->setRootLayer(0);

    assertLayerTreeHostMatchesForSubtree(parent.get(), 0);
}

TEST(LayerLayerTreeHostTest, addingLayerSubtree)
{
    scoped_refptr<Layer> parent = Layer::create();
    scoped_ptr<FakeLayerImplTreeHost> layerTreeHost(FakeLayerImplTreeHost::create());

    layerTreeHost->setRootLayer(parent.get());

    EXPECT_EQ(parent->layerTreeHost(), layerTreeHost.get());

    // Adding a subtree to a layer already associated with a host should set the host pointer on all layers in that subtree.
    scoped_refptr<Layer> child = Layer::create();
    scoped_refptr<Layer> grandChild = Layer::create();
    child->addChild(grandChild);

    // Masks, replicas, and replica masks should pick up the new host too.
    scoped_refptr<Layer> childMask = Layer::create();
    child->setMaskLayer(childMask.get());
    scoped_refptr<Layer> childReplica = Layer::create();
    child->setReplicaLayer(childReplica.get());
    scoped_refptr<Layer> childReplicaMask = Layer::create();
    childReplica->setMaskLayer(childReplicaMask.get());

    parent->addChild(child);
    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    layerTreeHost->setRootLayer(0);
}

TEST(LayerLayerTreeHostTest, changeHost)
{
    scoped_refptr<Layer> parent = Layer::create();
    scoped_refptr<Layer> child = Layer::create();
    scoped_refptr<Layer> mask = Layer::create();
    scoped_refptr<Layer> replica = Layer::create();
    scoped_refptr<Layer> replicaMask = Layer::create();

    // Same setup as the previous test.
    parent->addChild(child);
    child->setMaskLayer(mask.get());
    child->setReplicaLayer(replica.get());
    replica->setMaskLayer(mask.get());

    scoped_ptr<FakeLayerImplTreeHost> firstLayerTreeHost(FakeLayerImplTreeHost::create());
    firstLayerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), firstLayerTreeHost.get());

    // Now re-root the tree to a new host (simulating what we do on a context lost event).
    // This should update the host pointers for all layers in the tree.
    scoped_ptr<FakeLayerImplTreeHost> secondLayerTreeHost(FakeLayerImplTreeHost::create());
    secondLayerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), secondLayerTreeHost.get());

    secondLayerTreeHost->setRootLayer(0);
}

TEST(LayerLayerTreeHostTest, changeHostInSubtree)
{
    scoped_refptr<Layer> firstParent = Layer::create();
    scoped_refptr<Layer> firstChild = Layer::create();
    scoped_refptr<Layer> secondParent = Layer::create();
    scoped_refptr<Layer> secondChild = Layer::create();
    scoped_refptr<Layer> secondGrandChild = Layer::create();

    // First put all children under the first parent and set the first host.
    firstParent->addChild(firstChild);
    secondChild->addChild(secondGrandChild);
    firstParent->addChild(secondChild);

    scoped_ptr<FakeLayerImplTreeHost> firstLayerTreeHost(FakeLayerImplTreeHost::create());
    firstLayerTreeHost->setRootLayer(firstParent.get());

    assertLayerTreeHostMatchesForSubtree(firstParent.get(), firstLayerTreeHost.get());

    // Now reparent the subtree starting at secondChild to a layer in a different tree.
    scoped_ptr<FakeLayerImplTreeHost> secondLayerTreeHost(FakeLayerImplTreeHost::create());
    secondLayerTreeHost->setRootLayer(secondParent.get());

    secondParent->addChild(secondChild);

    // The moved layer and its children should point to the new host.
    EXPECT_EQ(secondLayerTreeHost.get(), secondChild->layerTreeHost());
    EXPECT_EQ(secondLayerTreeHost.get(), secondGrandChild->layerTreeHost());

    // Test over, cleanup time.
    firstLayerTreeHost->setRootLayer(0);
    secondLayerTreeHost->setRootLayer(0);
}

TEST(LayerLayerTreeHostTest, replaceMaskAndReplicaLayer)
{
    scoped_refptr<Layer> parent = Layer::create();
    scoped_refptr<Layer> mask = Layer::create();
    scoped_refptr<Layer> replica = Layer::create();
    scoped_refptr<Layer> maskChild = Layer::create();
    scoped_refptr<Layer> replicaChild = Layer::create();
    scoped_refptr<Layer> maskReplacement = Layer::create();
    scoped_refptr<Layer> replicaReplacement = Layer::create();

    parent->setMaskLayer(mask.get());
    parent->setReplicaLayer(replica.get());
    mask->addChild(maskChild);
    replica->addChild(replicaChild);

    scoped_ptr<FakeLayerImplTreeHost> layerTreeHost(FakeLayerImplTreeHost::create());
    layerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    // Replacing the mask should clear out the old mask's subtree's host pointers.
    parent->setMaskLayer(maskReplacement.get());
    EXPECT_EQ(0, mask->layerTreeHost());
    EXPECT_EQ(0, maskChild->layerTreeHost());

    // Same for replacing a replica layer.
    parent->setReplicaLayer(replicaReplacement.get());
    EXPECT_EQ(0, replica->layerTreeHost());
    EXPECT_EQ(0, replicaChild->layerTreeHost());

    // Test over, cleanup time.
    layerTreeHost->setRootLayer(0);
}

TEST(LayerLayerTreeHostTest, destroyHostWithNonNullRootLayer)
{
    scoped_refptr<Layer> root = Layer::create();
    scoped_refptr<Layer> child = Layer::create();
    root->addChild(child);
    scoped_ptr<FakeLayerImplTreeHost> layerTreeHost(FakeLayerImplTreeHost::create());
    layerTreeHost->setRootLayer(root);
}

static bool addTestAnimation(Layer* layer)
{
    scoped_ptr<KeyframedFloatAnimationCurve> curve(KeyframedFloatAnimationCurve::create());
    curve->addKeyframe(FloatKeyframe::create(0, 0.3f, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(FloatKeyframe::create(1, 0.7f, scoped_ptr<TimingFunction>()));
    scoped_ptr<Animation> animation(Animation::create(curve.PassAs<AnimationCurve>(), 0, 0, Animation::Opacity));

    return layer->addAnimation(animation.Pass());
}

TEST(LayerLayerTreeHostTest, shouldNotAddAnimationWithoutLayerTreeHost)
{
    // Currently, WebCore assumes that animations will be started immediately / very soon
    // if a composited layer's addAnimation() returns true. However, without a layerTreeHost,
    // layers cannot actually animate yet. So, to prevent violating this WebCore assumption,
    // the animation should not be accepted if the layer doesn't already have a layerTreeHost.

    scoped_refptr<Layer> layer = Layer::create();

    // Case 1: without a layerTreeHost, the animation should not be accepted.
#if defined(OS_ANDROID)
    // All animations are enabled on Android to avoid performance regressions.
    // Other platforms will be enabled with http://crbug.com/129683
    EXPECT_TRUE(addTestAnimation(layer.get()));
#else
    EXPECT_FALSE(addTestAnimation(layer.get()));
#endif

    scoped_ptr<FakeLayerImplTreeHost> layerTreeHost(FakeLayerImplTreeHost::create());
    layerTreeHost->setRootLayer(layer.get());
    layer->setLayerTreeHost(layerTreeHost.get());
    assertLayerTreeHostMatchesForSubtree(layer.get(), layerTreeHost.get());

    // Case 2: with a layerTreeHost, the animation should be accepted.
    EXPECT_TRUE(addTestAnimation(layer.get()));
}

class MockLayer : public Layer {
public:
    bool needsDisplay() const { return m_needsDisplay; }

private:
    virtual ~MockLayer()
    {
    }
};

TEST(LayerTestWithoutFixture, setBoundsTriggersSetNeedsRedrawAfterGettingNonEmptyBounds)
{
    scoped_refptr<MockLayer> layer(new MockLayer);
    EXPECT_FALSE(layer->needsDisplay());
    layer->setBounds(gfx::Size(0, 10));
    EXPECT_FALSE(layer->needsDisplay());
    layer->setBounds(gfx::Size(10, 10));
    EXPECT_TRUE(layer->needsDisplay());
}

}  // namespace
}  // namespace cc
