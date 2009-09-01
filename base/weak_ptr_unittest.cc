// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "base/thread.h"
#include "base/weak_ptr.h"

namespace base {
namespace {

template <class T>
class OffThreadObjectCreator {
 public:
  static T* NewObject() {
    T* result;
    {
      Thread creator_thread("creator_thread");
      creator_thread.Start();
      creator_thread.message_loop()->PostTask(FROM_HERE,
          NewRunnableFunction(OffThreadObjectCreator::CreateObject, &result));
    }
    DCHECK(result);  // We synchronized on thread destruction above.
    return result;
  }
 private:
  static void CreateObject(T** result) {
    *result = new T;
  }
};

struct Base {};
struct Derived : Base {};

struct Producer : SupportsWeakPtr<Producer> {};
struct Consumer { WeakPtr<Producer> producer; };

}  // namespace

TEST(WeakPtrTest, Basic) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
}

TEST(WeakPtrTest, Comparison) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  WeakPtr<int> ptr2 = ptr;
  EXPECT_TRUE(ptr == ptr2);
}

TEST(WeakPtrTest, OutOfScope) {
  WeakPtr<int> ptr;
  EXPECT_EQ(NULL, ptr.get());
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    ptr = factory.GetWeakPtr();
  }
  EXPECT_EQ(NULL, ptr.get());
}

TEST(WeakPtrTest, Multiple) {
  WeakPtr<int> a, b;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    b = factory.GetWeakPtr();
    EXPECT_EQ(&data, a.get());
    EXPECT_EQ(&data, b.get());
  }
  EXPECT_EQ(NULL, a.get());
  EXPECT_EQ(NULL, b.get());
}

TEST(WeakPtrTest, UpCast) {
  Derived data;
  WeakPtrFactory<Derived> factory(&data);
  WeakPtr<Base> ptr = factory.GetWeakPtr();
  ptr = factory.GetWeakPtr();
  EXPECT_EQ(ptr.get(), &data);
}

TEST(WeakPtrTest, SupportsWeakPtr) {
  Producer f;
  WeakPtr<Producer> ptr = f.AsWeakPtr();
  EXPECT_EQ(&f, ptr.get());
}

TEST(WeakPtrTest, InvalidateWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  factory.InvalidateWeakPtrs();
  EXPECT_EQ(NULL, ptr.get());
}

TEST(WeakPtrTest, SingleThreaded1) {
  // Test that it is OK to create a class that supports weak references on one
  // thread, but use it on another.  This tests that we do not trip runtime
  // checks that ensure that a weak reference is not used by multiple threads.
  scoped_ptr<Producer> producer(OffThreadObjectCreator<Producer>::NewObject());
  WeakPtr<Producer> weak_producer = producer->AsWeakPtr();
  EXPECT_EQ(producer.get(), weak_producer.get());
}

TEST(WeakPtrTest, SingleThreaded2) {
  // Test that it is OK to create a class that has a WeakPtr member on one
  // thread, but use it on another.  This tests that we do not trip runtime
  // checks that ensure that a weak reference is not used by multiple threads.
  scoped_ptr<Consumer> consumer(OffThreadObjectCreator<Consumer>::NewObject());
  Producer producer;
  consumer->producer = producer.AsWeakPtr();
  EXPECT_EQ(&producer, consumer->producer.get());
}

}  // namespace base
