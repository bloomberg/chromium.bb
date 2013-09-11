// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_registry.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class Listener {
 public:
  Listener() : total_(0), scaler_(1) {}
  explicit Listener(int scaler) : total_(0), scaler_(scaler) {}
  void IncrementTotal() { total_++; }
  void IncrementByMultipleOfScaler(const int& x) { total_ += x * scaler_; }

  int total_;

 private:
  int scaler_;
  DISALLOW_COPY_AND_ASSIGN(Listener);
};

class Remover {
 public:
  Remover() : total_(0) {}
  void IncrementTotalAndRemove() {
    total_++;
    removal_subscription_.reset();
  }
  void SetSubscriptionToRemove(
      scoped_ptr<CallbackRegistry<void>::Subscription> sub) {
    removal_subscription_ = sub.Pass();
  }

  int total_;

 private:
  scoped_ptr<CallbackRegistry<void>::Subscription> removal_subscription_;
  DISALLOW_COPY_AND_ASSIGN(Remover);
};

class Adder {
 public:
  explicit Adder(CallbackRegistry<void>* cb_reg)
      : added_(false),
        total_(0),
        cb_reg_(cb_reg) {}
  void AddCallback() {
    if (!added_) {
      added_ = true;
      subscription_ =
          cb_reg_->Add(Bind(&Adder::IncrementTotal, Unretained(this)));
    }
  }
  void IncrementTotal() { total_++; }

  bool added_;
  int total_;

 private:
  CallbackRegistry<void>* cb_reg_;
  scoped_ptr<CallbackRegistry<void>::Subscription> subscription_;
  DISALLOW_COPY_AND_ASSIGN(Adder);
};

// Sanity check that closures added to the list will be run, and those removed
// from the list will not be run.
TEST(CallbackRegistryTest, BasicTest) {
  CallbackRegistry<void> cb_reg;
  Listener a, b, c;

  scoped_ptr<CallbackRegistry<void>::Subscription> a_subscription =
      cb_reg.Add(Bind(&Listener::IncrementTotal, Unretained(&a)));
  scoped_ptr<CallbackRegistry<void>::Subscription> b_subscription =
      cb_reg.Add(Bind(&Listener::IncrementTotal, Unretained(&b)));

  EXPECT_TRUE(a_subscription.get());
  EXPECT_TRUE(b_subscription.get());

  cb_reg.Notify();

  EXPECT_EQ(1, a.total_);
  EXPECT_EQ(1, b.total_);

  b_subscription.reset();

  scoped_ptr<CallbackRegistry<void>::Subscription> c_subscription =
      cb_reg.Add(Bind(&Listener::IncrementTotal, Unretained(&c)));

  cb_reg.Notify();

  EXPECT_EQ(2, a.total_);
  EXPECT_EQ(1, b.total_);
  EXPECT_EQ(1, c.total_);

  a_subscription.reset();
  b_subscription.reset();
  c_subscription.reset();
}

// Sanity check that callbacks with details added to the list will be run, with
// the correct details, and those removed from the list will not be run.
TEST(CallbackRegistryTest, BasicTestWithParams) {
  CallbackRegistry<int> cb_reg;
  Listener a(1), b(-1), c(1);

  scoped_ptr<CallbackRegistry<int>::Subscription> a_subscription =
      cb_reg.Add(Bind(&Listener::IncrementByMultipleOfScaler, Unretained(&a)));
  scoped_ptr<CallbackRegistry<int>::Subscription> b_subscription =
      cb_reg.Add(Bind(&Listener::IncrementByMultipleOfScaler, Unretained(&b)));

  EXPECT_TRUE(a_subscription.get());
  EXPECT_TRUE(b_subscription.get());

  cb_reg.Notify(10);

  EXPECT_EQ(10, a.total_);
  EXPECT_EQ(-10, b.total_);

  b_subscription.reset();

  scoped_ptr<CallbackRegistry<int>::Subscription> c_subscription =
      cb_reg.Add(Bind(&Listener::IncrementByMultipleOfScaler, Unretained(&c)));

  cb_reg.Notify(10);

  EXPECT_EQ(20, a.total_);
  EXPECT_EQ(-10, b.total_);
  EXPECT_EQ(10, c.total_);

  a_subscription.reset();
  b_subscription.reset();
  c_subscription.reset();
}

// Test the a callback can remove itself or a different callback from the list
// during iteration without invalidating the iterator.
TEST(CallbackRegistryTest, RemoveCallbacksDuringIteration) {
  CallbackRegistry<void> cb_reg;
  Listener a, b;
  Remover remover_1, remover_2;

  scoped_ptr<CallbackRegistry<void>::Subscription> remover_1_subscription =
      cb_reg.Add(Bind(&Remover::IncrementTotalAndRemove,
          Unretained(&remover_1)));
  scoped_ptr<CallbackRegistry<void>::Subscription> remover_2_subscription =
      cb_reg.Add(Bind(&Remover::IncrementTotalAndRemove,
          Unretained(&remover_2)));
  scoped_ptr<CallbackRegistry<void>::Subscription> a_subscription =
      cb_reg.Add(Bind(&Listener::IncrementTotal, Unretained(&a)));
  scoped_ptr<CallbackRegistry<void>::Subscription> b_subscription =
      cb_reg.Add(Bind(&Listener::IncrementTotal, Unretained(&b)));

  // |remover_1| will remove itself.
  remover_1.SetSubscriptionToRemove(remover_1_subscription.Pass());
  // |remover_2| will remove a.
  remover_2.SetSubscriptionToRemove(a_subscription.Pass());

  cb_reg.Notify();

  // |remover_1| runs once (and removes itself), |remover_2| runs once (and
  // removes a), |a| never runs, and |b| runs once.
  EXPECT_EQ(1, remover_1.total_);
  EXPECT_EQ(1, remover_2.total_);
  EXPECT_EQ(0, a.total_);
  EXPECT_EQ(1, b.total_);

  cb_reg.Notify();

  // Only |remover_2| and |b| run this time.
  EXPECT_EQ(1, remover_1.total_);
  EXPECT_EQ(2, remover_2.total_);
  EXPECT_EQ(0, a.total_);
  EXPECT_EQ(2, b.total_);
}

// Test that a callback can add another callback to the list durning iteration
// without invalidating the iterator. The newly added callback should be run on
// the current iteration as will all other callbacks in the list.
TEST(CallbackRegistryTest, AddCallbacksDuringIteration) {
  CallbackRegistry<void> cb_reg;
  Adder a(&cb_reg);
  Listener b;
  scoped_ptr<CallbackRegistry<void>::Subscription> a_subscription =
      cb_reg.Add(Bind(&Adder::AddCallback, Unretained(&a)));
  scoped_ptr<CallbackRegistry<void>::Subscription> b_subscription =
      cb_reg.Add(Bind(&Listener::IncrementTotal, Unretained(&b)));

  cb_reg.Notify();

  EXPECT_EQ(1, a.total_);
  EXPECT_EQ(1, b.total_);
  EXPECT_TRUE(a.added_);

  cb_reg.Notify();

  EXPECT_EQ(2, a.total_);
  EXPECT_EQ(2, b.total_);
}

// Sanity check: notifying an empty list is a no-op.
TEST(CallbackRegistryTest, EmptyList) {
  CallbackRegistry<void> cb_reg;

  cb_reg.Notify();
}

}  // namespace
}  // namespace base
