// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/deque.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

template <typename T>
class DequeAccessor {
 public:
  explicit DequeAccessor(internal::deque<T>* value) : value_(value) {}

  size_t front() { return value_->front_; }

  size_t back() { return value_->back_; }

  std::vector<T>& buffer() { return value_->buffer_; }

 private:
  internal::deque<T>* value_;
};

namespace {

using internal::deque;

template <typename T>
struct ElementTraits {
  static T GetValue(const T& value) { return value; }
  static T CreateElement(const T& value) { return value; }
};

template <typename T>
struct ElementTraits<std::unique_ptr<T>> {
  static T GetValue(const std::unique_ptr<T>& value) { return *value; }
  static std::unique_ptr<T> CreateElement(const T& value) {
    return base::MakeUnique<T>(value);
  }
};

template <typename T>
class DequeTest : public ::testing::Test {};

using CaseList = ::testing::Types<int32_t, std::unique_ptr<int32_t>>;

TYPED_TEST_CASE(DequeTest, CaseList);

TYPED_TEST(DequeTest, PushPop) {
  using Traits = ElementTraits<TypeParam>;

  deque<TypeParam> deque_object(2);
  EXPECT_TRUE(deque_object.empty());

  deque_object.push_front(Traits::CreateElement(123));
  EXPECT_FALSE(deque_object.empty());

  EXPECT_EQ(123, Traits::GetValue(deque_object.front()));
  EXPECT_EQ(123, Traits::GetValue(deque_object.back()));

  deque_object.push_back(Traits::CreateElement(456));

  EXPECT_EQ(123, Traits::GetValue(deque_object.front()));
  EXPECT_EQ(456, Traits::GetValue(deque_object.back()));

  deque_object.push_front(Traits::CreateElement(789));

  EXPECT_EQ(789, Traits::GetValue(deque_object.front()));
  EXPECT_EQ(456, Traits::GetValue(deque_object.back()));

  deque_object.pop_back();
  EXPECT_EQ(789, Traits::GetValue(deque_object.front()));
  EXPECT_EQ(123, Traits::GetValue(deque_object.back()));

  deque_object.pop_front();
  EXPECT_EQ(123, Traits::GetValue(deque_object.front()));
  EXPECT_EQ(123, Traits::GetValue(deque_object.back()));

  deque_object.pop_front();
  EXPECT_TRUE(deque_object.empty());
}

TYPED_TEST(DequeTest, Clear) {
  using Traits = ElementTraits<TypeParam>;

  deque<TypeParam> deque_object;
  EXPECT_TRUE(deque_object.empty());

  deque_object.clear();
  EXPECT_TRUE(deque_object.empty());

  deque_object.push_front(Traits::CreateElement(123));
  deque_object.push_back(Traits::CreateElement(456));
  deque_object.push_front(Traits::CreateElement(789));

  deque_object.clear();

  EXPECT_TRUE(deque_object.empty());
}

TYPED_TEST(DequeTest, Move) {
  using Traits = ElementTraits<TypeParam>;

  {
    deque<TypeParam> deque_object;
    deque_object.push_back(Traits::CreateElement(123));
    deque_object.push_back(Traits::CreateElement(456));
    deque_object.push_back(Traits::CreateElement(789));

    deque<TypeParam> deque_object2(std::move(deque_object));

    EXPECT_TRUE(deque_object.empty());

    EXPECT_EQ(123, Traits::GetValue(deque_object2.front()));
    deque_object2.pop_front();
    EXPECT_EQ(456, Traits::GetValue(deque_object2.front()));
    deque_object2.pop_front();
    EXPECT_EQ(789, Traits::GetValue(deque_object2.front()));

    // |deque_object| is still in valid state and could be reused.
    deque_object.push_back(Traits::CreateElement(321));
    EXPECT_EQ(321, Traits::GetValue(deque_object.front()));
    EXPECT_EQ(321, Traits::GetValue(deque_object.back()));
  }

  {
    deque<TypeParam> deque_object;
    deque_object.push_back(Traits::CreateElement(123));
    deque_object.push_back(Traits::CreateElement(456));
    deque_object.push_back(Traits::CreateElement(789));

    deque<TypeParam> deque_object2;
    deque_object2 = std::move(deque_object);

    EXPECT_TRUE(deque_object.empty());

    EXPECT_EQ(123, Traits::GetValue(deque_object2.front()));
    deque_object2.pop_front();
    EXPECT_EQ(456, Traits::GetValue(deque_object2.front()));
    deque_object2.pop_front();
    EXPECT_EQ(789, Traits::GetValue(deque_object2.front()));

    // |deque_object| is still in valid state and could be reused.
    deque_object.push_back(Traits::CreateElement(321));
    EXPECT_EQ(321, Traits::GetValue(deque_object.front()));
    EXPECT_EQ(321, Traits::GetValue(deque_object.back()));
  }
}

// Constructs a deque with a fully occupied ring buffer.
// |front_index| is the position of the front element in the ring buffer.
// |values| stores the values from front to back.
template <typename T>
void ConstructFullDeque(const std::vector<int32_t>& values,
                        size_t front_index,
                        deque<T>* deque_object) {
  using Traits = ElementTraits<T>;

  DCHECK_LT(front_index, values.size());
  *deque_object = deque<T>(values.size());

  // Push some temp elements to make sure the desired front element starts at
  // |front_index|.
  for (size_t i = 0; i < front_index; ++i)
    deque_object->push_back(Traits::CreateElement(0));

  deque_object->push_back(Traits::CreateElement(values[0]));

  for (size_t i = 0; i < front_index; ++i)
    deque_object->pop_front();

  for (size_t i = 1; i < values.size(); ++i)
    deque_object->push_back(Traits::CreateElement(values[i]));
}

template <typename T>
void VerifyDequeInternalState(deque<T>* deque_object,
                              size_t expected_front,
                              size_t expected_back,
                              const std::vector<int32_t>& expected_values,
                              size_t expected_ring_buffer_size) {
  using Traits = ElementTraits<T>;

  DequeAccessor<T> accessor(deque_object);

  EXPECT_EQ(expected_front, accessor.front());
  EXPECT_EQ(expected_back, accessor.back());
  EXPECT_EQ(expected_ring_buffer_size, accessor.buffer().size());

  size_t index = expected_front;
  for (size_t i = 0; i < expected_values.size(); ++i) {
    EXPECT_EQ(expected_values[i], Traits::GetValue(accessor.buffer()[index]));
    index++;
    if (index == accessor.buffer().size())
      index = 0;
  }
}

TYPED_TEST(DequeTest, ExpandWhiteBox) {
  using Traits = ElementTraits<TypeParam>;

  // Test ConstructFullDeque() and VerifyDequeInternalState().
  {
    deque<TypeParam> deque_object;
    ConstructFullDeque({1, 2, 3}, 0, &deque_object);
    VerifyDequeInternalState(&deque_object, 0, 2, {1, 2, 3}, 3);

    deque<TypeParam> deque_object1;
    ConstructFullDeque({1, 2, 3}, 1, &deque_object1);
    VerifyDequeInternalState(&deque_object1, 1, 0, {1, 2, 3}, 3);
  }

  // Test that ring buffer [1(front), 2, 3(back)] becomes
  // [1(front), 2, 3, 4(back), _, _] after push_back(4).
  {
    deque<TypeParam> deque_object;
    ConstructFullDeque({1, 2, 3}, 0, &deque_object);

    deque_object.push_back(Traits::CreateElement(4));
    VerifyDequeInternalState(&deque_object, 0, 3, {1, 2, 3, 4}, 6);
  }

  // Test that ring buffer [1(front), 2, 3(back)] becomes
  // [1, 2, 3(back), _, _, 4(front)] after push_front(4).
  {
    deque<TypeParam> deque_object;
    ConstructFullDeque({1, 2, 3}, 0, &deque_object);

    deque_object.push_front(Traits::CreateElement(4));
    VerifyDequeInternalState(&deque_object, 5, 2, {4, 1, 2, 3}, 6);
  }

  // Test that ring buffer [3(back), 1(front), 2] becomes
  // [_, 1(front), 2, 3, 4(back), _] after push_back(4).
  {
    deque<TypeParam> deque_object;
    ConstructFullDeque({1, 2, 3}, 1, &deque_object);

    deque_object.push_back(Traits::CreateElement(4));
    VerifyDequeInternalState(&deque_object, 1, 4, {1, 2, 3, 4}, 6);
  }

  // Test that ring buffer [2, 3(back), 1(front)] becomes
  // [2, 3, 4(back), _, _, 1(front)] after push_back(4).
  {
    deque<TypeParam> deque_object;
    ConstructFullDeque({1, 2, 3}, 2, &deque_object);

    deque_object.push_back(Traits::CreateElement(4));
    VerifyDequeInternalState(&deque_object, 5, 2, {1, 2, 3, 4}, 6);
  }
}

}  // namespace
}  // namespace test
}  // namespace mojo
