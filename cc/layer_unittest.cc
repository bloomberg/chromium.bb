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
#include "cc/test/animation_test_common.h"
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
        EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times((expect)); \
        codeToTest;                                                      \
        Mock::VerifyAndClearExpectations(layer_tree_host_.get());         \
    } while (0)

#define EXPECT_SET_NEEDS_FULL_TREE_SYNC(expect, codeToTest) do {               \
        EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times((expect)); \
        codeToTest;                                                            \
        Mock::VerifyAndClearExpectations(layer_tree_host_.get());               \
    } while (0)


namespace cc {
namespace {

class MockLayerImplTreeHost : public LayerTreeHost {
public:
    MockLayerImplTreeHost()
        : LayerTreeHost(&m_fakeClient, LayerTreeSettings())
    {
        Initialize(scoped_ptr<Thread>(NULL));
    }

    MOCK_METHOD0(SetNeedsCommit, void());
    MOCK_METHOD0(SetNeedsFullTreeSync, void());

private:
    FakeLayerImplTreeHostClient m_fakeClient;
};

class MockLayerPainter : public LayerPainter {
public:
    virtual void Paint(SkCanvas* canvas, gfx::Rect content_rect, gfx::RectF* opaque) OVERRIDE { }
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
        layer_tree_host_.reset(new StrictMock<MockLayerImplTreeHost>);
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(layer_tree_host_.get());
        EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AnyNumber());
        parent_ = NULL;
        m_child1 = NULL;
        m_child2 = NULL;
        m_child3 = NULL;
        m_grandChild1 = NULL;
        m_grandChild2 = NULL;
        m_grandChild3 = NULL;

        layer_tree_host_->SetRootLayer(NULL);
        layer_tree_host_.reset();
    }

    void verifyTestTreeInitialState() const
    {
        ASSERT_EQ(3U, parent_->children().size());
        EXPECT_EQ(m_child1, parent_->children()[0]);
        EXPECT_EQ(m_child2, parent_->children()[1]);
        EXPECT_EQ(m_child3, parent_->children()[2]);
        EXPECT_EQ(parent_.get(), m_child1->parent());
        EXPECT_EQ(parent_.get(), m_child2->parent());
        EXPECT_EQ(parent_.get(), m_child3->parent());

        ASSERT_EQ(2U, m_child1->children().size());
        EXPECT_EQ(m_grandChild1, m_child1->children()[0]);
        EXPECT_EQ(m_grandChild2, m_child1->children()[1]);
        EXPECT_EQ(m_child1.get(), m_grandChild1->parent());
        EXPECT_EQ(m_child1.get(), m_grandChild2->parent());

        ASSERT_EQ(1U, m_child2->children().size());
        EXPECT_EQ(m_grandChild3, m_child2->children()[0]);
        EXPECT_EQ(m_child2.get(), m_grandChild3->parent());

        ASSERT_EQ(0U, m_child3->children().size());
    }

    void createSimpleTestTree()
    {
        parent_ = Layer::Create();
        m_child1 = Layer::Create();
        m_child2 = Layer::Create();
        m_child3 = Layer::Create();
        m_grandChild1 = Layer::Create();
        m_grandChild2 = Layer::Create();
        m_grandChild3 = Layer::Create();

        EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AnyNumber());
        layer_tree_host_->SetRootLayer(parent_);

        parent_->AddChild(m_child1);
        parent_->AddChild(m_child2);
        parent_->AddChild(m_child3);
        m_child1->AddChild(m_grandChild1);
        m_child1->AddChild(m_grandChild2);
        m_child2->AddChild(m_grandChild3);

        Mock::VerifyAndClearExpectations(layer_tree_host_.get());

        verifyTestTreeInitialState();
    }

    FakeImplProxy m_proxy;
    FakeLayerTreeHostImpl m_hostImpl;

    scoped_ptr<StrictMock<MockLayerImplTreeHost> > layer_tree_host_;
    scoped_refptr<Layer> parent_;
    scoped_refptr<Layer> m_child1;
    scoped_refptr<Layer> m_child2;
    scoped_refptr<Layer> m_child3;
    scoped_refptr<Layer> m_grandChild1;
    scoped_refptr<Layer> m_grandChild2;
    scoped_refptr<Layer> m_grandChild3;
};

TEST_F(LayerTest, basicCreateAndDestroy)
{
    scoped_refptr<Layer> testLayer = Layer::Create();
    ASSERT_TRUE(testLayer);

    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
    testLayer->SetLayerTreeHost(layer_tree_host_.get());
}

TEST_F(LayerTest, addAndRemoveChild)
{
    scoped_refptr<Layer> parent = Layer::Create();
    scoped_refptr<Layer> child = Layer::Create();

    // Upon creation, layers should not have children or parent.
    ASSERT_EQ(0U, parent->children().size());
    EXPECT_FALSE(child->parent());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, layer_tree_host_->SetRootLayer(parent));
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->AddChild(child));

    ASSERT_EQ(1U, parent->children().size());
    EXPECT_EQ(child.get(), parent->children()[0]);
    EXPECT_EQ(parent.get(), child->parent());
    EXPECT_EQ(parent.get(), child->RootLayer());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), child->RemoveFromParent());
}

