// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_model.h"

#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

// Basic harness to adquire and release the Diagnostic model object.
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
        progress_called_(0),
        finished_(0),
        id_of_failed_stop_test(-1) {
  }

  virtual void OnProgress(int id, int percent, DiagnosticsModel* model) {
    EXPECT_TRUE(model != NULL);
    ++progress_called_;
  }

  virtual void OnSkipped(int id, DiagnosticsModel* model) {
    EXPECT_TRUE(model != NULL);
  }

  virtual void OnFinished(int id, DiagnosticsModel* model) {
    EXPECT_TRUE(model != NULL);
    ++finished_;
    if (model->GetTest(id).GetResult() == DiagnosticsModel::TEST_FAIL_STOP) {
      id_of_failed_stop_test = id;
      ASSERT_TRUE(false);
    }
  }

  virtual void OnDoneAll(DiagnosticsModel* model) {
    done_ = true;
    EXPECT_TRUE(model != NULL);
  }

  bool done() const { return done_; }

  int progress_called() const { return progress_called_; }

  int finished() const { return finished_;}

 private:
  bool done_;
  int progress_called_;
  int finished_;
  int id_of_failed_stop_test;
};

// We currently have more tests operational on windows.
#if defined(OS_WIN)
const int kDiagnosticsTestCount = 19;
#elif defined(OS_MACOSX)
const int kDiagnosticsTestCount = 16;
#elif defined(OS_POSIX)
const int kDiagnosticsTestCount = 17;
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
  EXPECT_GT(observer.progress_called(), 0);
  EXPECT_EQ(kDiagnosticsTestCount, model_->GetTestRunCount());
  EXPECT_EQ(kDiagnosticsTestCount, observer.finished());
}
