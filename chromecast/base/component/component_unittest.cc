// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/component/component.h"

#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

class ComponentTest : public ::testing::Test {
 protected:
  ComponentTest() : message_loop_(new base::MessageLoop()) {}

  const scoped_ptr<base::MessageLoop> message_loop_;
};

using ComponentDeathTest = ComponentTest;

class ComponentB;
class ComponentC;
class ComponentA : public Component<ComponentA> {
 public:
  void MakeSelfDependency() {
    a_.reset(new Component<ComponentA>::Dependency(GetRef(), this));
  }

  void MakeCircularDependency(const Component<ComponentB>::WeakRef& b) {
    b_.reset(new Component<ComponentB>::Dependency(b, this));
  }

  void MakeTransitiveCircularDependency(
      const Component<ComponentC>::WeakRef& c) {
    c_.reset(new Component<ComponentC>::Dependency(c, this));
  }

  void OnEnable() override {
    if (!fail_enable_) {
      enabled_ = true;
      Test();
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ComponentA::OnEnableComplete,
                              base::Unretained(this), !fail_enable_));
  }

  void OnDisable() override {
    if (enabled_)
      Test();
    enabled_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ComponentA::OnDisableComplete, base::Unretained(this)));
  }

  void Test() {
    EXPECT_TRUE(enabled_);
    EXPECT_FALSE(fail_enable_);
  }

  bool enabled() const { return enabled_; }
  void FailEnable() { fail_enable_ = true; }

 private:
  bool enabled_ = false;
  bool fail_enable_ = false;

  scoped_ptr<Component<ComponentA>::Dependency> a_;
  scoped_ptr<Component<ComponentB>::Dependency> b_;
  scoped_ptr<Component<ComponentC>::Dependency> c_;
};

class ComponentB : public Component<ComponentB> {
 public:
  explicit ComponentB(const ComponentA::WeakRef& a) : a_(a, this) {}

  void OnEnable() override {
    if (!fail_enable_) {
      enabled_ = true;
      Test();
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ComponentB::OnEnableComplete,
                              base::Unretained(this), !fail_enable_));
  }

  void OnDisable() override {
    if (enabled_)
      Test();
    enabled_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ComponentB::OnDisableComplete, base::Unretained(this)));
  }

  void Test() {
    EXPECT_TRUE(enabled_);
    EXPECT_FALSE(fail_enable_);
    a_->Test();
  }

  bool enabled() const { return enabled_; }
  void FailEnable() { fail_enable_ = true; }

 private:
  bool enabled_ = false;
  bool fail_enable_ = false;

  ComponentA::Dependency a_;
};

class ComponentC : public Component<ComponentC> {
 public:
  explicit ComponentC(const ComponentB::WeakRef& b) : b_(b, this) {}

  void OnEnable() override {
    if (!fail_enable_) {
      enabled_ = true;
      Test();
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ComponentC::OnEnableComplete,
                              base::Unretained(this), !fail_enable_));
  }

  void OnDisable() override {
    if (enabled_)
      Test();
    enabled_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ComponentC::OnDisableComplete, base::Unretained(this)));
  }

  void Test() {
    EXPECT_TRUE(enabled_);
    EXPECT_FALSE(fail_enable_);
    b_->Test();
  }

  bool enabled() const { return enabled_; }
  void FailEnable() { fail_enable_ = true; }

 private:
  bool enabled_ = false;
  bool fail_enable_ = false;

  ComponentB::Dependency b_;
};

TEST_F(ComponentDeathTest, SelfDependency) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  ComponentA a;
  EXPECT_DEATH(a.MakeSelfDependency(), "Circular dependency");
}

TEST_F(ComponentDeathTest, CircularDependency) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  ComponentA a;
  ComponentB b(a.GetRef());
  EXPECT_DEATH(a.MakeCircularDependency(b.GetRef()), "Circular dependency");
}

TEST_F(ComponentDeathTest, TransitiveCircularDependency) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  ComponentA a;
  ComponentB b(a.GetRef());
  ComponentC c(b.GetRef());
  EXPECT_DEATH(a.MakeTransitiveCircularDependency(c.GetRef()),
               "Circular dependency");
}

