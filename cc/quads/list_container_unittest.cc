// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/list_container.h"

#include <vector>
#include "cc/quads/draw_quad.h"
#include "cc/quads/largest_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(jbroman): This unit test should be made generic and moved to cc/base/.

namespace cc {
namespace {

int kMagicNumberToUseForDrawQuadOne = 42;
int kMagicNumberToUseForDrawQuadTwo = 314;

bool isConstSharedQuadStatePointer(const SharedQuadState* ptr) {
  return true;
}

bool isConstSharedQuadStatePointer(SharedQuadState* ptr) {
  return false;
}

class SimpleDrawQuad : public DrawQuad {
 public:
  ~SimpleDrawQuad() override {}

  void set_value(int val) { value = val; }
  int get_value() { return value; }
  void ExtendValue(base::trace_event::TracedValue* value) const override {}

 private:
  int value;
};

class SimpleDrawQuadConstructMagicNumberOne : public SimpleDrawQuad {
 public:
  SimpleDrawQuadConstructMagicNumberOne() : SimpleDrawQuad() {
    set_value(kMagicNumberToUseForDrawQuadOne);
  }
};

class SimpleDrawQuadConstructMagicNumberTwo : public SimpleDrawQuad {
 public:
  SimpleDrawQuadConstructMagicNumberTwo() : SimpleDrawQuad() {
    set_value(kMagicNumberToUseForDrawQuadTwo);
  }
};

class MockDrawQuad : public SimpleDrawQuadConstructMagicNumberOne {
 public:
  ~MockDrawQuad() override { Destruct(); }
  MOCK_METHOD0(Destruct, void());
};

class MockDrawQuadSubclass : public MockDrawQuad {
 public:
  MockDrawQuadSubclass() { set_value(kMagicNumberToUseForDrawQuadTwo); }
};

const size_t kLargestQuadSize =
    std::max(LargestDrawQuadSize(), sizeof(MockDrawQuadSubclass));

TEST(ListContainerTest, ConstructorCalledInAllocateAndConstruct) {
  ListContainer<DrawQuad> list(kLargestQuadSize);

  size_t size = 2;
  SimpleDrawQuadConstructMagicNumberOne* dq_1 =
      list.AllocateAndConstruct<SimpleDrawQuadConstructMagicNumberOne>();
  SimpleDrawQuadConstructMagicNumberTwo* dq_2 =
      list.AllocateAndConstruct<SimpleDrawQuadConstructMagicNumberTwo>();

  EXPECT_EQ(size, list.size());
  EXPECT_EQ(dq_1, list.front());
  EXPECT_EQ(dq_2, list.back());

  EXPECT_EQ(kMagicNumberToUseForDrawQuadOne, dq_1->get_value());
  EXPECT_EQ(kMagicNumberToUseForDrawQuadTwo, dq_2->get_value());
}

TEST(ListContainerTest, DestructorCalled) {
  ListContainer<DrawQuad> list(kLargestQuadSize);

  size_t size = 1;
  MockDrawQuad* dq_1 = list.AllocateAndConstruct<MockDrawQuad>();

  EXPECT_CALL(*dq_1, Destruct());
  EXPECT_EQ(size, list.size());
  EXPECT_EQ(dq_1, list.front());
}

TEST(ListContainerTest, DestructorCalledOnceWhenClear) {
  ListContainer<DrawQuad> list(kLargestQuadSize);
  size_t size = 1;
  MockDrawQuad* dq_1 = list.AllocateAndConstruct<MockDrawQuad>();

  EXPECT_EQ(size, list.size());
  EXPECT_EQ(dq_1, list.front());

  // Make sure destructor is called once during clear, and won't be called
  // again.
  testing::MockFunction<void()> separator;
  {
    testing::InSequence s;
    EXPECT_CALL(*dq_1, Destruct());
    EXPECT_CALL(separator, Call());
    EXPECT_CALL(*dq_1, Destruct()).Times(0);
  }

  list.clear();
  separator.Call();
}

TEST(ListContainerTest, ReplaceExistingElement) {
  ListContainer<DrawQuad> list(kLargestQuadSize);
  size_t size = 1;
  MockDrawQuad* dq_1 = list.AllocateAndConstruct<MockDrawQuad>();

  EXPECT_EQ(size, list.size());
  EXPECT_EQ(dq_1, list.front());

  // Make sure destructor is called once during clear, and won't be called
  // again.
  testing::MockFunction<void()> separator;
  {
    testing::InSequence s;
    EXPECT_CALL(*dq_1, Destruct());
    EXPECT_CALL(separator, Call());
    EXPECT_CALL(*dq_1, Destruct()).Times(0);
  }

  list.ReplaceExistingElement<MockDrawQuadSubclass>(list.begin());
  EXPECT_EQ(kMagicNumberToUseForDrawQuadTwo, dq_1->get_value());
  separator.Call();

  EXPECT_CALL(*dq_1, Destruct());
  list.clear();
}

TEST(ListContainerTest, DestructorCalledOnceWhenErase) {
  ListContainer<DrawQuad> list(kLargestQuadSize);
  size_t size = 1;
  MockDrawQuad* dq_1 = list.AllocateAndConstruct<MockDrawQuad>();

  EXPECT_EQ(size, list.size());
  EXPECT_EQ(dq_1, list.front());

  // Make sure destructor is called once during clear, and won't be called
  // again.
  testing::MockFunction<void()> separator;
  {
    testing::InSequence s;
    EXPECT_CALL(*dq_1, Destruct());
    EXPECT_CALL(separator, Call());
    EXPECT_CALL(*dq_1, Destruct()).Times(0);
  }

  list.EraseAndInvalidateAllPointers(list.begin());
  separator.Call();
}

TEST(ListContainerTest, SimpleIndexAccessSharedQuadState) {
  ListContainer<SharedQuadState> list;

  size_t size = 3;
  SharedQuadState* sqs_1 = list.AllocateAndConstruct<SharedQuadState>();
  SharedQuadState* sqs_2 = list.AllocateAndConstruct<SharedQuadState>();
  SharedQuadState* sqs_3 = list.AllocateAndConstruct<SharedQuadState>();

  EXPECT_EQ(size, list.size());
  EXPECT_EQ(sqs_1, list.front());
  EXPECT_EQ(sqs_3, list.back());
  EXPECT_EQ(list.front(), list.ElementAt(0));
  EXPECT_EQ(sqs_2, list.ElementAt(1));
  EXPECT_EQ(list.back(), list.ElementAt(2));
}

TEST(ListContainerTest, SimpleInsertionSharedQuadState) {
  ListContainer<SharedQuadState> list;

  size_t size = 3;
  SharedQuadState* sqs_1 = list.AllocateAndConstruct<SharedQuadState>();
  list.AllocateAndConstruct<SharedQuadState>();
  SharedQuadState* sqs_3 = list.AllocateAndConstruct<SharedQuadState>();

  EXPECT_EQ(size, list.size());
  EXPECT_EQ(sqs_1, list.front());
  EXPECT_EQ(sqs_3, list.back());
}

TEST(ListContainerTest, SimpleInsertionAndClearSharedQuadState) {
  ListContainer<SharedQuadState> list;
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(0u, list.size());

  size_t size = 3;
  SharedQuadState* sqs_1 = list.AllocateAndConstruct<SharedQuadState>();
  list.AllocateAndConstruct<SharedQuadState>();
  SharedQuadState* sqs_3 = list.AllocateAndConstruct<SharedQuadState>();

  EXPECT_EQ(size, list.size());
  EXPECT_EQ(sqs_1, list.front());
  EXPECT_EQ(sqs_3, list.back());
  EXPECT_FALSE(list.empty());

  list.clear();
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(0u, list.size());
}

TEST(ListContainerTest, SimpleInsertionClearAndInsertAgainSharedQuadState) {
  ListContainer<SharedQuadState> list;
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(0u, list.size());

  size_t size = 2;
  SharedQuadState* sqs_front = list.AllocateAndConstruct<SharedQuadState>();
  SharedQuadState* sqs_back = list.AllocateAndConstruct<SharedQuadState>();

  EXPECT_EQ(size, list.size());
  EXPECT_EQ(sqs_front, list.front());
  EXPECT_EQ(sqs_back, list.back());
  EXPECT_FALSE(list.empty());

  list.clear();
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(0u, list.size());

  size = 3;
  sqs_front = list.AllocateAndConstruct<SharedQuadState>();
  list.AllocateAndConstruct<SharedQuadState>();
  sqs_back = list.AllocateAndConstruct<SharedQuadState>();

  EXPECT_EQ(size, list.size());
  EXPECT_EQ(sqs_front, list.front());
  EXPECT_EQ(sqs_back, list.back());
  EXPECT_FALSE(list.empty());
}

// This test is used to test when there is more than one allocation needed
// for, ListContainer can still perform like normal vector.
TEST(ListContainerTest,
     SimpleInsertionTriggerMoreThanOneAllocationSharedQuadState) {
  ListContainer<SharedQuadState> list(sizeof(SharedQuadState), 2);
  std::vector<SharedQuadState*> sqs_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    sqs_list.push_back(list.AllocateAndConstruct<SharedQuadState>());
  }
  EXPECT_EQ(size, list.size());

