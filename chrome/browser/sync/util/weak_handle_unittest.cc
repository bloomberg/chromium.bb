// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/weak_handle.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/tracked.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;

class Base {
 public:
  Base() : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  WeakHandle<Base> AsWeakHandle() {
    return MakeWeakHandle(weak_ptr_factory_.GetWeakPtr());
  }

  void Kill() {
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  MOCK_METHOD0(Test, void());
  MOCK_METHOD1(Test1, void(const int&));
  MOCK_METHOD2(Test2, void(const int&, Base*));
  MOCK_METHOD3(Test3, void(const int&, Base*, float));
  MOCK_METHOD4(Test4, void(const int&, Base*, float, const char*));

  MOCK_METHOD1(TestWithSelf, void(const WeakHandle<Base>&));

 private:
  base::WeakPtrFactory<Base> weak_ptr_factory_;
};

class WeakHandleTest : public ::testing::Test {
 protected:
  virtual void TearDown() {
    // Process any last-minute posted tasks.
    PumpLoop();
  }

  void PumpLoop() {
    message_loop_.RunAllPending();
  }

  static void CallTestFromOtherThread(tracked_objects::Location from_here,
                                      const WeakHandle<Base>& h) {
    base::Thread t("Test thread");
    ASSERT_TRUE(t.Start());
    t.message_loop()->PostTask(
        from_here, base::Bind(&WeakHandleTest::CallTest, from_here, h));
  }

 private:
  static void CallTest(tracked_objects::Location from_here,
                       const WeakHandle<Base>& h) {
    h.Call(from_here, &Base::Test);
  }