TEST_F(ComponentTest, SimpleEnable) {
  scoped_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  message_loop_->RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, TransitiveEnable) {
  scoped_ptr<ComponentA> a(new ComponentA());
  scoped_ptr<ComponentB> b(new ComponentB(a->GetRef()));
  scoped_ptr<ComponentC> c(new ComponentC(b->GetRef()));
  c->Enable();
  message_loop_->RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  EXPECT_TRUE(b->enabled());
  EXPECT_TRUE(c->enabled());
  a.release()->Destroy();
  b.release()->Destroy();
  c.release()->Destroy();
}

TEST_F(ComponentTest, FailEnable) {
  scoped_ptr<ComponentA> a(new ComponentA());
  a->FailEnable();
  a->Enable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, TransitiveFailEnable) {
  scoped_ptr<ComponentA> a(new ComponentA());
  scoped_ptr<ComponentB> b(new ComponentB(a->GetRef()));
  scoped_ptr<ComponentC> c(new ComponentC(b->GetRef()));
  a->FailEnable();
  c->Enable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  EXPECT_FALSE(b->enabled());
  EXPECT_FALSE(c->enabled());
  a.release()->Destroy();
  b.release()->Destroy();
  c.release()->Destroy();
}

TEST_F(ComponentTest, DisableWhileEnabling) {
  scoped_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  a->Disable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, EnableTwice) {
  scoped_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  a->Enable();
  message_loop_->RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, DisableTwice) {
  scoped_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  message_loop_->RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  a->Disable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a->Disable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, DisableAfterFailedEnable) {
  scoped_ptr<ComponentA> a(new ComponentA());
  a->FailEnable();
  a->Enable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a->Disable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, DisableAfterNeverEnabled) {
  scoped_ptr<ComponentA> a(new ComponentA());
  a->Disable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, DisableDependencyWhileEnabling) {
  scoped_ptr<ComponentA> a(new ComponentA());
  scoped_ptr<ComponentB> b(new ComponentB(a->GetRef()));
  scoped_ptr<ComponentC> c(new ComponentC(b->GetRef()));
  b->Enable();
  message_loop_->RunUntilIdle();
  c->Enable();
  a->Disable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  EXPECT_FALSE(b->enabled());
  EXPECT_FALSE(c->enabled());
  a.release()->Destroy();
  b.release()->Destroy();
  c.release()->Destroy();
}

TEST_F(ComponentTest, EnableDisableEnable) {
  scoped_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  a->Disable();
  a->Enable();
  message_loop_->RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, DisableEnableDisable) {
  scoped_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  message_loop_->RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  a->Disable();
  a->Enable();
  a->Disable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, TransitiveEnableDisableEnable) {
  scoped_ptr<ComponentA> a(new ComponentA());
  scoped_ptr<ComponentB> b(new ComponentB(a->GetRef()));
  scoped_ptr<ComponentC> c(new ComponentC(b->GetRef()));
  a->Enable();
  message_loop_->RunUntilIdle();
  c->Enable();
  a->Disable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  EXPECT_FALSE(b->enabled());
  EXPECT_FALSE(c->enabled());
  c->Enable();
  message_loop_->RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  EXPECT_TRUE(b->enabled());
  EXPECT_TRUE(c->enabled());
  a.release()->Destroy();
  b.release()->Destroy();
  c.release()->Destroy();
}

TEST_F(ComponentTest, WeakRefs) {
  scoped_ptr<ComponentA> a(new ComponentA());
  ComponentA::WeakRef weak = a->GetRef();
  EXPECT_FALSE(weak.Try());
  a->Enable();
  EXPECT_FALSE(weak.Try());
  message_loop_->RunUntilIdle();
  EXPECT_TRUE(weak.Try());
  weak.Try()->Test();
  a->Disable();
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(weak.Try());
  a.release()->Destroy();
}

TEST_F(ComponentTest, WeakRefsKeepEnabled) {
  scoped_ptr<ComponentA> a(new ComponentA());
  ComponentA::WeakRef weak = a->GetRef();
  EXPECT_FALSE(weak.Try());
  a->Enable();
  EXPECT_FALSE(weak.Try());
  message_loop_->RunUntilIdle();
  {
    auto held_ref = weak.Try();
    EXPECT_TRUE(held_ref);
    held_ref->Test();
    a->Disable();
    message_loop_->RunUntilIdle();
    // The held ref keeps |a| enabled until it goes out of scope.
    EXPECT_TRUE(a->enabled());
  }
  message_loop_->RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  EXPECT_FALSE(weak.Try());
  a.release()->Destroy();
}

}  // namespace chromecast