TEST_F(LayerTest, addSameChildTwice)
{
    EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AtLeast(1));

    scoped_refptr<Layer> parent = Layer::Create();
    scoped_refptr<Layer> child = Layer::Create();

    layer_tree_host_->SetRootLayer(parent);

    ASSERT_EQ(0u, parent->children().size());

    parent->AddChild(child);
    ASSERT_EQ(1u, parent->children().size());
    EXPECT_EQ(parent.get(), child->parent());

    parent->AddChild(child);
    ASSERT_EQ(1u, parent->children().size());
    EXPECT_EQ(parent.get(), child->parent());
}

TEST_F(LayerTest, insertChild)
{
    scoped_refptr<Layer> parent = Layer::Create();
    scoped_refptr<Layer> child1 = Layer::Create();
    scoped_refptr<Layer> child2 = Layer::Create();
    scoped_refptr<Layer> child3 = Layer::Create();
    scoped_refptr<Layer> child4 = Layer::Create();

    parent->SetLayerTreeHost(layer_tree_host_.get());

    ASSERT_EQ(0U, parent->children().size());

    // Case 1: inserting to empty list.
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child3, 0));
    ASSERT_EQ(1U, parent->children().size());
    EXPECT_EQ(child3, parent->children()[0]);
    EXPECT_EQ(parent.get(), child3->parent());

    // Case 2: inserting to beginning of list
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child1, 0));
    ASSERT_EQ(2U, parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child3, parent->children()[1]);
    EXPECT_EQ(parent.get(), child1->parent());

    // Case 3: inserting to middle of list
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child2, 1));
    ASSERT_EQ(3U, parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
    EXPECT_EQ(child3, parent->children()[2]);
    EXPECT_EQ(parent.get(), child2->parent());

    // Case 4: inserting to end of list
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child4, 3));

    ASSERT_EQ(4U, parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
    EXPECT_EQ(child3, parent->children()[2]);
    EXPECT_EQ(child4, parent->children()[3]);
    EXPECT_EQ(parent.get(), child4->parent());

    EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AtLeast(1));
}

TEST_F(LayerTest, insertChildPastEndOfList)
{
    scoped_refptr<Layer> parent = Layer::Create();
    scoped_refptr<Layer> child1 = Layer::Create();
    scoped_refptr<Layer> child2 = Layer::Create();

    ASSERT_EQ(0U, parent->children().size());

    // insert to an out-of-bounds index
    parent->InsertChild(child1, 53);

    ASSERT_EQ(1U, parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);

    // insert another child to out-of-bounds, when list is not already empty.
    parent->InsertChild(child2, 2459);

    ASSERT_EQ(2U, parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
}

TEST_F(LayerTest, insertSameChildTwice)
{
    scoped_refptr<Layer> parent = Layer::Create();
    scoped_refptr<Layer> child1 = Layer::Create();
    scoped_refptr<Layer> child2 = Layer::Create();

    parent->SetLayerTreeHost(layer_tree_host_.get());

    ASSERT_EQ(0U, parent->children().size());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child1, 0));
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, parent->InsertChild(child2, 1));

    ASSERT_EQ(2U, parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);

    // Inserting the same child again should cause the child to be removed and re-inserted at the new location.
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), parent->InsertChild(child1, 1));

    // child1 should now be at the end of the list.
    ASSERT_EQ(2U, parent->children().size());
    EXPECT_EQ(child2, parent->children()[0]);
    EXPECT_EQ(child1, parent->children()[1]);

    EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AtLeast(1));
}

TEST_F(LayerTest, replaceChildWithNewChild)
{
    createSimpleTestTree();
    scoped_refptr<Layer> child4 = Layer::Create();

    EXPECT_FALSE(child4->parent());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), parent_->ReplaceChild(m_child2.get(), child4));
    EXPECT_FALSE(parent_->NeedsDisplayForTesting());
    EXPECT_FALSE(m_child1->NeedsDisplayForTesting());
    EXPECT_FALSE(m_child2->NeedsDisplayForTesting());
    EXPECT_FALSE(m_child3->NeedsDisplayForTesting());
    EXPECT_FALSE(child4->NeedsDisplayForTesting());

    ASSERT_EQ(static_cast<size_t>(3), parent_->children().size());
    EXPECT_EQ(m_child1, parent_->children()[0]);
    EXPECT_EQ(child4, parent_->children()[1]);
    EXPECT_EQ(m_child3, parent_->children()[2]);
    EXPECT_EQ(parent_.get(), child4->parent());

    EXPECT_FALSE(m_child2->parent());
}

