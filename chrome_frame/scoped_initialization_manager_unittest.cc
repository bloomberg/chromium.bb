// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/scoped_initialization_manager.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::InSequence;

// Initialization traits that delegate to a registered delegate instance.
class TestInitializationTraits {
 public:
  class Delegate {
   public:
    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
  };

  static void SetDelegate(Delegate* delegate) {
    delegate_ = delegate;
  }

  static void Initialize() {
    ASSERT_TRUE(delegate_ != NULL);
    delegate_->Initialize();
  }

  static void Shutdown() {
    ASSERT_TRUE(delegate_ != NULL);
    delegate_->Shutdown();
  }

 private:
  static Delegate* delegate_;
};

TestInitializationTraits::Delegate* TestInitializationTraits::delegate_ = NULL;

// A mock delegate for use in tests.
class MockInitializationTraitsDelegate
    : public TestInitializationTraits::Delegate {
 public:
  MOCK_METHOD0(Initialize, void());
  MOCK_METHOD0(Shutdown, void());
};

// A test fixture that sets up a mock traits delegate for use in tests.
class ScopedInitializationManagerTest : public ::testing::Test {
 protected:
  // A manager type that will invoke methods
  typedef chrome_frame::ScopedInitializationManager<TestInitializationTraits>
      TestScopedInitializationManager;

  virtual void SetUp() OVERRIDE {
    TestInitializationTraits::SetDelegate(&mock_traits_delegate_);
  }
  virtual void TearDown() OVERRIDE {
    TestInitializationTraits::SetDelegate(NULL);
  }

  MockInitializationTraitsDelegate mock_traits_delegate_;
};

// Test that Initialize and Shutdown are called in order for the simple case.
TEST_F(ScopedInitializationManagerTest, InitializeAndShutdown) {
  {
    InSequence dummy;
    EXPECT_CALL(mock_traits_delegate_, Initialize());
    EXPECT_CALL(mock_traits_delegate_, Shutdown());
  }
  TestScopedInitializationManager test_manager;
}

// Test that Initialize and Shutdown are called in order only once despite
// multiple instances.
TEST_F(ScopedInitializationManagerTest, InitializeAndShutdownOnlyOnce) {
  {
    InSequence dummy;
    EXPECT_CALL(mock_traits_delegate_, Initialize());
    EXPECT_CALL(mock_traits_delegate_, Shutdown());
  }
  TestScopedInitializationManager test_manager_1;
  TestScopedInitializationManager test_manager_2;
  TestScopedInitializationManager test_manager_3;
}
