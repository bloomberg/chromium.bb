// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/keyframed_animation_curve.h"

#include "cc/transform_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformationMatrix.h"

using WebKit::WebTransformationMatrix;

namespace cc {
namespace {

void expectTranslateX(double translateX, const WebTransformationMatrix& matrix)
{
    EXPECT_FLOAT_EQ(translateX, matrix.m41());
}

// Tests that a float animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneFloatKeyframe)
{
    scoped_ptr<KeyframedFloatAnimationCurve> curve(KeyframedFloatAnimationCurve::create());
    curve->addKeyframe(FloatKeyframe::create(0, 2, scoped_ptr<TimingFunction>()));
    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(2, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(2, curve->getValue(1));
    EXPECT_FLOAT_EQ(2, curve->getValue(2));
}

// Tests that a float animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoFloatKeyframe)
{
    scoped_ptr<KeyframedFloatAnimationCurve> curve(KeyframedFloatAnimationCurve::create());
    curve->addKeyframe(FloatKeyframe::create(0, 2, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(FloatKeyframe::create(1, 4, scoped_ptr<TimingFunction>()));
    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve->getValue(1));
    EXPECT_FLOAT_EQ(4, curve->getValue(2));
}

// Tests that a float animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeFloatKeyframe)
{
    scoped_ptr<KeyframedFloatAnimationCurve> curve(KeyframedFloatAnimationCurve::create());
    curve->addKeyframe(FloatKeyframe::create(0, 2, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(FloatKeyframe::create(1, 4, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(FloatKeyframe::create(2, 8, scoped_ptr<TimingFunction>()));
    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve->getValue(1));
    EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
    EXPECT_FLOAT_EQ(8, curve->getValue(2));
    EXPECT_FLOAT_EQ(8, curve->getValue(3));
}

// Tests that a float animation with multiple keys at a given time works sanely.
TEST(KeyframedAnimationCurveTest, RepeatedFloatKeyTimes)
{
    scoped_ptr<KeyframedFloatAnimationCurve> curve(KeyframedFloatAnimationCurve::create());
    curve->addKeyframe(FloatKeyframe::create(0, 4, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(FloatKeyframe::create(1, 4, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(FloatKeyframe::create(1, 6, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(FloatKeyframe::create(2, 6, scoped_ptr<TimingFunction>()));

    EXPECT_FLOAT_EQ(4, curve->getValue(-1));
    EXPECT_FLOAT_EQ(4, curve->getValue(0));
    EXPECT_FLOAT_EQ(4, curve->getValue(0.5));

    // There is a discontinuity at 1. Any value between 4 and 6 is valid.
    float value = curve->getValue(1);
    EXPECT_TRUE(value >= 4 && value <= 6);

    EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
    EXPECT_FLOAT_EQ(6, curve->getValue(2));
    EXPECT_FLOAT_EQ(6, curve->getValue(3));
}


// Tests that a transform animation with one keyframe works as expected.
TEST(KeyframedAnimationCurveTest, OneTransformKeyframe)
{
    scoped_ptr<KeyframedTransformAnimationCurve> curve(KeyframedTransformAnimationCurve::create());
    TransformOperations operations;
    operations.AppendTranslate(2, 0, 0);
    curve->addKeyframe(TransformKeyframe::create(0, operations, scoped_ptr<TimingFunction>()));

    expectTranslateX(2, curve->getValue(-1));
    expectTranslateX(2, curve->getValue(0));
    expectTranslateX(2, curve->getValue(0.5));
    expectTranslateX(2, curve->getValue(1));
    expectTranslateX(2, curve->getValue(2));
}

// Tests that a transform animation with two keyframes works as expected.
TEST(KeyframedAnimationCurveTest, TwoTransformKeyframe)
{
    scoped_ptr<KeyframedTransformAnimationCurve> curve(KeyframedTransformAnimationCurve::create());
    TransformOperations operations1;
    operations1.AppendTranslate(2, 0, 0);
    TransformOperations operations2;
    operations2.AppendTranslate(4, 0, 0);

    curve->addKeyframe(TransformKeyframe::create(0, operations1, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(TransformKeyframe::create(1, operations2, scoped_ptr<TimingFunction>()));
    expectTranslateX(2, curve->getValue(-1));
    expectTranslateX(2, curve->getValue(0));
    expectTranslateX(3, curve->getValue(0.5));
    expectTranslateX(4, curve->getValue(1));
    expectTranslateX(4, curve->getValue(2));
}

// Tests that a transform animation with three keyframes works as expected.
TEST(KeyframedAnimationCurveTest, ThreeTransformKeyframe)
{
    scoped_ptr<KeyframedTransformAnimationCurve> curve(KeyframedTransformAnimationCurve::create());
    TransformOperations operations1;
    operations1.AppendTranslate(2, 0, 0);
    TransformOperations operations2;
    operations2.AppendTranslate(4, 0, 0);
    TransformOperations operations3;
    operations3.AppendTranslate(8, 0, 0);
    curve->addKeyframe(TransformKeyframe::create(0, operations1, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(TransformKeyframe::create(1, operations2, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(TransformKeyframe::create(2, operations3, scoped_ptr<TimingFunction>()));
    expectTranslateX(2, curve->getValue(-1));
    expectTranslateX(2, curve->getValue(0));
    expectTranslateX(3, curve->getValue(0.5));
    expectTranslateX(4, curve->getValue(1));
    expectTranslateX(6, curve->getValue(1.5));
    expectTranslateX(8, curve->getValue(2));
    expectTranslateX(8, curve->getValue(3));
}

// Tests that a transform animation with multiple keys at a given time works sanely.
TEST(KeyframedAnimationCurveTest, RepeatedTransformKeyTimes)
{
    scoped_ptr<KeyframedTransformAnimationCurve> curve(KeyframedTransformAnimationCurve::create());
    // A step function.
    TransformOperations operations1;
    operations1.AppendTranslate(4, 0, 0);
    TransformOperations operations2;
    operations2.AppendTranslate(4, 0, 0);
    TransformOperations operations3;
    operations3.AppendTranslate(6, 0, 0);
    TransformOperations operations4;
    operations4.AppendTranslate(6, 0, 0);
    curve->addKeyframe(TransformKeyframe::create(0, operations1, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(TransformKeyframe::create(1, operations2, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(TransformKeyframe::create(1, operations3, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(TransformKeyframe::create(2, operations4, scoped_ptr<TimingFunction>()));

    expectTranslateX(4, curve->getValue(-1));
    expectTranslateX(4, curve->getValue(0));
    expectTranslateX(4, curve->getValue(0.5));

    // There is a discontinuity at 1. Any value between 4 and 6 is valid.
    WebTransformationMatrix value = curve->getValue(1);
    EXPECT_TRUE(value.m41() >= 4 && value.m41() <= 6);

    expectTranslateX(6, curve->getValue(1.5));
    expectTranslateX(6, curve->getValue(2));
    expectTranslateX(6, curve->getValue(3));
}

// Tests that the keyframes may be added out of order.
TEST(KeyframedAnimationCurveTest, UnsortedKeyframes)
{
    scoped_ptr<KeyframedFloatAnimationCurve> curve(KeyframedFloatAnimationCurve::create());
    curve->addKeyframe(FloatKeyframe::create(2, 8, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(FloatKeyframe::create(0, 2, scoped_ptr<TimingFunction>()));
    curve->addKeyframe(FloatKeyframe::create(1, 4, scoped_ptr<TimingFunction>()));
    EXPECT_FLOAT_EQ(2, curve->getValue(-1));
    EXPECT_FLOAT_EQ(2, curve->getValue(0));
    EXPECT_FLOAT_EQ(3, curve->getValue(0.5));
    EXPECT_FLOAT_EQ(4, curve->getValue(1));
    EXPECT_FLOAT_EQ(6, curve->getValue(1.5));
    EXPECT_FLOAT_EQ(8, curve->getValue(2));
    EXPECT_FLOAT_EQ(8, curve->getValue(3));
}

// Tests that a cubic bezier timing function works as expected.
TEST(KeyframedAnimationCurveTest, CubicBezierTimingFunction)
{
    scoped_ptr<KeyframedFloatAnimationCurve> curve(KeyframedFloatAnimationCurve::create());
    curve->addKeyframe(FloatKeyframe::create(0, 0, CubicBezierTimingFunction::create(0.25, 0, 0.75, 1).PassAs<TimingFunction>()));
    curve->addKeyframe(FloatKeyframe::create(1, 1, scoped_ptr<TimingFunction>()));

    EXPECT_FLOAT_EQ(0, curve->getValue(0));
    EXPECT_LT(0, curve->getValue(0.25));
    EXPECT_GT(0.25, curve->getValue(0.25));
    EXPECT_NEAR(curve->getValue(0.5), 0.5, 0.00015);
    EXPECT_LT(0.75, curve->getValue(0.75));
    EXPECT_GT(1, curve->getValue(0.75));
    EXPECT_FLOAT_EQ(1, curve->getValue(1));
}

}  // namespace
}  // namespace cc