TEST_F(LayerTest, replaceChildWithNewChildAutomaticRasterScale)
{
    createSimpleTestTree();
    scoped_refptr<Layer> child4 = Layer::Create();
    EXPECT_SET_NEEDS_COMMIT(1, m_child1->SetAutomaticallyComputeRasterScale(true));
    EXPECT_SET_NEEDS_COMMIT(1, m_child2->SetAutomaticallyComputeRasterScale(true));
    EXPECT_SET_NEEDS_COMMIT(1, m_child3->SetAutomaticallyComputeRasterScale(true));

    EXPECT_FALSE(child4->parent());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), parent_->ReplaceChild(m_child2.get(), child4));
    EXPECT_FALSE(parent_->NeedsDisplayForTesting());
    EXPECT_FALSE(m_child1->NeedsDisplayForTesting());
    EXPECT_FALSE(m_child2->NeedsDisplayForTesting());
    EXPECT_FALSE(m_child3->NeedsDisplayForTesting());
    EXPECT_FALSE(child4->NeedsDisplayForTesting());

    ASSERT_EQ(3U, parent_->children().size());
    EXPECT_EQ(m_child1, parent_->children()[0]);
    EXPECT_EQ(child4, parent_->children()[1]);
    EXPECT_EQ(m_child3, parent_->children()[2]);
    EXPECT_EQ(parent_.get(), child4->parent());

    EXPECT_FALSE(m_child2->parent());
}

TEST_F(LayerTest, replaceChildWithNewChildThatHasOtherParent)
{
    createSimpleTestTree();

    // create another simple tree with testLayer and child4.
    scoped_refptr<Layer> testLayer = Layer::Create();
    scoped_refptr<Layer> child4 = Layer::Create();
    testLayer->AddChild(child4);
    ASSERT_EQ(1U, testLayer->children().size());
    EXPECT_EQ(child4, testLayer->children()[0]);
    EXPECT_EQ(testLayer.get(), child4->parent());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), parent_->ReplaceChild(m_child2.get(), child4));

    ASSERT_EQ(3U, parent_->children().size());
    EXPECT_EQ(m_child1, parent_->children()[0]);
    EXPECT_EQ(child4, parent_->children()[1]);
    EXPECT_EQ(m_child3, parent_->children()[2]);
    EXPECT_EQ(parent_.get(), child4->parent());

    // testLayer should no longer have child4,
    // and child2 should no longer have a parent.
    ASSERT_EQ(0U, testLayer->children().size());
    EXPECT_FALSE(m_child2->parent());
}

TEST_F(LayerTest, replaceChildWithSameChild)
{
    createSimpleTestTree();

    // SetNeedsFullTreeSync / SetNeedsCommit should not be called because its the same child
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
    EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(0);
    parent_->ReplaceChild(m_child2.get(), m_child2);

    verifyTestTreeInitialState();
}

TEST_F(LayerTest, removeAllChildren)
{
    createSimpleTestTree();

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(3), parent_->RemoveAllChildren());

    ASSERT_EQ(0U, parent_->children().size());
    EXPECT_FALSE(m_child1->parent());
    EXPECT_FALSE(m_child2->parent());
    EXPECT_FALSE(m_child3->parent());
}

TEST_F(LayerTest, setChildren)
{
    scoped_refptr<Layer> oldParent = Layer::Create();
    scoped_refptr<Layer> newParent = Layer::Create();

    scoped_refptr<Layer> child1 = Layer::Create();
    scoped_refptr<Layer> child2 = Layer::Create();

    std::vector<scoped_refptr<Layer> > newChildren;
    newChildren.push_back(child1);
    newChildren.push_back(child2);

    // Set up and verify initial test conditions: child1 has a parent, child2 has no parent.
    oldParent->AddChild(child1);
    ASSERT_EQ(0U, newParent->children().size());
    EXPECT_EQ(oldParent.get(), child1->parent());
    EXPECT_FALSE(child2->parent());

    newParent->SetLayerTreeHost(layer_tree_host_.get());

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(AtLeast(1), newParent->SetChildren(newChildren));

    ASSERT_EQ(2U, newParent->children().size());
    EXPECT_EQ(newParent.get(), child1->parent());
    EXPECT_EQ(newParent.get(), child2->parent());

    EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AtLeast(1));
}

