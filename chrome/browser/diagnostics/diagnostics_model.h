// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_MODEL_H_
#define CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_MODEL_H_
#pragma once

#include "base/string16.h"

class CommandLine;

// The chrome diagnostics system is a model-view-controller system. The Model
// responsible for holding and running the individual tests and providing a
// uniform interface for querying the outcome.
// TODO(cpu): The view and the controller are not yet built.
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
    // Called once upon test start with |percent| = 0 and periodically as the
    // test progresses. There is no cancellation method.
    virtual void OnProgress(int id, int percent, DiagnosticsModel* model) = 0;
    // Called if the test in question cannot be run.
    virtual void OnSkipped(int id, DiagnosticsModel* model) = 0;
    // Called when the test has finished regardless of outcome.
    virtual void OnFinished(int id, DiagnosticsModel* model) = 0;
    // Called once all the test are run.
    virtual void OnDoneAll(DiagnosticsModel* model) = 0;
  };

  // Encapsulates what you can know about a given test.
  class TestInfo {
   public:
    virtual ~TestInfo() {}
    // A human readable, localized string that tells you what is being tested.
    virtual string16 GetTitle() = 0;
    // The result of running the test. If called before the test is ran the
    // answer is TEST_NOT_RUN.
    virtual TestResult GetResult() = 0;
    // A human readable, localized string that tells you what happened. If
    // called before the test is run it returns the empty string.
    virtual string16 GetAdditionalInfo() = 0;
  };

  virtual ~DiagnosticsModel() {}
  // Returns how many tests have been run.
  virtual int GetTestRunCount() = 0;
  // Returns how many tests are available. This value never changes.
  virtual int GetTestAvailableCount() =0;
  // Runs all the available tests, the |observer| callbacks will be called as
  // the test progress and thus cannot be null.
  virtual void RunAll(DiagnosticsModel::Observer* observer) = 0;
  // Get the information for a particular test. Do not keep a pointer to the
  // returned object.
  virtual TestInfo& GetTest(size_t id) = 0;
};

// The factory for the model. The main purpose is to hide the creation of
// different models for different platforms.
DiagnosticsModel* MakeDiagnosticsModel(const CommandLine& cmdline);


#endif  // CHROME_BROWSER_DIAGNOSTICS_DIAGNOSTICS_MODEL_H_
