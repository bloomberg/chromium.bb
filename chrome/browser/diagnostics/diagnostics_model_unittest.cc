// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_model.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace diagnostics {

// Basic harness to acquire and release the Diagnostic model object.
class DiagnosticsModelTest : public testing::Test {
 protected:
  DiagnosticsModelTest()
      : model_(NULL),
        cmdline_(CommandLine::NO_PROGRAM) {
  }

  virtual ~DiagnosticsModelTest() { }

  virtual void SetUp() {
    model_ = MakeDiagnosticsModel(cmdline_);
    ASSERT_TRUE(model_ != NULL);
  }

  virtual void TearDown() {
    delete model_;
  }

  DiagnosticsModel* model_;
  CommandLine cmdline_;
};

// The test observer is used to know if the callbacks are being called.
class UTObserver: public DiagnosticsModel::Observer {
 public:
  UTObserver()
      : done_(false),
        finished_(0),
        id_of_failed_stop_test(-1) {
  }

  virtual void OnFinished(int index, DiagnosticsModel* model) OVERRIDE {
    EXPECT_TRUE(model != NULL);
    ++finished_;
    if (model->GetTest(index).GetResult() == DiagnosticsModel::TEST_FAIL_STOP) {
      id_of_failed_stop_test = index;
      ASSERT_TRUE(false);
    }
  }

  virtual void OnDoneAll(DiagnosticsModel* model) OVERRIDE {
    done_ = true;
    EXPECT_TRUE(model != NULL);
  }

  bool done() const { return done_; }

  int finished() const { return finished_;}

 private:
  bool done_;
  int finished_;
  int id_of_failed_stop_test;
};

// This is the count of tests on each platform.
#if defined(OS_WIN)
const int kDiagnosticsTestCount = 19;
#elif defined(OS_MACOSX)
const int kDiagnosticsTestCount = 16;
#elif defined(OS_POSIX)
#if defined(OS_CHROMEOS)
const int kDiagnosticsTestCount = 19;
#else
const int kDiagnosticsTestCount = 17;
#endif
#endif

// Test that the initial state is correct.
TEST_F(DiagnosticsModelTest, BeforeRun) {
  int available = model_->GetTestAvailableCount();
  EXPECT_EQ(kDiagnosticsTestCount, available);
  EXPECT_EQ(0, model_->GetTestRunCount());
  EXPECT_EQ(DiagnosticsModel::TEST_NOT_RUN, model_->GetTest(0).GetResult());
}

// Run all the tests, verify that the basic callbacks are run and that the
// final state is correct.
TEST_F(DiagnosticsModelTest, RunAll) {
  UTObserver observer;
  EXPECT_FALSE(observer.done());
  model_->RunAll(&observer);
  EXPECT_TRUE(observer.done());
  EXPECT_EQ(kDiagnosticsTestCount, model_->GetTestRunCount());
  EXPECT_EQ(kDiagnosticsTestCount, observer.finished());
}

}  // namespace diagnostics