TEST_F(LayerTest, getRootLayerAfterTreeManipulations)
{
    createSimpleTestTree();

    // For this test we don't care about SetNeedsFullTreeSync calls.
    EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times(AnyNumber());

    scoped_refptr<Layer> child4 = Layer::Create();

    EXPECT_EQ(parent_.get(), parent_->RootLayer());
    EXPECT_EQ(parent_.get(), m_child1->RootLayer());
    EXPECT_EQ(parent_.get(), m_child2->RootLayer());
    EXPECT_EQ(parent_.get(), m_child3->RootLayer());
    EXPECT_EQ(child4.get(),   child4->RootLayer());
    EXPECT_EQ(parent_.get(), m_grandChild1->RootLayer());
    EXPECT_EQ(parent_.get(), m_grandChild2->RootLayer());
    EXPECT_EQ(parent_.get(), m_grandChild3->RootLayer());

    m_child1->RemoveFromParent();

    // child1 and its children, grandChild1 and grandChild2 are now on a separate subtree.
    EXPECT_EQ(parent_.get(), parent_->RootLayer());
    EXPECT_EQ(m_child1.get(), m_child1->RootLayer());
    EXPECT_EQ(parent_.get(), m_child2->RootLayer());
    EXPECT_EQ(parent_.get(), m_child3->RootLayer());
    EXPECT_EQ(child4.get(), child4->RootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild1->RootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild2->RootLayer());
    EXPECT_EQ(parent_.get(), m_grandChild3->RootLayer());

    m_grandChild3->AddChild(child4);

    EXPECT_EQ(parent_.get(), parent_->RootLayer());
    EXPECT_EQ(m_child1.get(), m_child1->RootLayer());
    EXPECT_EQ(parent_.get(), m_child2->RootLayer());
    EXPECT_EQ(parent_.get(), m_child3->RootLayer());
    EXPECT_EQ(parent_.get(), child4->RootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild1->RootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild2->RootLayer());
    EXPECT_EQ(parent_.get(), m_grandChild3->RootLayer());

    m_child2->ReplaceChild(m_grandChild3.get(), m_child1);

    // grandChild3 gets orphaned and the child1 subtree gets planted back into the tree under child2.
    EXPECT_EQ(parent_.get(), parent_->RootLayer());
    EXPECT_EQ(parent_.get(), m_child1->RootLayer());
    EXPECT_EQ(parent_.get(), m_child2->RootLayer());
    EXPECT_EQ(parent_.get(), m_child3->RootLayer());
    EXPECT_EQ(m_grandChild3.get(), child4->RootLayer());
    EXPECT_EQ(parent_.get(), m_grandChild1->RootLayer());
    EXPECT_EQ(parent_.get(), m_grandChild2->RootLayer());
    EXPECT_EQ(m_grandChild3.get(), m_grandChild3->RootLayer());
}

TEST_F(LayerTest, checkSetNeedsDisplayCausesCorrectBehavior)
{
    // The semantics for setNeedsDisplay which are tested here:
    //   1. sets needsDisplay flag appropriately.
    //   2. indirectly calls SetNeedsCommit, exactly once for each call to setNeedsDisplay.

    scoped_refptr<Layer> testLayer = Layer::Create();
    testLayer->SetLayerTreeHost(layer_tree_host_.get());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetIsDrawable(true));

    gfx::Size testBounds = gfx::Size(501, 508);

    gfx::RectF dirty1 = gfx::RectF(10, 15, 1, 2);
    gfx::RectF dirty2 = gfx::RectF(20, 25, 3, 4);
    gfx::RectF emptyDirtyRect = gfx::RectF(40, 45, 0, 0);
    gfx::RectF outOfBoundsDirtyRect = gfx::RectF(400, 405, 500, 502);

    // Before anything, testLayer should not be dirty.
    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());

    // This is just initialization, but SetNeedsCommit behavior is verified anyway to avoid warnings.
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetBounds(testBounds));
    EXPECT_TRUE(testLayer->NeedsDisplayForTesting());

    // The real test begins here.
    testLayer->ResetNeedsDisplayForTesting();
    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());

    // Case 1: Layer should accept dirty rects that go beyond its bounds.
    testLayer->ResetNeedsDisplayForTesting();
    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetNeedsDisplayRect(outOfBoundsDirtyRect));
    EXPECT_TRUE(testLayer->NeedsDisplayForTesting());
    testLayer->ResetNeedsDisplayForTesting();

    // Case 2: SetNeedsDisplay() without the dirty rect arg.
    testLayer->ResetNeedsDisplayForTesting();
    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetNeedsDisplay());
    EXPECT_TRUE(testLayer->NeedsDisplayForTesting());
    testLayer->ResetNeedsDisplayForTesting();

    // Case 3: SetNeedsDisplay() with a non-drawable layer
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetIsDrawable(false));
    testLayer->ResetNeedsDisplayForTesting();
    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());
    EXPECT_SET_NEEDS_COMMIT(0, testLayer->SetNeedsDisplayRect(dirty1));
    EXPECT_TRUE(testLayer->NeedsDisplayForTesting());
}

