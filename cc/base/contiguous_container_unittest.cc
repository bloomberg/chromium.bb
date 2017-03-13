// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <iterator>

#include "cc/base/contiguous_container.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

struct Point2D {
  Point2D() : Point2D(0, 0) {}
  Point2D(int x, int y) : x(x), y(y) {}
  int x, y;
};

struct Point3D : public Point2D {
  Point3D() : Point3D(0, 0, 0) {}
  Point3D(int x, int y, int z) : Point2D(x, y), z(z) {}
  int z;
};

// Maximum size of a subclass of Point2D.
static const size_t kMaxPointSize = sizeof(Point3D);

// Alignment for Point2D and its subclasses.
static const size_t kPointAlignment = sizeof(int);

// How many elements to use for tests with "plenty" of elements.
static const size_t kNumElements = 150;

TEST(ContiguousContainerTest, SimpleStructs) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  list.AllocateAndConstruct<Point2D>(1, 2);
  list.AllocateAndConstruct<Point3D>(3, 4, 5);
  list.AllocateAndConstruct<Point2D>(6, 7);

  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(1, list[0].x);
  EXPECT_EQ(2, list[0].y);
  EXPECT_EQ(3, list[1].x);
  EXPECT_EQ(4, list[1].y);
  EXPECT_EQ(5, static_cast<Point3D&>(list[1]).z);
  EXPECT_EQ(6, list[2].x);
  EXPECT_EQ(7, list[2].y);
}

TEST(ContiguousContainerTest, AllocateLots) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  for (int i = 0; i < (int)kNumElements; i++)
    list.AllocateAndConstruct<Point2D>(i, i);
  ASSERT_EQ(kNumElements, list.size());
  for (int i = 0; i < (int)kNumElements; i++) {
    ASSERT_EQ(i, list[i].x);
    ASSERT_EQ(i, list[i].y);
  }
}

class MockDestructible {
 public:
  ~MockDestructible() { Destruct(); }
  MOCK_METHOD0(Destruct, void());
};

TEST(ContiguousContainerTest, DestructorCalled) {
  ContiguousContainer<MockDestructible> list(sizeof(MockDestructible));
  auto& destructible = list.AllocateAndConstruct<MockDestructible>();
  EXPECT_EQ(&destructible, &list.first());
  EXPECT_CALL(destructible, Destruct());
}

TEST(ContiguousContainerTest, InsertionAndIndexedAccess) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);

  auto& point1 = list.AllocateAndConstruct<Point2D>();
  auto& point2 = list.AllocateAndConstruct<Point2D>();
  auto& point3 = list.AllocateAndConstruct<Point2D>();

  EXPECT_EQ(3u, list.size());
  EXPECT_EQ(&point1, &list.first());
  EXPECT_EQ(&point3, &list.last());
  EXPECT_EQ(&point1, &list[0]);
  EXPECT_EQ(&point2, &list[1]);
  EXPECT_EQ(&point3, &list[2]);
}

TEST(ContiguousContainerTest, ElementAddressesAreStable) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  std::vector<Point2D*> pointers;
  for (int i = 0; i < (int)kNumElements; i++)
    pointers.push_back(&list.AllocateAndConstruct<Point2D>());
  EXPECT_EQ(kNumElements, list.size());
  EXPECT_EQ(kNumElements, pointers.size());

  auto listIt = list.begin();
  auto vectorIt = pointers.begin();
  for (; listIt != list.end(); ++listIt, ++vectorIt)
    EXPECT_EQ(&*listIt, *vectorIt);
}

TEST(ContiguousContainerTest, ForwardIteration) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  for (int i = 0; i < (int)kNumElements; i++)
    list.AllocateAndConstruct<Point2D>(i, i);
  unsigned count = 0;
  for (Point2D& point : list) {
    EXPECT_EQ((int)count, point.x);
    count++;
  }
  EXPECT_EQ(kNumElements, count);

  static_assert(std::is_same<decltype(*list.begin()), Point2D&>::value,
                "Non-const iteration should produce non-const references.");
}

TEST(ContiguousContainerTest, ForwardIterationEmpty) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);

  // ContiguousContainer allocates memory for elements lazily at first append
  // operation, so at this point memory is not allocated. Check that iteration
  // doesn't crash and produces zero elements.
  EXPECT_EQ(0, std::distance(list.begin(), list.end()));
}

