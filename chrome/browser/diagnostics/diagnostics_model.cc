// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_model.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/path_service.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"
#include "chrome/common/chrome_paths.h"

namespace {

// Embodies the commonalities of the model across platforms. It manages the
// list of tests and can loop over them. The main job of the platform specific
// code becomes:
// 1- Inserting the appropiate tests into |tests_|
// 2- Overriding RunTest() to wrap it with the appropiate fatal exception
//    handler for the OS.
// This class owns the all the tests and will only delete them upon
// destruction.
class DiagnosticsModelImpl : public DiagnosticsModel {
 public:
  DiagnosticsModelImpl() : tests_run_(0) {
  }

  ~DiagnosticsModelImpl() {
    STLDeleteElements(&tests_);
  }

  virtual int GetTestRunCount() {
    return tests_run_;
  }

  virtual int GetTestAvailableCount() {
    return tests_.size();
  }

  virtual void RunAll(DiagnosticsModel::Observer* observer) {
    size_t test_count = tests_.size();
    for (size_t ix = 0; ix != test_count; ++ix) {
      bool do_next = RunTest(tests_[ix], observer);
      ++tests_run_;
      if (!do_next)
        break;
    }
    observer->OnDoneAll(this);
  }

  virtual TestInfo& GetTest(size_t id) {
    return *tests_[id];
  }

 protected:
  // Run a particular test. Return false if no other tests should be run.
  virtual bool RunTest(DiagnosticTest* test, Observer* observer) {
    return test->Execute(observer, this);
  }

  typedef std::vector<DiagnosticTest*> TestArray;
  TestArray tests_;
  int tests_run_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsModelImpl);
};

// Simple basic diagnostic test. Check that the user's data directory
// exists. This test works for all platforms.
// TODO(cpu): Move to its final place.
// TODO(cpu): Localize strings.
class UserPathsTest : public DiagnosticTest {
 public:
  UserPathsTest() : DiagnosticTest(ASCIIToUTF16("User data Directory")) {}

  // Not used at the moment but it allows one test to query the status
  // of another test so we can build test dependencies.
  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
    FilePath dir;
    if (!PathService::Get(chrome::DIR_USER_DATA, &dir)) {
      RecordStopFailure(ASCIIToUTF16("Path provider failure"));
      return false;
    }
    if (!file_util::PathExists(dir)) {
      RecordFailure(ASCIIToUTF16("No user data dir found"));
      return true;
    }
    if (!file_util::PathIsWritable(dir)) {
      RecordFailure(ASCIIToUTF16("User data dir is not writable"));
      return true;
    }
    RecordSuccess(ASCIIToUTF16("Directory exists and is writable"));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserPathsTest);
};

// Each platform can have their own tests. For the time being there is only
// one test that works on all platforms.
#if defined(OS_WIN)
class DiagnosticsModelWin : public DiagnosticsModelImpl {
 public:
  DiagnosticsModelWin() {
    tests_.push_back(new UserPathsTest());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsModelWin);
};

#elif defined(OS_MACOSX)
class DiagnosticsModelMac : public DiagnosticsModelImpl {
 public:
  DiagnosticsModelMac() {
    tests_.push_back(new UserPathsTest());
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsModelMac);
};

#elif defined(OS_LINUX)
class DiagnosticsModelLinux : public DiagnosticsModelImpl {
 public:
  DiagnosticsModelLinux() {
    tests_.push_back(new UserPathsTest());
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsModelLinux);
};

#endif

}  // namespace

DiagnosticsModel* MakeDiagnosticsModel() {
#if defined(OS_WIN)
  return new DiagnosticsModelWin();
#elif defined(OS_MACOSX)
  return new DiagnosticsModelMac();
#elif defined(OS_LINUX)
  return new DiagnosticsModelLinux();
#endif
}