TEST_F(LayerTest, checkPropertyChangeCausesCorrectBehavior)
{
    scoped_refptr<Layer> testLayer = Layer::Create();
    testLayer->SetLayerTreeHost(layer_tree_host_.get());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetIsDrawable(true));

    scoped_refptr<Layer> dummyLayer1 = Layer::Create(); // just a dummy layer for this test case.
    scoped_refptr<Layer> dummyLayer2 = Layer::Create(); // just a dummy layer for this test case.

    // sanity check of initial test condition
    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());

    // Next, test properties that should call SetNeedsCommit (but not setNeedsDisplay)
    // All properties need to be set to new values in order for SetNeedsCommit to be called.
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetAnchorPoint(gfx::PointF(1.23f, 4.56f)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetAnchorPointZ(0.7f));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetBackgroundColor(SK_ColorLTGRAY));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetMasksToBounds(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetOpacity(0.5));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetContentsOpaque(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetPosition(gfx::PointF(4, 9)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetSublayerTransform(gfx::Transform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetScrollable(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetScrollOffset(gfx::Vector2d(10, 10)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetShouldScrollOnMainThread(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetNonFastScrollableRegion(gfx::Rect(1, 1, 2, 2)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetHaveWheelEventHandlers(true));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetTransform(gfx::Transform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetDoubleSided(false));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetDebugName("Test Layer"));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetDrawCheckerboardForMissingTiles(!testLayer->DrawCheckerboardForMissingTiles()));
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetForceRenderSurface(true));

    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, testLayer->SetMaskLayer(dummyLayer1.get()));
    EXPECT_SET_NEEDS_FULL_TREE_SYNC(1, testLayer->SetReplicaLayer(dummyLayer2.get()));

    // The above tests should not have caused a change to the needsDisplay flag.
    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());

    // As layers are removed from the tree, they will cause a tree sync.
    EXPECT_CALL(*layer_tree_host_, SetNeedsFullTreeSync()).Times((AnyNumber()));
}

TEST_F(LayerTest, setBoundsTriggersSetNeedsRedrawAfterGettingNonEmptyBounds)
{
    scoped_refptr<Layer> testLayer = Layer::Create();
    testLayer->SetLayerTreeHost(layer_tree_host_.get());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetIsDrawable(true));

    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetBounds(gfx::Size(0, 10)));
    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetBounds(gfx::Size(10, 10)));
    EXPECT_TRUE(testLayer->NeedsDisplayForTesting());

    testLayer->ResetNeedsDisplayForTesting();
    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());

    // Calling setBounds only invalidates on the first time.
    EXPECT_SET_NEEDS_COMMIT(1, testLayer->SetBounds(gfx::Size(7, 10)));
    EXPECT_FALSE(testLayer->NeedsDisplayForTesting());
}

TEST_F(LayerTest, verifyPushPropertiesAccumulatesUpdateRect)
{
    scoped_refptr<Layer> testLayer = Layer::Create();
    scoped_ptr<LayerImpl> implLayer = LayerImpl::Create(m_hostImpl.active_tree(), 1);

    testLayer->SetNeedsDisplayRect(gfx::RectF(gfx::PointF(), gfx::SizeF(5, 5)));
    testLayer->PushPropertiesTo(implLayer.get());
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(), gfx::SizeF(5, 5)), implLayer->update_rect());

    // The LayerImpl's updateRect should be accumulated here, since we did not do anything to clear it.
    testLayer->SetNeedsDisplayRect(gfx::RectF(gfx::PointF(10, 10), gfx::SizeF(5, 5)));
    testLayer->PushPropertiesTo(implLayer.get());
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(), gfx::SizeF(15, 15)), implLayer->update_rect());

    // If we do clear the LayerImpl side, then the next updateRect should be fresh without accumulation.
    implLayer->ResetAllChangeTrackingForSubtree();
    testLayer->SetNeedsDisplayRect(gfx::RectF(gfx::PointF(10, 10), gfx::SizeF(5, 5)));
    testLayer->PushPropertiesTo(implLayer.get());
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(10, 10), gfx::SizeF(5, 5)), implLayer->update_rect());
}

TEST_F(LayerTest, verifyPushPropertiesCausesSurfacePropertyChangedForTransform)
{
    scoped_refptr<Layer> testLayer = Layer::Create();
    scoped_ptr<LayerImpl> implLayer = LayerImpl::Create(m_hostImpl.active_tree(), 1);

    gfx::Transform transform;
    transform.Rotate(45.0);
    testLayer->SetTransform(transform);

    EXPECT_FALSE(implLayer->LayerSurfacePropertyChanged());

    testLayer->PushPropertiesTo(implLayer.get());

    EXPECT_TRUE(implLayer->LayerSurfacePropertyChanged());
}

TEST_F(LayerTest, verifyPushPropertiesCausesSurfacePropertyChangedForOpacity)
{
    scoped_refptr<Layer> testLayer = Layer::Create();
    scoped_ptr<LayerImpl> implLayer = LayerImpl::Create(m_hostImpl.active_tree(), 1);

    testLayer->SetOpacity(0.5);

    EXPECT_FALSE(implLayer->LayerSurfacePropertyChanged());

    testLayer->PushPropertiesTo(implLayer.get());

    EXPECT_TRUE(implLayer->LayerSurfacePropertyChanged());
}