  MessageLoop message_loop_;
};

TEST_F(WeakHandleTest, Uninitialized) {
  // Default.
  WeakHandle<int> h;
  EXPECT_FALSE(h.IsInitialized());
  // Copy.
  {
    WeakHandle<int> h2(h);
    EXPECT_FALSE(h2.IsInitialized());
  }
  // Assign.
  {
    WeakHandle<int> h2;
    h2 = h;
    EXPECT_FALSE(h.IsInitialized());
  }
}

TEST_F(WeakHandleTest, InitializedAfterDestroy) {
  WeakHandle<Base> h;
  {
    StrictMock<Base> b;
    h = b.AsWeakHandle();
  }
  EXPECT_TRUE(h.IsInitialized());
  EXPECT_FALSE(h.Get());
}

TEST_F(WeakHandleTest, InitializedAfterInvalidate) {
  StrictMock<Base> b;
  WeakHandle<Base> h = b.AsWeakHandle();
  b.Kill();
  EXPECT_TRUE(h.IsInitialized());
  EXPECT_FALSE(h.Get());
}

TEST_F(WeakHandleTest, Call) {
  StrictMock<Base> b;
  const char test_str[] = "test";
  EXPECT_CALL(b, Test());
  EXPECT_CALL(b, Test1(5));
  EXPECT_CALL(b, Test2(5, &b));
  EXPECT_CALL(b, Test3(5, &b, 5));
  EXPECT_CALL(b, Test4(5, &b, 5, test_str));

  WeakHandle<Base> h = b.AsWeakHandle();
  EXPECT_TRUE(h.IsInitialized());

  // Should run.
  h.Call(FROM_HERE, &Base::Test);
  h.Call(FROM_HERE, &Base::Test1, 5);
  h.Call(FROM_HERE, &Base::Test2, 5, &b);
  h.Call(FROM_HERE, &Base::Test3, 5, &b, 5);
  h.Call(FROM_HERE, &Base::Test4, 5, &b, 5, test_str);
  PumpLoop();
}

TEST_F(WeakHandleTest, CallAfterDestroy) {
  {
    StrictMock<Base> b;
    EXPECT_CALL(b, Test()).Times(0);

    WeakHandle<Base> h = b.AsWeakHandle();
    EXPECT_TRUE(h.IsInitialized());

    // Should not run.
    h.Call(FROM_HERE, &Base::Test);
  }
  PumpLoop();
}

TEST_F(WeakHandleTest, CallAfterInvalidate) {
  StrictMock<Base> b;
  EXPECT_CALL(b, Test()).Times(0);

  WeakHandle<Base> h = b.AsWeakHandle();
  EXPECT_TRUE(h.IsInitialized());

  // Should not run.
  h.Call(FROM_HERE, &Base::Test);

  b.Kill();
  PumpLoop();
}

TEST_F(WeakHandleTest, CallThreaded) {
  StrictMock<Base> b;
  EXPECT_CALL(b, Test());

  WeakHandle<Base> h = b.AsWeakHandle();
  // Should run.
  CallTestFromOtherThread(FROM_HERE, h);
  PumpLoop();
}

TEST_F(WeakHandleTest, CallAfterDestroyThreaded) {
  WeakHandle<Base> h;
  {
    StrictMock<Base> b;
    EXPECT_CALL(b, Test()).Times(0);
    h = b.AsWeakHandle();
  }

  // Should not run.
  CallTestFromOtherThread(FROM_HERE, h);
  PumpLoop();
}

TEST_F(WeakHandleTest, CallAfterInvalidateThreaded) {
  StrictMock<Base> b;
  EXPECT_CALL(b, Test()).Times(0);

  WeakHandle<Base> h = b.AsWeakHandle();
  b.Kill();
  // Should not run.
  CallTestFromOtherThread(FROM_HERE, h);
  PumpLoop();
}

TEST_F(WeakHandleTest, DeleteOnOtherThread) {
  StrictMock<Base> b;
  EXPECT_CALL(b, Test()).Times(0);

  WeakHandle<Base>* h = new WeakHandle<Base>(b.AsWeakHandle());

  {
    base::Thread t("Test thread");
    ASSERT_TRUE(t.Start());
    t.message_loop()->DeleteSoon(FROM_HERE, h);
  }

  PumpLoop();
}

void CallTestWithSelf(const WeakHandle<Base>& b1) {
  StrictMock<Base> b2;
  b1.Call(FROM_HERE, &Base::TestWithSelf, b2.AsWeakHandle());
}

TEST_F(WeakHandleTest, WithDestroyedThread) {
  StrictMock<Base> b1;
  WeakHandle<Base> b2;
  EXPECT_CALL(b1, TestWithSelf(_)).WillOnce(SaveArg<0>(&b2));

  {
    base::Thread t("Test thread");
    ASSERT_TRUE(t.Start());
    t.message_loop()->PostTask(FROM_HERE,
                               base::Bind(&CallTestWithSelf,
                                          b1.AsWeakHandle()));
  }

  // Calls b1.TestWithSelf().
  PumpLoop();

  // Shouldn't do anything, since the thread is gone.
  b2.Call(FROM_HERE, &Base::Test);

  // |b2| shouldn't leak when it's destroyed, even if the original
  // thread is gone.
}

TEST_F(WeakHandleTest, InitializedAcrossCopyAssign) {
  StrictMock<Base> b;
  EXPECT_CALL(b, Test()).Times(3);

  EXPECT_TRUE(b.AsWeakHandle().IsInitialized());
  b.AsWeakHandle().Call(FROM_HERE, &Base::Test);

  {
    WeakHandle<Base> h(b.AsWeakHandle());
    EXPECT_TRUE(h.IsInitialized());
    h.Call(FROM_HERE, &Base::Test);
    h.Reset();
    EXPECT_FALSE(h.IsInitialized());
  }

  {
    WeakHandle<Base> h;
    h = b.AsWeakHandle();
    EXPECT_TRUE(h.IsInitialized());
    h.Call(FROM_HERE, &Base::Test);
    h.Reset();
    EXPECT_FALSE(h.IsInitialized());
  }

  PumpLoop();
}

}  // namespace
}  // namespace browser_sync
