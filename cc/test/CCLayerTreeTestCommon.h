// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCLayerTreeTestCommon_h
#define CCLayerTreeTestCommon_h

#include <public/WebTransformationMatrix.h>

namespace WebKitTests {

// These are macros instead of functions so that we get useful line numbers where a test failed.
#define EXPECT_FLOAT_RECT_EQ(expected, actual)                          \
    EXPECT_FLOAT_EQ((expected).location().x(), (actual).location().x()); \
    EXPECT_FLOAT_EQ((expected).location().y(), (actual).location().y()); \
    EXPECT_FLOAT_EQ((expected).size().width(), (actual).size().width()); \
    EXPECT_FLOAT_EQ((expected).size().height(), (actual).size().height())

#define EXPECT_INT_RECT_EQ(expected, actual)                            \
    EXPECT_EQ((expected).location().x(), (actual).location().x());      \
    EXPECT_EQ((expected).location().y(), (actual).location().y());      \
    EXPECT_EQ((expected).size().width(), (actual).size().width());      \
    EXPECT_EQ((expected).size().height(), (actual).size().height())

// This is a function rather than a macro because when this is included as a macro
// in bulk, it causes a significant slow-down in compilation time. This problem
// exists with both gcc and clang, and bugs have been filed at
// http://llvm.org/bugs/show_bug.cgi?id=13651 and http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54337
void ExpectTransformationMatrixEq(WebKit::WebTransformationMatrix expected,
                                  WebKit::WebTransformationMatrix actual);

#define EXPECT_TRANSFORMATION_MATRIX_EQ(expected, actual)            \
    {                                                                \
        SCOPED_TRACE("");                                            \
        WebKitTests::ExpectTransformationMatrixEq(expected, actual); \
    }

} // namespace

#endif // CCLayerTreeTestCommon_h
