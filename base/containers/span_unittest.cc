// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

#include <vector>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// TODO(dcheng): Add tests for initializer list, containers, etc.

TEST(SpanTest, ConstructFromDataAndSize) {
  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  Span<int> span(vector.data(), vector.size());
  EXPECT_EQ(vector.data(), span.data());
  EXPECT_EQ(vector.size(), span.size());

  // TODO(dcheng): Use operator[] when implemented.
  for (size_t i = 0; i < span.size(); ++i)
    EXPECT_EQ(vector[i], span.data()[i]);
}

TEST(SpanTest, ConstructFromConstexprArray) {
  static constexpr int kArray[] = {5, 4, 3, 2, 1};

  constexpr Span<const int> span(kArray);
  EXPECT_EQ(kArray, span.data());
  EXPECT_EQ(arraysize(kArray), span.size());

  // TODO(dcheng): Use operator[] when implemented.
  for (size_t i = 0; i < span.size(); ++i)
    EXPECT_EQ(kArray[i], span.data()[i]);
}

TEST(SpanTest, ConstructFromArray) {
  int array[] = {5, 4, 3, 2, 1};

  Span<const int> const_span(array);
  EXPECT_EQ(array, const_span.data());
  EXPECT_EQ(arraysize(array), const_span.size());
  // TODO(dcheng): Use operator[] when implemented.
  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(array[i], const_span.data()[i]);

  Span<int> span(array);
  EXPECT_EQ(array, span.data());
  EXPECT_EQ(arraysize(array), span.size());
  // TODO(dcheng): Use operator[] when implemented.
  for (size_t i = 0; i < span.size(); ++i)
    EXPECT_EQ(array[i], span.data()[i]);
}

TEST(SpanTest, Subspan) {
  int array[] = {1, 2, 3};
  Span<int> span(array);

  {
    auto subspan = span.subspan(0, 0);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(1, 0);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(2, 0);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.subspan(0, 1);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(1, subspan.data()[0]);
  }

  {
    auto subspan = span.subspan(1, 1);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(2, subspan.data()[0]);
  }

  {
    auto subspan = span.subspan(2, 1);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan.data()[0]);
  }

  {
    auto subspan = span.subspan(0, 2);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(1, subspan.data()[0]);
    EXPECT_EQ(2, subspan.data()[1]);
  }

  {
    auto subspan = span.subspan(1, 2);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan.data()[0]);
    EXPECT_EQ(3, subspan.data()[1]);
  }

  {
    auto subspan = span.subspan(0, 3);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(span.size(), subspan.size());
    EXPECT_EQ(1, subspan.data()[0]);
    EXPECT_EQ(2, subspan.data()[1]);
    EXPECT_EQ(3, subspan.data()[2]);
  }
}

}  // namespace base