TEST(ContiguousContainerTest, ConstForwardIteration) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  for (int i = 0; i < (int)kNumElements; i++)
    list.AllocateAndConstruct<Point2D>(i, i);

  const auto& const_list = list;
  unsigned count = 0;
  for (const Point2D& point : const_list) {
    EXPECT_EQ((int)count, point.x);
    count++;
  }
  EXPECT_EQ(kNumElements, count);

  static_assert(
      std::is_same<decltype(*const_list.begin()), const Point2D&>::value,
      "Const iteration should produce const references.");
}

TEST(ContiguousContainerTest, ReverseIteration) {
  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize);
  for (int i = 0; i < (int)kNumElements; i++)
    list.AllocateAndConstruct<Point2D>(i, i);

  unsigned count = 0;
  for (auto it = list.rbegin(); it != list.rend(); ++it) {
    EXPECT_EQ((int)(kNumElements - 1 - count), it->x);
    count++;
  }
  EXPECT_EQ(kNumElements, count);

  static_assert(std::is_same<decltype(*list.rbegin()), Point2D&>::value,
                "Non-const iteration should produce non-const references.");
}

// Checks that the latter list has pointers to the elements of the former.
template <typename It1, typename It2>
bool EqualPointers(It1 it1, const It1& end1, It2 it2) {
  for (; it1 != end1; ++it1, ++it2) {
    if (&*it1 != *it2)
      return false;
  }
  return true;
}

TEST(ContiguousContainerTest, CapacityInBytes) {
  const int iterations = 500;
  const size_t initial_capacity = 10 * kMaxPointSize;
  const size_t upper_bound_on_min_capacity = initial_capacity;

  // At time of writing, removing elements from the end can cause up to 7x the
  // memory required to be consumed, in the worst case, since we can have up to
  // two trailing inner lists that are empty (for 2*size + 4*size in unused
  // memory, due to the exponential growth strategy).
  // Unfortunately, this captures behaviour of the underlying allocator as
  // well as this container, so we're pretty loose here. This constant may
  // need to be adjusted.
  const size_t max_waste_factor = 8;

  ContiguousContainer<Point2D, kPointAlignment> list(kMaxPointSize,
                                                     initial_capacity);

  // The capacity should grow with the list.
  for (int i = 0; i < iterations; i++) {
    size_t capacity = list.GetCapacityInBytes();
    ASSERT_GE(capacity, list.size() * sizeof(Point2D));
    ASSERT_LE(capacity, std::max(list.size() * sizeof(Point2D),
                                 upper_bound_on_min_capacity) *
                            max_waste_factor);
    list.AllocateAndConstruct<Point2D>();
  }
}

TEST(ContiguousContainerTest, MemoryUsageInBytes) {
  constexpr size_t initial_size1 = 10 * kMaxPointSize;
  ContiguousContainer<Point2D, kPointAlignment> list1(kMaxPointSize,
                                                      initial_size1);

  constexpr size_t initial_size2 = 10000 * kMaxPointSize;
  ContiguousContainer<Point2D, kPointAlignment> list2(kMaxPointSize,
                                                      initial_size2);

  // Memory is allocated lazily, so even though lists were created with
  // different initial_size values, they'll have the same memory usage here.
  size_t memory_usage1 = list1.MemoryUsageInBytes();
  size_t memory_usage2 = list2.MemoryUsageInBytes();
  EXPECT_EQ(memory_usage1, memory_usage2);

  // Trigger memory allocation.
  list1.AllocateAndConstruct<Point2D>();
  list2.AllocateAndConstruct<Point2D>();

  // Same object was created in both lists, but their memory usages grew
  // differently, based on initial_size values lists were created with.
  EXPECT_NE(list1.MemoryUsageInBytes(), list2.MemoryUsageInBytes());
  EXPECT_GE(list1.MemoryUsageInBytes(), memory_usage1 + initial_size1);
  EXPECT_GE(list2.MemoryUsageInBytes(), memory_usage2 + initial_size2);
}

TEST(ContiguousContainerTest, Alignment) {
  const size_t max_align = ALIGNOF(long double);
  ContiguousContainer<Point2D, max_align> list(kMaxPointSize);

  list.AllocateAndConstruct<Point2D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.last()) & (max_align - 1));
  list.AllocateAndConstruct<Point2D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.last()) & (max_align - 1));
  list.AllocateAndConstruct<Point3D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.last()) & (max_align - 1));
  list.AllocateAndConstruct<Point3D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.last()) & (max_align - 1));
  list.AllocateAndConstruct<Point2D>();
  EXPECT_EQ(0u, reinterpret_cast<intptr_t>(&list.last()) & (max_align - 1));
}

}  // namespace
}  // namespace cc
