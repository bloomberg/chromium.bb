// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_model.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace diagnostics {

// Basic harness to acquire and release the Diagnostic model object.
class DiagnosticsModelTest : public testing::Test {
 protected:
  DiagnosticsModelTest()
      : cmdline_(CommandLine::NO_PROGRAM) {
  }

  virtual ~DiagnosticsModelTest() { }

  virtual void SetUp() {
    model_.reset(MakeDiagnosticsModel(cmdline_));
    ASSERT_TRUE(model_.get() != NULL);
  }

  virtual void TearDown() {
    model_.reset();
  }

  scoped_ptr<DiagnosticsModel> model_;
  CommandLine cmdline_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsModelTest);
};

// The test observer is used to know if the callbacks are being called.
class UTObserver: public DiagnosticsModel::Observer {
 public:
  UTObserver()
      : tests_done_(false),
        recovery_done_(false),
        num_tested_(0),
        num_recovered_(0) {
  }

  virtual void OnTestFinished(int index, DiagnosticsModel* model) OVERRIDE {
    EXPECT_TRUE(model != NULL);
    ++num_tested_;
    EXPECT_NE(DiagnosticsModel::TEST_FAIL_STOP,
              model->GetTest(index).GetResult())
        << "Failed stop test: " << index;
  }

  virtual void OnAllTestsDone(DiagnosticsModel* model) OVERRIDE {
    EXPECT_TRUE(model != NULL);
    tests_done_ = true;
  }

  virtual void OnRecoveryFinished(int index, DiagnosticsModel* model) OVERRIDE {
    EXPECT_TRUE(model != NULL);
    ++num_recovered_;
    EXPECT_NE(DiagnosticsModel::RECOVERY_FAIL_STOP,
              model->GetTest(index).GetResult())
        << "Failed stop recovery: " << index;
  }

  virtual void OnAllRecoveryDone(DiagnosticsModel* model) OVERRIDE {
    EXPECT_TRUE(model != NULL);
    recovery_done_ = true;
  }

  bool tests_done() const { return tests_done_; }
  bool recovery_done() const { return recovery_done_; }

  int num_tested() const { return num_tested_;}
  int num_recovered() const { return num_recovered_;}

 private:
  bool tests_done_;
  bool recovery_done_;
  int num_tested_;
  int num_recovered_;

  DISALLOW_COPY_AND_ASSIGN(UTObserver);
};

// Test that the initial state is correct.
TEST_F(DiagnosticsModelTest, BeforeRun) {
  int available = model_->GetTestAvailableCount();
  EXPECT_EQ(DiagnosticsModel::kDiagnosticsTestCount, available);
  EXPECT_EQ(0, model_->GetTestRunCount());
  EXPECT_EQ(DiagnosticsModel::TEST_NOT_RUN, model_->GetTest(0).GetResult());
}

// Run all the tests, verify that the basic callbacks are run and that the
// final state is correct.
TEST_F(DiagnosticsModelTest, RunAll) {
  UTObserver observer;
  EXPECT_FALSE(observer.tests_done());
  model_->RunAll(&observer);
  EXPECT_TRUE(observer.tests_done());
  EXPECT_FALSE(observer.recovery_done());
  model_->RecoverAll(&observer);
  EXPECT_TRUE(observer.recovery_done());
  EXPECT_EQ(DiagnosticsModel::kDiagnosticsTestCount, model_->GetTestRunCount());
  EXPECT_EQ(DiagnosticsModel::kDiagnosticsTestCount, observer.num_tested());
  EXPECT_EQ(DiagnosticsModel::kDiagnosticsTestCount, observer.num_recovered());
}

}  // namespace diagnostics
