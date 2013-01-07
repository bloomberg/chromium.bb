// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/geometry_test_utils.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformationMatrix.h"
#include "ui/gfx/transform.h"

namespace cc {

// NOTE: even though transform data types use double precision, we only check
// for equality within single-precision error bounds because many transforms
// originate from single-precision data types such as quads/rects/etc.

void ExpectTransformationMatrixEq(const WebKit::WebTransformationMatrix& expected,
                                  const WebKit::WebTransformationMatrix& actual)
{
    EXPECT_FLOAT_EQ((expected).m11(), (actual).m11());
    EXPECT_FLOAT_EQ((expected).m12(), (actual).m12());
    EXPECT_FLOAT_EQ((expected).m13(), (actual).m13());
    EXPECT_FLOAT_EQ((expected).m14(), (actual).m14());
    EXPECT_FLOAT_EQ((expected).m21(), (actual).m21());
    EXPECT_FLOAT_EQ((expected).m22(), (actual).m22());
    EXPECT_FLOAT_EQ((expected).m23(), (actual).m23());
    EXPECT_FLOAT_EQ((expected).m24(), (actual).m24());
    EXPECT_FLOAT_EQ((expected).m31(), (actual).m31());
    EXPECT_FLOAT_EQ((expected).m32(), (actual).m32());
    EXPECT_FLOAT_EQ((expected).m33(), (actual).m33());
    EXPECT_FLOAT_EQ((expected).m34(), (actual).m34());
    EXPECT_FLOAT_EQ((expected).m41(), (actual).m41());
    EXPECT_FLOAT_EQ((expected).m42(), (actual).m42());
    EXPECT_FLOAT_EQ((expected).m43(), (actual).m43());
    EXPECT_FLOAT_EQ((expected).m44(), (actual).m44());
}

void ExpectTransformationMatrixEq(const gfx::Transform& expected,
                                  const gfx::Transform& actual)
{
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(0, 0), (actual).matrix().getDouble(0, 0));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(1, 0), (actual).matrix().getDouble(1, 0));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(2, 0), (actual).matrix().getDouble(2, 0));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(3, 0), (actual).matrix().getDouble(3, 0));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(0, 1), (actual).matrix().getDouble(0, 1));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(1, 1), (actual).matrix().getDouble(1, 1));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(2, 1), (actual).matrix().getDouble(2, 1));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(3, 1), (actual).matrix().getDouble(3, 1));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(0, 2), (actual).matrix().getDouble(0, 2));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(1, 2), (actual).matrix().getDouble(1, 2));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(2, 2), (actual).matrix().getDouble(2, 2));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(3, 2), (actual).matrix().getDouble(3, 2));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(0, 3), (actual).matrix().getDouble(0, 3));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(1, 3), (actual).matrix().getDouble(1, 3));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(2, 3), (actual).matrix().getDouble(2, 3));
    EXPECT_FLOAT_EQ((expected).matrix().getDouble(3, 3), (actual).matrix().getDouble(3, 3));
}

gfx::Transform inverse(const gfx::Transform& transform)
{
    gfx::Transform result(gfx::Transform::kSkipInitialization);
    bool invertedSuccessfully = transform.GetInverse(&result);
    DCHECK(invertedSuccessfully);
    return result;
}

}  // namespace cc
