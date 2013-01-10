// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_sorter.h"

#include "cc/layer_impl.h"
#include "cc/math_util.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

// Note: In the following overlap tests, the "camera" is looking down the negative Z axis,
// meaning that layers with smaller z values (more negative) are further from the camera
// and therefore must be drawn before layers with higher z values.

TEST(LayerSorterTest, BasicOverlap)
{
    LayerSorter::ABCompareResult overlapResult;
    const float zThreshold = 0.1f;
    float weight = 0;

    // Trivial test, with one layer directly obscuring the other.
    gfx::Transform neg4Translate;
    neg4Translate.Translate3d(0, 0, -4);
    LayerShape front(2, 2, neg4Translate);

    gfx::Transform neg5Translate;
    neg5Translate.Translate3d(0, 0, -5);
    LayerShape back(2, 2, neg5Translate);

    overlapResult = LayerSorter::checkOverlap(&front, &back, zThreshold, weight);
    EXPECT_EQ(LayerSorter::BBeforeA, overlapResult);
    EXPECT_EQ(1, weight);

    overlapResult = LayerSorter::checkOverlap(&back, &front, zThreshold, weight);
    EXPECT_EQ(LayerSorter::ABeforeB, overlapResult);
    EXPECT_EQ(1, weight);

    // One layer translated off to the right. No overlap should be detected.
    gfx::Transform rightTranslate;
    rightTranslate.Translate3d(10, 0, -5);
    LayerShape backRight(2, 2, rightTranslate);
    overlapResult = LayerSorter::checkOverlap(&front, &backRight, zThreshold, weight);
    EXPECT_EQ(LayerSorter::None, overlapResult);

    // When comparing a layer with itself, z difference is always 0.
    overlapResult = LayerSorter::checkOverlap(&front, &front, zThreshold, weight);
    EXPECT_EQ(0, weight);
}

TEST(LayerSorterTest, RightAngleOverlap)
{
    LayerSorter::ABCompareResult overlapResult;
    const float zThreshold = 0.1f;
    float weight = 0;

    gfx::Transform perspectiveMatrix;
    perspectiveMatrix.ApplyPerspectiveDepth(1000);

    // Two layers forming a right angle with a perspective viewing transform.
    gfx::Transform leftFaceMatrix;
    leftFaceMatrix.Translate3d(-1, 0, -5);
    leftFaceMatrix.RotateAboutYAxis(-90);
    leftFaceMatrix.Translate(-1, -1);
    LayerShape leftFace(2, 2, perspectiveMatrix * leftFaceMatrix);
    gfx::Transform frontFaceMatrix;
    frontFaceMatrix.Translate3d(0, 0, -4);
    frontFaceMatrix.Translate(-1, -1);
    LayerShape frontFace(2, 2, perspectiveMatrix * frontFaceMatrix);

    overlapResult = LayerSorter::checkOverlap(&frontFace, &leftFace, zThreshold, weight);
    EXPECT_EQ(LayerSorter::BBeforeA, overlapResult);
}

TEST(LayerSorterTest, IntersectingLayerOverlap)
{
    LayerSorter::ABCompareResult overlapResult;
    const float zThreshold = 0.1f;
    float weight = 0;

    gfx::Transform perspectiveMatrix;
    perspectiveMatrix.ApplyPerspectiveDepth(1000);

    // Intersecting layers. An explicit order will be returned based on relative z
    // values at the overlapping features but the weight returned should be zero.
    gfx::Transform frontFaceMatrix;
    frontFaceMatrix.Translate3d(0, 0, -4);
    frontFaceMatrix.Translate(-1, -1);
    LayerShape frontFace(2, 2, perspectiveMatrix * frontFaceMatrix);

    gfx::Transform throughMatrix;
    throughMatrix.Translate3d(0, 0, -4);
    throughMatrix.RotateAboutYAxis(45);
    throughMatrix.Translate(-1, -1);
    LayerShape rotatedFace(2, 2, perspectiveMatrix * throughMatrix);
    overlapResult = LayerSorter::checkOverlap(&frontFace, &rotatedFace, zThreshold, weight);
    EXPECT_NE(LayerSorter::None, overlapResult);
    EXPECT_EQ(0, weight);
}

