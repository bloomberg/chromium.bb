// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_GEOMETRY_TEST_UTILS_H_
#define CC_TEST_GEOMETRY_TEST_UTILS_H_

namespace gfx {
class Transform;
}

namespace cc {

// These are macros instead of functions so that we get useful line numbers where a test failed.
#define EXPECT_FLOAT_RECT_EQ(expected, actual) \
do { \
    EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
    EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
    EXPECT_FLOAT_EQ((expected).width(), (actual).width()); \
    EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
} while (false)

#define EXPECT_RECT_EQ(expected, actual) \
do { \
    EXPECT_EQ((expected).x(), (actual).x()); \
    EXPECT_EQ((expected).y(), (actual).y()); \
    EXPECT_EQ((expected).width(), (actual).width()); \
    EXPECT_EQ((expected).height(), (actual).height()); \
} while (false)

#define EXPECT_SIZE_EQ(expected, actual) \
do { \
    EXPECT_EQ((expected).width(), (actual).width()); \
    EXPECT_EQ((expected).height(), (actual).height()); \
} while (false)

#define EXPECT_POINT_EQ(expected, actual) \
do { \
    EXPECT_EQ((expected).x(), (actual).x()); \
    EXPECT_EQ((expected).y(), (actual).y()); \
} while (false)

#define EXPECT_POINT3F_EQ(expected, actual) \
do { \
    EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
    EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
    EXPECT_FLOAT_EQ((expected).z(), (actual).z()); \
} while (false)

#define EXPECT_VECTOR_EQ(expected, actual) \
do { \
    EXPECT_EQ((expected).x(), (actual).x()); \
    EXPECT_EQ((expected).y(), (actual).y()); \
} while (false)

#define EXPECT_FLOAT_ARRAY_EQ(expected, actual, count) \
do { \
    for (int i = 0; i < count; i++) {\
        EXPECT_FLOAT_EQ((expected)[i], (actual)[i]); \
    }\
} while (false)

// This is a function rather than a macro because when this is included as a macro
// in bulk, it causes a significant slow-down in compilation time. This problem
// exists with both gcc and clang, and bugs have been filed at
// http://llvm.org/bugs/show_bug.cgi?id=13651 and http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54337
void ExpectTransformationMatrixEq(const gfx::Transform& expected,
                                  const gfx::Transform& actual);

#define EXPECT_TRANSFORMATION_MATRIX_EQ(expected, actual)            \
    {                                                                \
        SCOPED_TRACE("");                                            \
        cc::ExpectTransformationMatrixEq(expected, actual);          \
    }

// Should be used in test code only, for convenience. Production code should use
// the gfx::Transform::GetInverse() API.
gfx::Transform inverse(const gfx::Transform& transform);

}  // namespace cc

#endif  // CC_TEST_GEOMETRY_TEST_UTILS_H_