  ListContainer<SharedQuadState>::Iterator iter = list.begin();
  for (std::vector<SharedQuadState*>::const_iterator sqs_iter =
           sqs_list.begin();
       sqs_iter != sqs_list.end(); ++sqs_iter) {
    EXPECT_EQ(*sqs_iter, *iter);
    ++iter;
  }
}

TEST(ListContainerTest,
     CorrectAllocationSizeForMoreThanOneAllocationSharedQuadState) {
  // Constructor sets the allocation size to 2. Every time ListContainer needs
  // to allocate again, it doubles allocation size. In this test, 10 elements is
  // needed, thus ListContainerShould allocate spaces 2, 4 and 8 elements.
  ListContainer<SharedQuadState> list(sizeof(SharedQuadState), 2);
  std::vector<SharedQuadState*> sqs_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    // Before asking for a new element, space available without another
    // allocation follows.
    switch (i) {
      case 2:
      case 6:
        EXPECT_EQ(0u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 1:
      case 5:
        EXPECT_EQ(1u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 0:
      case 4:
        EXPECT_EQ(2u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 3:
        EXPECT_EQ(3u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 9:
        EXPECT_EQ(5u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 8:
        EXPECT_EQ(6u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 7:
        EXPECT_EQ(7u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      default:
        break;
    }
    sqs_list.push_back(list.AllocateAndConstruct<SharedQuadState>());
    // After asking for a new element, space available without another
    // allocation follows.
    switch (i) {
      case 1:
      case 5:
        EXPECT_EQ(0u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 0:
      case 4:
        EXPECT_EQ(1u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 3:
        EXPECT_EQ(2u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 2:
        EXPECT_EQ(3u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 9:
        EXPECT_EQ(4u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 8:
        EXPECT_EQ(5u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 7:
        EXPECT_EQ(6u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      case 6:
        EXPECT_EQ(7u, list.AvailableSizeWithoutAnotherAllocationForTesting());
        break;
      default:
        break;
    }
  }
  EXPECT_EQ(size, list.size());

  ListContainer<SharedQuadState>::Iterator iter = list.begin();
  for (std::vector<SharedQuadState*>::const_iterator sqs_iter =
           sqs_list.begin();
       sqs_iter != sqs_list.end(); ++sqs_iter) {
    EXPECT_EQ(*sqs_iter, *iter);
    ++iter;
  }
}

TEST(ListContainerTest, SimpleIterationSharedQuadState) {
  ListContainer<SharedQuadState> list;
  std::vector<SharedQuadState*> sqs_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    sqs_list.push_back(list.AllocateAndConstruct<SharedQuadState>());
  }
  EXPECT_EQ(size, list.size());

  size_t num_iters_in_list = 0;
  {
    std::vector<SharedQuadState*>::const_iterator sqs_iter = sqs_list.begin();
    for (ListContainer<SharedQuadState>::Iterator iter = list.begin();
         iter != list.end(); ++iter) {
      EXPECT_EQ(*sqs_iter, *iter);
      ++num_iters_in_list;
      ++sqs_iter;
    }
  }

  size_t num_iters_in_vector = 0;
  {
    ListContainer<SharedQuadState>::Iterator iter = list.begin();
    for (std::vector<SharedQuadState*>::const_iterator sqs_iter =
             sqs_list.begin();
         sqs_iter != sqs_list.end(); ++sqs_iter) {
      EXPECT_EQ(*sqs_iter, *iter);
      ++num_iters_in_vector;
      ++iter;
    }
  }

  EXPECT_EQ(num_iters_in_vector, num_iters_in_list);
}

TEST(ListContainerTest, SimpleConstIteratorIterationSharedQuadState) {
  ListContainer<SharedQuadState> list;
  std::vector<const SharedQuadState*> sqs_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    sqs_list.push_back(list.AllocateAndConstruct<SharedQuadState>());
  }
  EXPECT_EQ(size, list.size());

  {
    std::vector<const SharedQuadState*>::const_iterator sqs_iter =
        sqs_list.begin();
    for (ListContainer<SharedQuadState>::ConstIterator iter = list.begin();
         iter != list.end(); ++iter) {
      EXPECT_TRUE(isConstSharedQuadStatePointer(*iter));
      EXPECT_EQ(*sqs_iter, *iter);
      ++sqs_iter;
    }
  }

  {
    std::vector<const SharedQuadState*>::const_iterator sqs_iter =
        sqs_list.begin();
    for (ListContainer<SharedQuadState>::Iterator iter = list.begin();
         iter != list.end(); ++iter) {
      EXPECT_FALSE(isConstSharedQuadStatePointer(*iter));
      EXPECT_EQ(*sqs_iter, *iter);
      ++sqs_iter;
    }
  }

  {
    ListContainer<SharedQuadState>::ConstIterator iter = list.begin();
    for (std::vector<const SharedQuadState*>::const_iterator sqs_iter =
             sqs_list.begin();
         sqs_iter != sqs_list.end(); ++sqs_iter) {
      EXPECT_EQ(*sqs_iter, *iter);
      ++iter;
    }
  }
}

TEST(ListContainerTest, SimpleReverseInsertionSharedQuadState) {
  ListContainer<SharedQuadState> list;
  std::vector<SharedQuadState*> sqs_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    sqs_list.push_back(list.AllocateAndConstruct<SharedQuadState>());
  }
  EXPECT_EQ(size, list.size());

  {
    std::vector<SharedQuadState*>::const_reverse_iterator sqs_iter =
        sqs_list.rbegin();
    for (ListContainer<SharedQuadState>::ReverseIterator iter = list.rbegin();
         iter != list.rend(); ++iter) {
      EXPECT_EQ(*sqs_iter, *iter);
      ++sqs_iter;
    }
  }

  {
    ListContainer<SharedQuadState>::ReverseIterator iter = list.rbegin();
    for (std::vector<SharedQuadState*>::reverse_iterator sqs_iter =
             sqs_list.rbegin();
         sqs_iter != sqs_list.rend(); ++sqs_iter) {
      EXPECT_EQ(*sqs_iter, *iter);
      ++iter;
    }
  }
}

TEST(ListContainerTest, SimpleDeletion) {
  ListContainer<DrawQuad> list(kLargestQuadSize);
  std::vector<SimpleDrawQuad*> sdq_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    sdq_list.push_back(list.AllocateAndConstruct<SimpleDrawQuad>());
    sdq_list.back()->set_value(i);
  }
  EXPECT_EQ(size, list.size());

  list.EraseAndInvalidateAllPointers(list.begin());
  --size;
  EXPECT_EQ(size, list.size());
  int i = 1;
  for (ListContainer<DrawQuad>::Iterator iter = list.begin();
       iter != list.end(); ++iter) {
    EXPECT_EQ(i, static_cast<SimpleDrawQuad*>(*iter)->get_value());
    ++i;
  }
}

TEST(ListContainerTest, DeletionAllInAllocation) {
  const size_t kReserve = 10;
  ListContainer<DrawQuad> list(kLargestQuadSize, kReserve);
  std::vector<SimpleDrawQuad*> sdq_list;
  // Add enough quads to cause another allocation.
  for (size_t i = 0; i < kReserve + 1; ++i) {
    sdq_list.push_back(list.AllocateAndConstruct<SimpleDrawQuad>());
    sdq_list.back()->set_value(static_cast<int>(i));
  }
  EXPECT_EQ(kReserve + 1, list.size());

  // Remove everything in the first allocation.
  for (size_t i = 0; i < kReserve; ++i)
    list.EraseAndInvalidateAllPointers(list.begin());
  EXPECT_EQ(1u, list.size());

  // The last quad is left.
  SimpleDrawQuad* quad = static_cast<SimpleDrawQuad*>(*list.begin());
  EXPECT_EQ(static_cast<int>(kReserve), quad->get_value());

  // Remove the quad from the 2nd allocation.
  list.EraseAndInvalidateAllPointers(list.begin());
  EXPECT_EQ(0u, list.size());
}

TEST(ListContainerTest, DeletionAllInAllocationReversed) {
  const size_t kReserve = 10;
  ListContainer<DrawQuad> list(kLargestQuadSize, kReserve);
  std::vector<SimpleDrawQuad*> sdq_list;
  // Add enough quads to cause another allocation.
  for (size_t i = 0; i < kReserve + 1; ++i) {
    sdq_list.push_back(list.AllocateAndConstruct<SimpleDrawQuad>());
    sdq_list.back()->set_value(static_cast<int>(i));
  }
  EXPECT_EQ(kReserve + 1, list.size());

  // Remove everything in the 2nd allocation.
  auto it = list.begin();
  for (size_t i = 0; i < kReserve; ++i)
    ++it;
  list.EraseAndInvalidateAllPointers(it);

  // The 2nd-last quad is next, and the rest of the quads exist.
  size_t i = kReserve - 1;
  for (auto it = list.rbegin(); it != list.rend(); ++it) {
    SimpleDrawQuad* quad = static_cast<SimpleDrawQuad*>(*it);
    EXPECT_EQ(static_cast<int>(i), quad->get_value());
    --i;
  }

  // Can forward iterate too.
  i = 0;
  for (auto it = list.begin(); it != list.end(); ++it) {
    SimpleDrawQuad* quad = static_cast<SimpleDrawQuad*>(*it);
    EXPECT_EQ(static_cast<int>(i), quad->get_value());
    ++i;
  }

  // Remove the last thing from the 1st allocation.
  it = list.begin();
  for (size_t i = 0; i < kReserve - 1; ++i)
    ++it;
  list.EraseAndInvalidateAllPointers(it);

  // The 2nd-last quad is next, and the rest of the quads exist.
  i = kReserve - 2;
  for (auto it = list.rbegin(); it != list.rend(); ++it) {
    SimpleDrawQuad* quad = static_cast<SimpleDrawQuad*>(*it);
    EXPECT_EQ(static_cast<int>(i), quad->get_value());
    --i;
  }

  // Can forward iterate too.
  i = 0;
  for (auto it = list.begin(); it != list.end(); ++it) {
    SimpleDrawQuad* quad = static_cast<SimpleDrawQuad*>(*it);
    EXPECT_EQ(static_cast<int>(i), quad->get_value());
    ++i;
  }
}

TEST(ListContainerTest, SimpleIterationAndManipulation) {
  ListContainer<DrawQuad> list(kLargestQuadSize);
  std::vector<SimpleDrawQuad*> sdq_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    SimpleDrawQuad* simple_dq = list.AllocateAndConstruct<SimpleDrawQuad>();
    sdq_list.push_back(simple_dq);
  }
  EXPECT_EQ(size, list.size());

  ListContainer<DrawQuad>::Iterator iter = list.begin();
  for (int i = 0; i < 10; ++i) {
    static_cast<SimpleDrawQuad*>(*iter)->set_value(i);
    ++iter;
  }

  int i = 0;
  for (std::vector<SimpleDrawQuad*>::const_iterator sdq_iter = sdq_list.begin();
       sdq_iter < sdq_list.end(); ++sdq_iter) {
    EXPECT_EQ(i, (*sdq_iter)->get_value());
    ++i;
  }
}

TEST(ListContainerTest, SimpleManipulationWithIndexSimpleDrawQuad) {
  ListContainer<DrawQuad> list(kLargestQuadSize);
  std::vector<SimpleDrawQuad*> dq_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    dq_list.push_back(list.AllocateAndConstruct<SimpleDrawQuad>());
  }
  EXPECT_EQ(size, list.size());

  for (size_t i = 0; i < size; ++i) {
    static_cast<SimpleDrawQuad*>(list.ElementAt(i))->set_value(i);
  }

  int i = 0;
  for (std::vector<SimpleDrawQuad*>::const_iterator dq_iter = dq_list.begin();
       dq_iter != dq_list.end(); ++dq_iter, ++i) {
    EXPECT_EQ(i, (*dq_iter)->get_value());
  }
}

TEST(ListContainerTest,
     SimpleManipulationWithIndexMoreThanOneAllocationSimpleDrawQuad) {
  ListContainer<DrawQuad> list(LargestDrawQuadSize(), 2);
  std::vector<SimpleDrawQuad*> dq_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    dq_list.push_back(list.AllocateAndConstruct<SimpleDrawQuad>());
  }
  EXPECT_EQ(size, list.size());

  for (size_t i = 0; i < size; ++i) {
    static_cast<SimpleDrawQuad*>(list.ElementAt(i))->set_value(i);
  }

  int i = 0;
  for (std::vector<SimpleDrawQuad*>::const_iterator dq_iter = dq_list.begin();
       dq_iter != dq_list.end(); ++dq_iter, ++i) {
    EXPECT_EQ(i, (*dq_iter)->get_value());
  }
}

TEST(ListContainerTest,
     SimpleIterationAndReverseIterationWithIndexSharedQuadState) {
  ListContainer<SharedQuadState> list;
  std::vector<SharedQuadState*> sqs_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    sqs_list.push_back(list.AllocateAndConstruct<SharedQuadState>());
  }
  EXPECT_EQ(size, list.size());

  size_t i = 0;
  for (ListContainer<SharedQuadState>::Iterator iter = list.begin();
       iter != list.end(); ++iter) {
    EXPECT_EQ(i, iter.index());
    ++i;
  }

  i = 0;
  for (ListContainer<SharedQuadState>::ReverseIterator iter = list.rbegin();
       iter != list.rend(); ++iter) {
    EXPECT_EQ(i, iter.index());
    ++i;
  }
}

// Increments an int when constructed (or the counter pointer is supplied) and
// decrements when destructed.
class InstanceCounter {
 public:
  InstanceCounter() : counter_(nullptr) {}
  explicit InstanceCounter(int* counter) { SetCounter(counter); }
  ~InstanceCounter() {
    if (counter_)
      --*counter_;
  }
  void SetCounter(int* counter) {
    counter_ = counter;
    ++*counter_;
  }

 private:
  int* counter_;
};

TEST(ListContainerTest, RemoveLastDestruction) {
  // We keep an explicit instance count to make sure that the destructors are
  // indeed getting called.
  int counter = 0;
  ListContainer<InstanceCounter> list(sizeof(InstanceCounter), 1);
  EXPECT_EQ(0, counter);
  EXPECT_EQ(0u, list.size());

  // We should be okay to add one and then go back to zero.
  list.AllocateAndConstruct<InstanceCounter>()->SetCounter(&counter);
  EXPECT_EQ(1, counter);
  EXPECT_EQ(1u, list.size());
  list.RemoveLast();
  EXPECT_EQ(0, counter);
  EXPECT_EQ(0u, list.size());

  // We should also be okay to remove the last multiple times, as long as there
  // are enough elements in the first place.
  list.AllocateAndConstruct<InstanceCounter>()->SetCounter(&counter);
  list.AllocateAndConstruct<InstanceCounter>()->SetCounter(&counter);
  list.AllocateAndConstruct<InstanceCounter>()->SetCounter(&counter);
  list.AllocateAndConstruct<InstanceCounter>()->SetCounter(&counter);
  list.AllocateAndConstruct<InstanceCounter>()->SetCounter(&counter);
  list.AllocateAndConstruct<InstanceCounter>()->SetCounter(&counter);
  list.RemoveLast();
  list.RemoveLast();
  EXPECT_EQ(4, counter);  // Leaves one in the last list.
  EXPECT_EQ(4u, list.size());
  list.RemoveLast();
  EXPECT_EQ(3, counter);  // Removes an inner list from before.
  EXPECT_EQ(3u, list.size());
}

// TODO(jbroman): std::equal would work if ListContainer iterators satisfied the
// usual STL iterator constraints. We should fix that.
template <typename It1, typename It2>
bool Equal(It1 it1, const It1& end1, It2 it2) {
  for (; it1 != end1; ++it1, ++it2) {
    if (!(*it1 == *it2))
      return false;
  }
  return true;
}

TEST(ListContainerTest, RemoveLastIteration) {
  struct SmallStruct {
    char dummy[16];
  };
  ListContainer<SmallStruct> list(sizeof(SmallStruct), 1);
  std::vector<SmallStruct*> pointers;

  // Utilities which keep these two lists in sync and check that their iteration
  // order matches.
  auto push = [&list, &pointers]() {
    pointers.push_back(list.AllocateAndConstruct<SmallStruct>());
  };
  auto pop = [&list, &pointers]() {
    pointers.pop_back();
    list.RemoveLast();
  };
  auto check_equal = [&list, &pointers]() {
    // They should be of the same size, and compare equal with all four kinds of
    // iteration.
    // Apparently Mac doesn't have vector::cbegin and vector::crbegin?
    const auto& const_pointers = pointers;
    ASSERT_EQ(list.size(), pointers.size());
    ASSERT_TRUE(Equal(list.begin(), list.end(), pointers.begin()));
    ASSERT_TRUE(Equal(list.cbegin(), list.cend(), const_pointers.begin()));
    ASSERT_TRUE(Equal(list.rbegin(), list.rend(), pointers.rbegin()));
    ASSERT_TRUE(Equal(list.crbegin(), list.crend(), const_pointers.rbegin()));
  };

  check_equal();  // Initially empty.
  push();
  check_equal();  // One full inner list.
  push();
  check_equal();  // One full, one partially full.
  push();
  push();
  check_equal();  // Two full, one partially full.
  pop();
  check_equal();  // Two full, one empty.
  pop();
  check_equal();  // One full, one partially full, one empty.
  pop();
  check_equal();  // One full, one empty.
  push();
  pop();
  pop();
  ASSERT_TRUE(list.empty());
  check_equal();  // Empty.
}

}  // namespace
}  // namespace cc