TEST_F(LayerTest, verifyPushPropertiesDoesNotCauseSurfacePropertyChangedDuringImplOnlyTransformAnimation)
{
    scoped_refptr<Layer> testLayer = Layer::Create();
    scoped_ptr<LayerImpl> implLayer = LayerImpl::Create(m_hostImpl.active_tree(), 1);

    scoped_ptr<AnimationRegistrar> registrar = AnimationRegistrar::create();
    implLayer->layer_animation_controller()->SetAnimationRegistrar(registrar.get());

    addAnimatedTransformToController(*implLayer->layer_animation_controller(), 1.0, 0, 100);

    gfx::Transform transform;
    transform.Rotate(45.0);
    testLayer->SetTransform(transform);

    EXPECT_FALSE(implLayer->LayerSurfacePropertyChanged());
    testLayer->PushPropertiesTo(implLayer.get());
    EXPECT_TRUE(implLayer->LayerSurfacePropertyChanged());

    implLayer->ResetAllChangeTrackingForSubtree();
    addAnimatedTransformToController(*implLayer->layer_animation_controller(), 1.0, 0, 100);
    implLayer->layer_animation_controller()->GetAnimation(Animation::Transform)->set_is_impl_only(true);
    transform.Rotate(45.0);
    testLayer->SetTransform(transform);

    EXPECT_FALSE(implLayer->LayerSurfacePropertyChanged());
    testLayer->PushPropertiesTo(implLayer.get());
    EXPECT_FALSE(implLayer->LayerSurfacePropertyChanged());
}

TEST_F(LayerTest, verifyPushPropertiesDoesNotCauseSurfacePropertyChangedDuringImplOnlyOpacityAnimation)
{
    scoped_refptr<Layer> testLayer = Layer::Create();
    scoped_ptr<LayerImpl> implLayer = LayerImpl::Create(m_hostImpl.active_tree(), 1);

    scoped_ptr<AnimationRegistrar> registrar = AnimationRegistrar::create();
    implLayer->layer_animation_controller()->SetAnimationRegistrar(registrar.get());

    addOpacityTransitionToController(*implLayer->layer_animation_controller(), 1.0, 0.3f, 0.7f, false);

    testLayer->SetOpacity(0.5f);

    EXPECT_FALSE(implLayer->LayerSurfacePropertyChanged());
    testLayer->PushPropertiesTo(implLayer.get());
    EXPECT_TRUE(implLayer->LayerSurfacePropertyChanged());

    implLayer->ResetAllChangeTrackingForSubtree();
    addOpacityTransitionToController(*implLayer->layer_animation_controller(), 1.0, 0.3f, 0.7f, false);
    implLayer->layer_animation_controller()->GetAnimation(Animation::Opacity)->set_is_impl_only(true);
    testLayer->SetOpacity(0.75f);

    EXPECT_FALSE(implLayer->LayerSurfacePropertyChanged());
    testLayer->PushPropertiesTo(implLayer.get());
    EXPECT_FALSE(implLayer->LayerSurfacePropertyChanged());
}


TEST_F(LayerTest, maskAndReplicaHasParent)
{
    scoped_refptr<Layer> parent = Layer::Create();
    scoped_refptr<Layer> child = Layer::Create();
    scoped_refptr<Layer> mask = Layer::Create();
    scoped_refptr<Layer> replica = Layer::Create();
    scoped_refptr<Layer> replicaMask = Layer::Create();
    scoped_refptr<Layer> maskReplacement = Layer::Create();
    scoped_refptr<Layer> replicaReplacement = Layer::Create();
    scoped_refptr<Layer> replicaMaskReplacement = Layer::Create();

    parent->AddChild(child);
    child->SetMaskLayer(mask.get());
    child->SetReplicaLayer(replica.get());
    replica->SetMaskLayer(replicaMask.get());

    EXPECT_EQ(parent, child->parent());
    EXPECT_EQ(child, mask->parent());
    EXPECT_EQ(child, replica->parent());
    EXPECT_EQ(replica, replicaMask->parent());

    replica->SetMaskLayer(replicaMaskReplacement.get());
    EXPECT_EQ(NULL, replicaMask->parent());
    EXPECT_EQ(replica, replicaMaskReplacement->parent());

    child->SetMaskLayer(maskReplacement.get());
    EXPECT_EQ(NULL, mask->parent());
    EXPECT_EQ(child, maskReplacement->parent());

    child->SetReplicaLayer(replicaReplacement.get());
    EXPECT_EQ(NULL, replica->parent());
    EXPECT_EQ(child, replicaReplacement->parent());

    EXPECT_EQ(replica, replica->mask_layer()->parent());
}

class FakeLayerImplTreeHost : public LayerTreeHost {
public:
    static scoped_ptr<FakeLayerImplTreeHost> Create()
    {
        scoped_ptr<FakeLayerImplTreeHost> host(new FakeLayerImplTreeHost(LayerTreeSettings()));
        // The initialize call will fail, since our client doesn't provide a valid GraphicsContext3D, but it doesn't matter in the tests that use this fake so ignore the return value.
        host->Initialize(scoped_ptr<Thread>(NULL));
        return host.Pass();
    }

