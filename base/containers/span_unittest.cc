// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pointwise;

namespace base {

TEST(SpanTest, DefaultConstructor) {
  span<int> span;
  EXPECT_EQ(nullptr, span.data());
  EXPECT_EQ(0u, span.size());
}

TEST(SpanTest, ConstructFromDataAndSize) {
  {
    span<int> empty_span(nullptr, 0);
    EXPECT_TRUE(empty_span.empty());
    EXPECT_EQ(nullptr, empty_span.data());
  }

  {
    std::vector<int> vector = {1, 1, 2, 3, 5, 8};

    span<int> span(vector.data(), vector.size());
    EXPECT_EQ(vector.data(), span.data());
    EXPECT_EQ(vector.size(), span.size());

    for (size_t i = 0; i < span.size(); ++i)
      EXPECT_EQ(vector[i], span[i]);
  }
}

TEST(SpanTest, ConstructFromPointerPair) {
  {
    span<int> empty_span(nullptr, nullptr);
    EXPECT_TRUE(empty_span.empty());
    EXPECT_EQ(nullptr, empty_span.data());
  }

  {
    std::vector<int> vector = {1, 1, 2, 3, 5, 8};

    span<int> span(vector.data(), vector.data() + vector.size() / 2);
    EXPECT_EQ(vector.data(), span.data());
    EXPECT_EQ(vector.size() / 2, span.size());

    for (size_t i = 0; i < span.size(); ++i)
      EXPECT_EQ(vector[i], span[i]);
  }
}

TEST(SpanTest, ConstructFromConstexprArray) {
  static constexpr int kArray[] = {5, 4, 3, 2, 1};

  constexpr span<const int> span(kArray);
  EXPECT_EQ(kArray, span.data());
  EXPECT_EQ(arraysize(kArray), span.size());

  for (size_t i = 0; i < span.size(); ++i)
    EXPECT_EQ(kArray[i], span[i]);
}

TEST(SpanTest, ConstructFromArray) {
  int array[] = {5, 4, 3, 2, 1};

  span<const int> const_span(array);
  EXPECT_EQ(array, const_span.data());
  EXPECT_EQ(arraysize(array), const_span.size());
  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(array[i], const_span[i]);

  span<int> span(array);
  EXPECT_EQ(array, span.data());
  EXPECT_EQ(arraysize(array), span.size());
  for (size_t i = 0; i < span.size(); ++i)
    EXPECT_EQ(array[i], span[i]);
}

TEST(SpanTest, ConstructFromStdArray) {
  // Note: Constructing a constexpr span from a constexpr std::array does not
  // work prior to C++17 due to non-constexpr std::array::data.
  std::array<int, 5> array = {5, 4, 3, 2, 1};

  span<const int> const_span(array);
  EXPECT_EQ(array.data(), const_span.data());
  EXPECT_EQ(array.size(), const_span.size());
  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(array[i], const_span[i]);

  span<int> span(array);
  EXPECT_EQ(array.data(), span.data());
  EXPECT_EQ(array.size(), span.size());
  for (size_t i = 0; i < span.size(); ++i)
    EXPECT_EQ(array[i], span[i]);
}

TEST(SpanTest, ConstructFromInitializerList) {
  std::initializer_list<int> il = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(il);
  EXPECT_EQ(il.begin(), const_span.data());
  EXPECT_EQ(il.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(il.begin()[i], const_span[i]);
}

TEST(SpanTest, ConstructFromStdString) {
  std::string str = "foobar";

  span<const char> const_span(str);
  EXPECT_EQ(str.data(), const_span.data());
  EXPECT_EQ(str.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(str[i], const_span[i]);

  span<char> span(str);
  EXPECT_EQ(str.data(), span.data());
  EXPECT_EQ(str.size(), span.size());

  for (size_t i = 0; i < span.size(); ++i)
    EXPECT_EQ(str[i], span[i]);
}

TEST(SpanTest, ConstructFromConstContainer) {
  const std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(vector);
  EXPECT_EQ(vector.data(), const_span.data());
  EXPECT_EQ(vector.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(vector[i], const_span[i]);
}

TEST(SpanTest, ConstructFromContainer) {
  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<const int> const_span(vector);
  EXPECT_EQ(vector.data(), const_span.data());
  EXPECT_EQ(vector.size(), const_span.size());

  for (size_t i = 0; i < const_span.size(); ++i)
    EXPECT_EQ(vector[i], const_span[i]);

  span<int> span(vector);
  EXPECT_EQ(vector.data(), span.data());
  EXPECT_EQ(vector.size(), span.size());

  for (size_t i = 0; i < span.size(); ++i)
    EXPECT_EQ(vector[i], span[i]);
}

TEST(SpanTest, ConvertNonConstIntegralToConst) {
  std::vector<int> vector = {1, 1, 2, 3, 5, 8};

  span<int> int_span(vector.data(), vector.size());
  span<const int> const_span(int_span);
  EXPECT_THAT(const_span, Pointwise(Eq(), int_span));
}

TEST(SpanTest, ConvertNonConstPointerToConst) {
  auto a = std::make_unique<int>(11);
  auto b = std::make_unique<int>(22);
  auto c = std::make_unique<int>(33);
  std::vector<int*> vector = {a.get(), b.get(), c.get()};

  span<int*> non_const_pointer_span(vector);
  EXPECT_THAT(non_const_pointer_span, Pointwise(Eq(), vector));
  span<int* const> const_pointer_span(non_const_pointer_span);
  EXPECT_THAT(const_pointer_span, Pointwise(Eq(), non_const_pointer_span));
  // Note: no test for conversion from span<int> to span<const int*>, since that
  // would imply a conversion from int** to const int**, which is unsafe.
  //
  // Note: no test for conversion from span<int*> to span<const int* const>,
  // due to CWG Defect 330:
  // http://open-std.org/JTC1/SC22/WG21/docs/cwg_defects.html#330
}

TEST(SpanTest, ConvertBetweenEquivalentTypes) {
  std::vector<int32_t> vector = {2, 4, 8, 16, 32};

  span<int32_t> int32_t_span(vector);
  span<int> converted_span(int32_t_span);
  EXPECT_EQ(int32_t_span, converted_span);
}

TEST(SpanTest, First) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.first(0);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.first(1);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.first(2);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.first(3);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, Last) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.last(0);
    EXPECT_EQ(span.data() + 3, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

  {
    auto subspan = span.last(1);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.last(2);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.last(3);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, Subspan) {
  int array[] = {1, 2, 3};
  span<int> span(array);

  {
    auto subspan = span.subspan(0);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(3u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }

  {
    auto subspan = span.subspan(1);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan(2);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan(3);
    EXPECT_EQ(span.data() + 3, subspan.data());
    EXPECT_EQ(0u, subspan.size());
  }

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
    EXPECT_EQ(1, subspan[0]);
  }

  {
    auto subspan = span.subspan(1, 1);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
  }

  {
    auto subspan = span.subspan(2, 1);
    EXPECT_EQ(span.data() + 2, subspan.data());
    EXPECT_EQ(1u, subspan.size());
    EXPECT_EQ(3, subspan[0]);
  }

  {
    auto subspan = span.subspan(0, 2);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
  }

  {
    auto subspan = span.subspan(1, 2);
    EXPECT_EQ(span.data() + 1, subspan.data());
    EXPECT_EQ(2u, subspan.size());
    EXPECT_EQ(2, subspan[0]);
    EXPECT_EQ(3, subspan[1]);
  }

  {
    auto subspan = span.subspan(0, 3);
    EXPECT_EQ(span.data(), subspan.data());
    EXPECT_EQ(span.size(), subspan.size());
    EXPECT_EQ(1, subspan[0]);
    EXPECT_EQ(2, subspan[1]);
    EXPECT_EQ(3, subspan[2]);
  }
}

TEST(SpanTest, Size) {
  {
    span<int> span;
    EXPECT_EQ(0u, span.size());
  }

  {
    int array[] = {1, 2, 3};
    span<int> span(array);
    EXPECT_EQ(3u, span.size());
  }
}

TEST(SpanTest, SizeBytes) {
  {
    span<int> span;
    EXPECT_EQ(0u, span.size_bytes());
  }

  {
    int array[] = {1, 2, 3};
    span<int> span(array);
    EXPECT_EQ(3u * sizeof(int), span.size_bytes());
  }
}

TEST(SpanTest, Empty) {
  {
    span<int> span;
    EXPECT_TRUE(span.empty());
  }

  {
    int array[] = {1, 2, 3};
    span<int> span(array);
    EXPECT_FALSE(span.empty());
  }
}

TEST(SpanTest, OperatorAt) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  static_assert(kArray[0] == span[0], "span[0] does not equal kArray[0]");
  static_assert(kArray[1] == span[1], "span[1] does not equal kArray[1]");
  static_assert(kArray[2] == span[2], "span[2] does not equal kArray[2]");
  static_assert(kArray[3] == span[3], "span[3] does not equal kArray[3]");
  static_assert(kArray[4] == span[4], "span[4] does not equal kArray[4]");

  static_assert(kArray[0] == span(0), "span(0) does not equal kArray[0]");
  static_assert(kArray[1] == span(1), "span(1) does not equal kArray[1]");
  static_assert(kArray[2] == span(2), "span(2) does not equal kArray[2]");
  static_assert(kArray[3] == span(3), "span(3) does not equal kArray[3]");
  static_assert(kArray[4] == span(4), "span(4) does not equal kArray[4]");
}

TEST(SpanTest, Iterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  std::vector<int> results;
  for (int i : span)
    results.emplace_back(i);
  EXPECT_THAT(results, ElementsAre(1, 6, 1, 8, 0));
}

TEST(SpanTest, ReverseIterator) {
  static constexpr int kArray[] = {1, 6, 1, 8, 0};
  constexpr span<const int> span(kArray);

  EXPECT_TRUE(std::equal(std::rbegin(kArray), std::rend(kArray), span.rbegin(),
                         span.rend()));
  EXPECT_TRUE(std::equal(std::crbegin(kArray), std::crend(kArray),
                         span.crbegin(), span.crend()));
}

TEST(SpanTest, Equality) {
  static constexpr int kArray1[] = {3, 1, 4, 1, 5};
  static constexpr int kArray2[] = {3, 1, 4, 1, 5};
  constexpr span<const int> span1(kArray1);
  constexpr span<const int> span2(kArray2);

  EXPECT_EQ(span1, span2);

  static constexpr int kArray3[] = {2, 7, 1, 8, 3};
  constexpr span<const int> span3(kArray3);

  EXPECT_FALSE(span1 == span3);

  static double kArray4[] = {2.0, 7.0, 1.0, 8.0, 3.0};
  span<double> span4(kArray4);

  EXPECT_EQ(span3, span4);
}

TEST(SpanTest, Inequality) {
  static constexpr int kArray1[] = {2, 3, 5, 7, 11};
  static constexpr int kArray2[] = {1, 4, 6, 8, 9};
  constexpr span<const int> span1(kArray1);
  constexpr span<const int> span2(kArray2);

  EXPECT_NE(span1, span2);

  static constexpr int kArray3[] = {2, 3, 5, 7, 11};
  constexpr span<const int> span3(kArray3);

  EXPECT_FALSE(span1 != span3);

  static double kArray4[] = {1.0, 4.0, 6.0, 8.0, 9.0};
  span<double> span4(kArray4);

  EXPECT_NE(span3, span4);
}

TEST(SpanTest, LessThan) {
  static constexpr int kArray1[] = {2, 3, 5, 7, 11};
  static constexpr int kArray2[] = {2, 3, 5, 7, 11, 13};
  constexpr span<const int> span1(kArray1);
  constexpr span<const int> span2(kArray2);

  EXPECT_LT(span1, span2);

  static constexpr int kArray3[] = {2, 3, 5, 7, 11};
  constexpr span<const int> span3(kArray3);

  EXPECT_FALSE(span1 < span3);

  static double kArray4[] = {2.0, 3.0, 5.0, 7.0, 11.0, 13.0};
  span<double> span4(kArray4);

  EXPECT_LT(span3, span4);
}

TEST(SpanTest, LessEqual) {
  static constexpr int kArray1[] = {2, 3, 5, 7, 11};
  static constexpr int kArray2[] = {2, 3, 5, 7, 11, 13};
  constexpr span<const int> span1(kArray1);
  constexpr span<const int> span2(kArray2);

  EXPECT_LE(span1, span1);
  EXPECT_LE(span1, span2);

  static constexpr int kArray3[] = {2, 3, 5, 7, 10};
  constexpr span<const int> span3(kArray3);

  EXPECT_FALSE(span1 <= span3);

  static double kArray4[] = {2.0, 3.0, 5.0, 7.0, 11.0, 13.0};
  span<double> span4(kArray4);

  EXPECT_LE(span3, span4);
}

TEST(SpanTest, GreaterThan) {
  static constexpr int kArray1[] = {2, 3, 5, 7, 11, 13};
  static constexpr int kArray2[] = {2, 3, 5, 7, 11};
  constexpr span<const int> span1(kArray1);
  constexpr span<const int> span2(kArray2);

  EXPECT_GT(span1, span2);

  static constexpr int kArray3[] = {2, 3, 5, 7, 11, 13};
  constexpr span<const int> span3(kArray3);

  EXPECT_FALSE(span1 > span3);

  static double kArray4[] = {2.0, 3.0, 5.0, 7.0, 11.0};
  span<double> span4(kArray4);

  EXPECT_GT(span3, span4);
}

TEST(SpanTest, GreaterEqual) {
  static constexpr int kArray1[] = {2, 3, 5, 7, 11, 13};
  static constexpr int kArray2[] = {2, 3, 5, 7, 11};
  constexpr span<const int> span1(kArray1);
  constexpr span<const int> span2(kArray2);

  EXPECT_GE(span1, span1);
  EXPECT_GE(span1, span2);

  static constexpr int kArray3[] = {2, 3, 5, 7, 12};
  constexpr span<const int> span3(kArray3);

  EXPECT_FALSE(span1 >= span3);

  static double kArray4[] = {2.0, 3.0, 5.0, 7.0, 11.0};
  span<double> span4(kArray4);

  EXPECT_GE(span3, span4);
}

TEST(SpanTest, AsBytes) {
  {
    constexpr int kArray[] = {2, 3, 5, 7, 11, 13};
    span<const uint8_t> bytes_span = as_bytes(make_span(kArray));
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(kArray), bytes_span.data());
    EXPECT_EQ(sizeof(kArray), bytes_span.size());
    EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
  }

  {
    std::vector<int> vec = {1, 1, 2, 3, 5, 8};
    span<int> mutable_span(vec);
    span<const uint8_t> bytes_span = as_bytes(mutable_span);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(vec.data()), bytes_span.data());
    EXPECT_EQ(sizeof(int) * vec.size(), bytes_span.size());
    EXPECT_EQ(bytes_span.size(), bytes_span.size_bytes());
  }
}

TEST(SpanTest, AsWritableBytes) {
  std::vector<int> vec = {1, 1, 2, 3, 5, 8};
  span<int> mutable_span(vec);
  span<uint8_t> writable_bytes_span = as_writable_bytes(mutable_span);
  EXPECT_EQ(reinterpret_cast<uint8_t*>(vec.data()), writable_bytes_span.data());
  EXPECT_EQ(sizeof(int) * vec.size(), writable_bytes_span.size());
  EXPECT_EQ(writable_bytes_span.size(), writable_bytes_span.size_bytes());

  // Set the first entry of vec to zero while writing through the span.
  std::fill(writable_bytes_span.data(),
            writable_bytes_span.data() + sizeof(int), 0);
  EXPECT_EQ(0, vec[0]);
}

TEST(SpanTest, MakeSpanFromDataAndSize) {
  int* nullint = nullptr;
  auto empty_span = make_span(nullint, 0);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};
  span<int> span(vector.data(), vector.size());
  EXPECT_EQ(span, make_span(vector.data(), vector.size()));
}