TEST(LayerSorterTest, LayersAtAngleOverlap)
{
    LayerSorter::ABCompareResult overlapResult;
    const float zThreshold = 0.1f;
    float weight = 0;

    // Trickier test with layers at an angle.
    //
    //   -x . . . . 0 . . . . +x
    // -z             /
    //  :            /----B----
    //  0           C
    //  : ----A----/
    // +z         /
    //
    // C is in front of A and behind B (not what you'd expect by comparing centers).
    // A and B don't overlap, so they're incomparable.

    gfx::Transform transformA;
    transformA.Translate3d(-6, 0, 1);
    transformA.Translate(-4, -10);
    LayerShape layerA(8, 20, transformA);

    gfx::Transform transformB;
    transformB.Translate3d(6, 0, -1);
    transformB.Translate(-4, -10);
    LayerShape layerB(8, 20, transformB);

    gfx::Transform transformC;
    transformC.RotateAboutYAxis(40);
    transformC.Translate(-4, -10);
    LayerShape layerC(8, 20, transformC);

    overlapResult = LayerSorter::checkOverlap(&layerA, &layerC, zThreshold, weight);
    EXPECT_EQ(LayerSorter::ABeforeB, overlapResult);
    overlapResult = LayerSorter::checkOverlap(&layerC, &layerB, zThreshold, weight);
    EXPECT_EQ(LayerSorter::ABeforeB, overlapResult);
    overlapResult = LayerSorter::checkOverlap(&layerA, &layerB, zThreshold, weight);
    EXPECT_EQ(LayerSorter::None, overlapResult);
}

TEST(LayerSorterTest, LayersUnderPathologicalPerspectiveTransform)
{
    LayerSorter::ABCompareResult overlapResult;
    const float zThreshold = 0.1f;
    float weight = 0;

    // On perspective projection, if w becomes negative, the re-projected point will be
    // invalid and un-usable. Correct code needs to clip away portions of the geometry
    // where w < 0. If the code uses the invalid value, it will think that a layer has
    // different bounds than it really does, which can cause things to sort incorrectly.

    gfx::Transform perspectiveMatrix;
    perspectiveMatrix.ApplyPerspectiveDepth(1);

    gfx::Transform transformA;
    transformA.Translate3d(-15, 0, -2);
    transformA.Translate(-5, -5);
    LayerShape layerA(10, 10, perspectiveMatrix * transformA);

    // With this sequence of transforms, when layer B is correctly clipped, it will be
    // visible on the left half of the projection plane, in front of layerA. When it is
    // not clipped, its bounds will actually incorrectly appear much smaller and the
    // correct sorting dependency will not be found.
    gfx::Transform transformB;
    transformB.Translate3d(0, 0, 0.7);
    MathUtil::rotateEulerAngles(&transformB, 0, 45, 0);
    transformB.Translate(-5, -5);
    LayerShape layerB(10, 10, perspectiveMatrix * transformB);

    // Sanity check that the test case actually covers the intended scenario, where part
    // of layer B go behind the w = 0 plane.
    gfx::QuadF testQuad = gfx::QuadF(gfx::RectF(-0.5, -0.5, 1, 1));
    bool clipped = false;
    MathUtil::mapQuad(perspectiveMatrix * transformB, testQuad, clipped);
    ASSERT_TRUE(clipped);

    overlapResult = LayerSorter::checkOverlap(&layerA, &layerB, zThreshold, weight);
    EXPECT_EQ(LayerSorter::ABeforeB, overlapResult);
}