    static scoped_ptr<FakeLayerImplTreeHost> Create(LayerTreeSettings settings)
    {
        scoped_ptr<FakeLayerImplTreeHost> host(new FakeLayerImplTreeHost(settings));
        host->Initialize(scoped_ptr<Thread>(NULL));
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
    EXPECT_EQ(host, layer->layer_tree_host());

    for (size_t i = 0; i < layer->children().size(); ++i)
        assertLayerTreeHostMatchesForSubtree(layer->children()[i].get(), host);

    if (layer->mask_layer())
        assertLayerTreeHostMatchesForSubtree(layer->mask_layer(), host);

    if (layer->replica_layer())
        assertLayerTreeHostMatchesForSubtree(layer->replica_layer(), host);
}

TEST(LayerLayerTreeHostTest, enteringTree)
{
    scoped_refptr<Layer> parent = Layer::Create();
    scoped_refptr<Layer> child = Layer::Create();
    scoped_refptr<Layer> mask = Layer::Create();
    scoped_refptr<Layer> replica = Layer::Create();
    scoped_refptr<Layer> replicaMask = Layer::Create();

    // Set up a detached tree of layers. The host pointer should be nil for these layers.
    parent->AddChild(child);
    child->SetMaskLayer(mask.get());
    child->SetReplicaLayer(replica.get());
    replica->SetMaskLayer(replicaMask.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), 0);

    scoped_ptr<FakeLayerImplTreeHost> layerTreeHost(FakeLayerImplTreeHost::Create());
    // Setting the root layer should set the host pointer for all layers in the tree.
    layerTreeHost->SetRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    // Clearing the root layer should also clear out the host pointers for all layers in the tree.
    layerTreeHost->SetRootLayer(NULL);

    assertLayerTreeHostMatchesForSubtree(parent.get(), 0);
}

TEST(LayerLayerTreeHostTest, addingLayerSubtree)
{
    scoped_refptr<Layer> parent = Layer::Create();
    scoped_ptr<FakeLayerImplTreeHost> layerTreeHost(FakeLayerImplTreeHost::Create());

    layerTreeHost->SetRootLayer(parent.get());

    EXPECT_EQ(parent->layer_tree_host(), layerTreeHost.get());

    // Adding a subtree to a layer already associated with a host should set the host pointer on all layers in that subtree.
    scoped_refptr<Layer> child = Layer::Create();
    scoped_refptr<Layer> grandChild = Layer::Create();
    child->AddChild(grandChild);

    // Masks, replicas, and replica masks should pick up the new host too.
    scoped_refptr<Layer> childMask = Layer::Create();
    child->SetMaskLayer(childMask.get());
    scoped_refptr<Layer> childReplica = Layer::Create();
    child->SetReplicaLayer(childReplica.get());
    scoped_refptr<Layer> childReplicaMask = Layer::Create();
    childReplica->SetMaskLayer(childReplicaMask.get());

    parent->AddChild(child);
    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    layerTreeHost->SetRootLayer(NULL);
}

TEST(LayerLayerTreeHostTest, changeHost)
{
    scoped_refptr<Layer> parent = Layer::Create();
    scoped_refptr<Layer> child = Layer::Create();
    scoped_refptr<Layer> mask = Layer::Create();
    scoped_refptr<Layer> replica = Layer::Create();
    scoped_refptr<Layer> replicaMask = Layer::Create();

    // Same setup as the previous test.
    parent->AddChild(child);
    child->SetMaskLayer(mask.get());
    child->SetReplicaLayer(replica.get());
    replica->SetMaskLayer(replicaMask.get());

    scoped_ptr<FakeLayerImplTreeHost> firstLayerTreeHost(FakeLayerImplTreeHost::Create());
    firstLayerTreeHost->SetRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), firstLayerTreeHost.get());

    // Now re-root the tree to a new host (simulating what we do on a context lost event).
    // This should update the host pointers for all layers in the tree.
    scoped_ptr<FakeLayerImplTreeHost> secondLayerTreeHost(FakeLayerImplTreeHost::Create());
    secondLayerTreeHost->SetRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), secondLayerTreeHost.get());

    secondLayerTreeHost->SetRootLayer(NULL);
}

