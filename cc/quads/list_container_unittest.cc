// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/list_container.h"

#include <vector>
#include "cc/quads/draw_quad.h"
#include "cc/quads/largest_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  virtual ~SimpleDrawQuad() {}
  virtual void IterateResources(
      const ResourceIteratorCallback& callback) OVERRIDE {}

  void set_value(int val) { value = val; }
  int get_value() { return value; }
  virtual void ExtendValue(base::debug::TracedValue* value) const OVERRIDE {}

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

class MockDrawQuad : public DrawQuad {
 public:
  virtual ~MockDrawQuad() { Destruct(); }
  virtual void IterateResources(
      const ResourceIteratorCallback& callback) OVERRIDE {}
  virtual void ExtendValue(base::debug::TracedValue* value) const OVERRIDE {}
  MOCK_METHOD0(Destruct, void());
};

TEST(ListContainerTest, ConstructorCalledInAllocateAndConstruct) {
  ListContainer<DrawQuad> list(sizeof(kLargestDrawQuad));

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
  ListContainer<DrawQuad> list(sizeof(kLargestDrawQuad));

  size_t size = 1;
  MockDrawQuad* dq_1 = list.AllocateAndConstruct<MockDrawQuad>();

  EXPECT_CALL(*dq_1, Destruct());
  EXPECT_EQ(size, list.size());
  EXPECT_EQ(dq_1, list.front());
}

TEST(ListContainerTest, DestructorCalledOnceWhenClear) {
  ListContainer<DrawQuad> list(sizeof(kLargestDrawQuad));
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

TEST(ListContainerTest, DestructorCalledOnceWhenErase) {
  ListContainer<DrawQuad> list(sizeof(kLargestDrawQuad));
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
       sqs_iter != sqs_list.end();
       ++sqs_iter) {
    EXPECT_EQ(*sqs_iter, &*iter);
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
       sqs_iter != sqs_list.end();
       ++sqs_iter) {
    EXPECT_EQ(*sqs_iter, &*iter);
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
         iter != list.end();
         ++iter) {
      EXPECT_EQ(*sqs_iter, &*iter);
      ++num_iters_in_list;
      ++sqs_iter;
    }
  }

  size_t num_iters_in_vector = 0;
  {
    ListContainer<SharedQuadState>::Iterator iter = list.begin();
    for (std::vector<SharedQuadState*>::const_iterator sqs_iter =
             sqs_list.begin();
         sqs_iter != sqs_list.end();
         ++sqs_iter) {
      EXPECT_EQ(*sqs_iter, &*iter);
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
         iter != list.end();
         ++iter) {
      EXPECT_TRUE(isConstSharedQuadStatePointer(&*iter));
      EXPECT_EQ(*sqs_iter, &*iter);
      ++sqs_iter;
    }
  }

  {
    std::vector<const SharedQuadState*>::const_iterator sqs_iter =
        sqs_list.begin();
    for (ListContainer<SharedQuadState>::Iterator iter = list.begin();
         iter != list.end();
         ++iter) {
      EXPECT_FALSE(isConstSharedQuadStatePointer(&*iter));
      EXPECT_EQ(*sqs_iter, &*iter);
      ++sqs_iter;
    }
  }

  {
    ListContainer<SharedQuadState>::ConstIterator iter = list.begin();
    for (std::vector<const SharedQuadState*>::const_iterator sqs_iter =
             sqs_list.begin();
         sqs_iter != sqs_list.end();
         ++sqs_iter) {
      EXPECT_EQ(*sqs_iter, &*iter);
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
         iter != list.rend();
         ++iter) {
      EXPECT_EQ(*sqs_iter, &(*iter));
      ++sqs_iter;
    }
  }

  {
    ListContainer<SharedQuadState>::ReverseIterator iter = list.rbegin();
    for (std::vector<SharedQuadState*>::reverse_iterator sqs_iter =
             sqs_list.rbegin();
         sqs_iter != sqs_list.rend();
         ++sqs_iter) {
      EXPECT_EQ(*sqs_iter, &(*iter));
      ++iter;
    }
  }
}

TEST(ListContainerTest, SimpleDeletion) {
  ListContainer<DrawQuad> list(sizeof(kLargestDrawQuad));
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
       iter != list.end();
       ++iter) {
    EXPECT_EQ(i, static_cast<SimpleDrawQuad*>(&*iter)->get_value());
    ++i;
  }
}

TEST(ListContainerTest, SimpleIterationAndManipulation) {
  ListContainer<DrawQuad> list(sizeof(kLargestDrawQuad));
  std::vector<SimpleDrawQuad*> sdq_list;
  size_t size = 10;
  for (size_t i = 0; i < size; ++i) {
    SimpleDrawQuad* simple_dq = list.AllocateAndConstruct<SimpleDrawQuad>();
    sdq_list.push_back(simple_dq);
  }
  EXPECT_EQ(size, list.size());

  ListContainer<DrawQuad>::Iterator iter = list.begin();
  for (int i = 0; i < 10; ++i) {
    static_cast<SimpleDrawQuad*>(&*iter)->set_value(i);
    ++iter;
  }

  int i = 0;
  for (std::vector<SimpleDrawQuad*>::const_iterator sdq_iter = sdq_list.begin();
       sdq_iter < sdq_list.end();
       ++sdq_iter) {
    EXPECT_EQ(i, (*sdq_iter)->get_value());
    ++i;
  }
}

TEST(ListContainerTest, SimpleManipulationWithIndexSimpleDrawQuad) {
  ListContainer<DrawQuad> list(sizeof(kLargestDrawQuad));
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
       dq_iter != dq_list.end();
       ++dq_iter, ++i) {
    EXPECT_EQ(i, (*dq_iter)->get_value());
  }
}

TEST(ListContainerTest,
     SimpleManipulationWithIndexMoreThanOneAllocationSimpleDrawQuad) {
  ListContainer<DrawQuad> list(sizeof(kLargestDrawQuad), 2);
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
       dq_iter != dq_list.end();
       ++dq_iter, ++i) {
    EXPECT_EQ(i, (*dq_iter)->get_value());
  }
}

}  // namespace
}  // namespace cc
