// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_MODEL_H_
#define CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_MODEL_H_

#include <string>
#include "base/time/time.h"

class CommandLine;

namespace diagnostics {

// The chrome diagnostics system is a model-view-controller system. The Model
// responsible for holding and running the individual tests and providing a
// uniform interface for querying the outcome.
class DiagnosticsModel {
 public:
  // A particular test can be in one of the following states.
  enum TestResult {
    TEST_NOT_RUN,
    TEST_RUNNING,
    TEST_OK,
    TEST_FAIL_CONTINUE,
    TEST_FAIL_STOP,
  };

  // Observer derived form this class which provides a way to be notified of
  // changes to the model as the tests are run. For all the callbacks |id|
  // is the index of the test in question and information can be obtained by
  // calling model->GetTest(id).
  class Observer {
   public:
    virtual ~Observer() {}
    // Called when a test has finished regardless of outcome.
    virtual void OnFinished(int index, DiagnosticsModel* model) = 0;
    // Called once all the test are run.
    virtual void OnDoneAll(DiagnosticsModel* model) = 0;
  };

  // Encapsulates what you can know about a given test.
  class TestInfo {
   public:
    virtual ~TestInfo() {}
    // A parse-able ASCII string that indicates what is being tested.
    virtual std::string GetId() const = 0;
    // A human readable string that tells you what is being tested.
    // This is not localized: it is only meant for developer consumption.
    virtual std::string GetTitle() const = 0;
    // The result of running the test. If called before the test is ran the
    // answer is TEST_NOT_RUN.
    virtual TestResult GetResult() const = 0;
    // A human readable string that tells you more about what happened. If
    // called before the test is run it returns the empty string.
    // This is not localized: it is only meant for developer consumption.
    virtual std::string GetAdditionalInfo() const = 0;
    // A test-specific code representing what happened. If called before the
    // test is run, it should return -1.
    virtual int GetOutcomeCode() const = 0;
    // Returns the system time when the test was performed.
    virtual base::Time GetStartTime() const = 0;
    // Returns the system time when the test was finished.
    virtual base::Time GetEndTime() const = 0;
  };

  virtual ~DiagnosticsModel() {}
  // Returns how many tests have been run.
  virtual int GetTestRunCount() const = 0;
  // Returns how many tests are available. This value never changes.
  virtual int GetTestAvailableCount() const = 0;
  // Runs all the available tests, the |observer| callbacks will be called as
  // the diagnostics progress. |observer| maybe NULL if no observation is
  // needed.
  virtual void RunAll(DiagnosticsModel::Observer* observer) = 0;
  // Get the information for a particular test. Lifetime of returned object is
  // limited to the lifetime of this model.
  virtual const TestInfo& GetTest(size_t index) = 0;
};

// The factory for the model. The main purpose is to hide the creation of
// different models for different platforms.
DiagnosticsModel* MakeDiagnosticsModel(const CommandLine& cmdline);

}  // namespace diagnostics

#endif  // CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_MODEL_H_