TEST(LayerLayerTreeHostTest, changeHostInSubtree)
{
    scoped_refptr<Layer> firstParent = Layer::Create();
    scoped_refptr<Layer> firstChild = Layer::Create();
    scoped_refptr<Layer> secondParent = Layer::Create();
    scoped_refptr<Layer> secondChild = Layer::Create();
    scoped_refptr<Layer> secondGrandChild = Layer::Create();

    // First put all children under the first parent and set the first host.
    firstParent->AddChild(firstChild);
    secondChild->AddChild(secondGrandChild);
    firstParent->AddChild(secondChild);

    scoped_ptr<FakeLayerImplTreeHost> firstLayerTreeHost(FakeLayerImplTreeHost::Create());
    firstLayerTreeHost->SetRootLayer(firstParent.get());

    assertLayerTreeHostMatchesForSubtree(firstParent.get(), firstLayerTreeHost.get());

    // Now reparent the subtree starting at secondChild to a layer in a different tree.
    scoped_ptr<FakeLayerImplTreeHost> secondLayerTreeHost(FakeLayerImplTreeHost::Create());
    secondLayerTreeHost->SetRootLayer(secondParent.get());

    secondParent->AddChild(secondChild);

    // The moved layer and its children should point to the new host.
    EXPECT_EQ(secondLayerTreeHost.get(), secondChild->layer_tree_host());
    EXPECT_EQ(secondLayerTreeHost.get(), secondGrandChild->layer_tree_host());

    // Test over, cleanup time.
    firstLayerTreeHost->SetRootLayer(NULL);
    secondLayerTreeHost->SetRootLayer(NULL);
}

TEST(LayerLayerTreeHostTest, replaceMaskAndReplicaLayer)
{
    scoped_refptr<Layer> parent = Layer::Create();
    scoped_refptr<Layer> mask = Layer::Create();
    scoped_refptr<Layer> replica = Layer::Create();
    scoped_refptr<Layer> maskChild = Layer::Create();
    scoped_refptr<Layer> replicaChild = Layer::Create();
    scoped_refptr<Layer> maskReplacement = Layer::Create();
    scoped_refptr<Layer> replicaReplacement = Layer::Create();

    parent->SetMaskLayer(mask.get());
    parent->SetReplicaLayer(replica.get());
    mask->AddChild(maskChild);
    replica->AddChild(replicaChild);

    scoped_ptr<FakeLayerImplTreeHost> layerTreeHost(FakeLayerImplTreeHost::Create());
    layerTreeHost->SetRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    // Replacing the mask should clear out the old mask's subtree's host pointers.
    parent->SetMaskLayer(maskReplacement.get());
    EXPECT_EQ(0, mask->layer_tree_host());
    EXPECT_EQ(0, maskChild->layer_tree_host());

    // Same for replacing a replica layer.
    parent->SetReplicaLayer(replicaReplacement.get());
    EXPECT_EQ(0, replica->layer_tree_host());
    EXPECT_EQ(0, replicaChild->layer_tree_host());

    // Test over, cleanup time.
    layerTreeHost->SetRootLayer(NULL);
}

TEST(LayerLayerTreeHostTest, destroyHostWithNonNullRootLayer)
{
    scoped_refptr<Layer> root = Layer::Create();
    scoped_refptr<Layer> child = Layer::Create();
    root->AddChild(child);
    scoped_ptr<FakeLayerImplTreeHost> layerTreeHost(FakeLayerImplTreeHost::Create());
    layerTreeHost->SetRootLayer(root);
}

static bool addTestAnimation(Layer* layer)
{
    scoped_ptr<KeyframedFloatAnimationCurve> curve(KeyframedFloatAnimationCurve::Create());
    curve->AddKeyframe(FloatKeyframe::Create(0.0, 0.3f, scoped_ptr<TimingFunction>()));
    curve->AddKeyframe(FloatKeyframe::Create(1.0, 0.7f, scoped_ptr<TimingFunction>()));
    scoped_ptr<Animation> animation(Animation::Create(curve.PassAs<AnimationCurve>(), 0, 0, Animation::Opacity));

    return layer->AddAnimation(animation.Pass());
}

TEST(LayerLayerTreeHostTest, shouldNotAddAnimationWithoutAnimationRegistrar)
{
    scoped_refptr<Layer> layer = Layer::Create();

    // Case 1: without a LayerTreeHost and without an AnimationRegistrar, the
    // animation should not be accepted.
    EXPECT_FALSE(addTestAnimation(layer.get()));

    scoped_ptr<AnimationRegistrar> registrar = AnimationRegistrar::create();
    layer->layer_animation_controller()->SetAnimationRegistrar(registrar.get());

    // Case 2: with an AnimationRegistrar, the animation should be accepted.
    EXPECT_TRUE(addTestAnimation(layer.get()));

    LayerTreeSettings settings;
    settings.acceleratedAnimationEnabled = false;
    scoped_ptr<FakeLayerImplTreeHost> layerTreeHost(FakeLayerImplTreeHost::Create(settings));
    layerTreeHost->SetRootLayer(layer);
    layer->SetLayerTreeHost(layerTreeHost.get());
    assertLayerTreeHostMatchesForSubtree(layer.get(), layerTreeHost.get());

    // Case 3: with a LayerTreeHost where accelerated animation is disabled, the
    // animation should be rejected.
    EXPECT_FALSE(addTestAnimation(layer.get()));
}

}  // namespace
}  // namespace cc
