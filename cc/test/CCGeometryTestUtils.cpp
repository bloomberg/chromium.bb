// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCGeometryTestUtils.h"

#include <gtest/gtest.h>
#include <public/WebTransformationMatrix.h>

namespace WebKitTests {

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

} // namespace WebKitTests