TEST(SpanTest, MakeSpanFromPointerPair) {
  int* nullint = nullptr;
  auto empty_span = make_span(nullint, nullint);
  EXPECT_TRUE(empty_span.empty());
  EXPECT_EQ(nullptr, empty_span.data());

  std::vector<int> vector = {1, 1, 2, 3, 5, 8};
  span<int> span(vector.data(), vector.size());
  EXPECT_EQ(span, make_span(vector.data(), vector.data() + vector.size()));
}

TEST(SpanTest, MakeSpanFromConstexprArray) {
  static constexpr int kArray[] = {1, 2, 3, 4, 5};
  constexpr span<const int> span(kArray);
  EXPECT_EQ(span, make_span(kArray));
}

TEST(SpanTest, MakeSpanFromStdArray) {
  const std::array<int, 5> kArray = {1, 2, 3, 4, 5};
  span<const int> span(kArray);
  EXPECT_EQ(span, make_span(kArray));
}

TEST(SpanTest, MakeSpanFromConstContainer) {
  const std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<const int> span(vector);
  EXPECT_EQ(span, make_span(vector));
}

TEST(SpanTest, MakeSpanFromContainer) {
  std::vector<int> vector = {-1, -2, -3, -4, -5};
  span<int> span(vector);
  EXPECT_EQ(span, make_span(vector));
}

TEST(SpanTest, EnsureConstexprGoodness) {
  static constexpr int kArray[] = {5, 4, 3, 2, 1};
  constexpr span<const int> constexpr_span(kArray);
  const size_t size = 2;

  const size_t start = 1;
  constexpr span<const int> subspan =
      constexpr_span.subspan(start, start + size);
  for (size_t i = 0; i < subspan.size(); ++i)
    EXPECT_EQ(kArray[start + i], subspan[i]);

  constexpr span<const int> firsts = constexpr_span.first(size);
  for (size_t i = 0; i < firsts.size(); ++i)
    EXPECT_EQ(kArray[i], firsts[i]);

  constexpr span<const int> lasts = constexpr_span.last(size);
  for (size_t i = 0; i < lasts.size(); ++i) {
    const size_t j = (arraysize(kArray) - size) + i;
    EXPECT_EQ(kArray[j], lasts[i]);
  }

  constexpr int item = constexpr_span[size];
  EXPECT_EQ(kArray[size], item);
}

}  // namespace base