TEST(LayerSorterTest, verifyExistingOrderingPreservedWhenNoZDiff)
{
    // If there is no reason to re-sort the layers (i.e. no 3d z difference), then the
    // existing ordering provided on input should be retained. This test covers the fix in
    // https://bugs.webkit.org/show_bug.cgi?id=75046. Before this fix, ordering was
    // accidentally reversed, causing bugs in z-index ordering on websites when
    // preserves3D triggered the LayerSorter.

    // Input list of layers: [1, 2, 3, 4, 5].
    // Expected output: [3, 4, 1, 2, 5].
    //    - 1, 2, and 5 do not have a 3d z difference, and therefore their relative ordering should be retained.
    //    - 3 and 4 do not have a 3d z difference, and therefore their relative ordering should be retained.
    //    - 3 and 4 should be re-sorted so they are in front of 1, 2, and 5.

    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    scoped_ptr<LayerImpl> layer1 = LayerImpl::create(hostImpl.activeTree(), 1);
    scoped_ptr<LayerImpl> layer2 = LayerImpl::create(hostImpl.activeTree(), 2);
    scoped_ptr<LayerImpl> layer3 = LayerImpl::create(hostImpl.activeTree(), 3);
    scoped_ptr<LayerImpl> layer4 = LayerImpl::create(hostImpl.activeTree(), 4);
    scoped_ptr<LayerImpl> layer5 = LayerImpl::create(hostImpl.activeTree(), 5);

    gfx::Transform BehindMatrix;
    BehindMatrix.Translate3d(0, 0, 2);
    gfx::Transform FrontMatrix;
    FrontMatrix.Translate3d(0, 0, 1);

    layer1->setBounds(gfx::Size(10, 10));
    layer1->setContentBounds(gfx::Size(10, 10));
    layer1->drawProperties().target_space_transform = BehindMatrix;
    layer1->setDrawsContent(true);

    layer2->setBounds(gfx::Size(20, 20));
    layer2->setContentBounds(gfx::Size(20, 20));
    layer2->drawProperties().target_space_transform = BehindMatrix;
    layer2->setDrawsContent(true);

    layer3->setBounds(gfx::Size(30, 30));
    layer3->setContentBounds(gfx::Size(30, 30));
    layer3->drawProperties().target_space_transform = FrontMatrix;
    layer3->setDrawsContent(true);

    layer4->setBounds(gfx::Size(40, 40));
    layer4->setContentBounds(gfx::Size(40, 40));
    layer4->drawProperties().target_space_transform = FrontMatrix;
    layer4->setDrawsContent(true);

    layer5->setBounds(gfx::Size(50, 50));
    layer5->setContentBounds(gfx::Size(50, 50));
    layer5->drawProperties().target_space_transform = BehindMatrix;
    layer5->setDrawsContent(true);

    std::vector<LayerImpl*> layerList;
    layerList.push_back(layer1.get());
    layerList.push_back(layer2.get());
    layerList.push_back(layer3.get());
    layerList.push_back(layer4.get());
    layerList.push_back(layer5.get());

    ASSERT_EQ(static_cast<size_t>(5), layerList.size());
    EXPECT_EQ(1, layerList[0]->id());
    EXPECT_EQ(2, layerList[1]->id());
    EXPECT_EQ(3, layerList[2]->id());
    EXPECT_EQ(4, layerList[3]->id());
    EXPECT_EQ(5, layerList[4]->id());

    LayerSorter layerSorter;
    layerSorter.sort(layerList.begin(), layerList.end());

    ASSERT_EQ(static_cast<size_t>(5), layerList.size());
    EXPECT_EQ(3, layerList[0]->id());
    EXPECT_EQ(4, layerList[1]->id());
    EXPECT_EQ(1, layerList[2]->id());
    EXPECT_EQ(2, layerList[3]->id());
    EXPECT_EQ(5, layerList[4]->id());
}

TEST(LayerSorterTest, verifyConcidentLayerPrecisionLossResultsInDocumentOrder)
{
    FakeImplProxy proxy;
    FakeLayerTreeHostImpl hostImpl(&proxy);

    scoped_ptr<LayerImpl> layer1 = LayerImpl::create(hostImpl.activeTree(), 1);
    scoped_ptr<LayerImpl> layer2 = LayerImpl::create(hostImpl.activeTree(), 2);

    // Layer 1 should occur before layer 2 in paint.  However, due to numeric
    // issues in the sorter, it will put the layers in the wrong order
    // in some situations.  Here we test a patch that results in  document
    // order rather than calculated order when numeric percision is suspect
    // in calculated order.

    gfx::Transform BehindMatrix;
    BehindMatrix.Translate3d(0, 0, 0.999999f);
    BehindMatrix.RotateAboutXAxis(38.5f);
    BehindMatrix.RotateAboutYAxis(77.0f);
    gfx::Transform FrontMatrix;
    FrontMatrix.Translate3d(0, 0, 1.0f);
    FrontMatrix.RotateAboutXAxis(38.5f);
    FrontMatrix.RotateAboutYAxis(77.0f);

    layer1->setBounds(gfx::Size(10, 10));
    layer1->setContentBounds(gfx::Size(10, 10));
    layer1->drawProperties().target_space_transform = BehindMatrix;
    layer1->setDrawsContent(true);

    layer2->setBounds(gfx::Size(10, 10));
    layer2->setContentBounds(gfx::Size(10, 10));
    layer2->drawProperties().target_space_transform = FrontMatrix;
    layer2->setDrawsContent(true);

    std::vector<LayerImpl*> layerList;
    layerList.push_back(layer1.get());
    layerList.push_back(layer2.get());

    ASSERT_EQ(static_cast<size_t>(2), layerList.size());
    EXPECT_EQ(1, layerList[0]->id());
    EXPECT_EQ(2, layerList[1]->id());

    LayerSorter layerSorter;
    layerSorter.sort(layerList.begin(), layerList.end());

    ASSERT_EQ(static_cast<size_t>(2), layerList.size());
    EXPECT_EQ(1, layerList[0]->id());
    EXPECT_EQ(2, layerList[1]->id());
}

}  // namespace
}  // namespace cc

