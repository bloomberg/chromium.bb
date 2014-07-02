// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_model.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"
#include "chrome/browser/diagnostics/recon_diagnostics.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"

namespace diagnostics {

// This is the count of diagnostic tests on each platform.  This should
// only be used by testing code.
#if defined(OS_WIN)
const int DiagnosticsModel::kDiagnosticsTestCount = 17;
#elif defined(OS_MACOSX)
const int DiagnosticsModel::kDiagnosticsTestCount = 13;
#elif defined(OS_POSIX)
#if defined(OS_CHROMEOS)
const int DiagnosticsModel::kDiagnosticsTestCount = 17;
#else
const int DiagnosticsModel::kDiagnosticsTestCount = 15;
#endif
#endif

namespace {

// Embodies the commonalities of the model across platforms. It manages the
// list of tests and can loop over them. The main job of the platform specific
// code becomes:
// 1- Inserting the appropriate tests into |tests_|
// 2- Overriding RunTest() to wrap it with the appropriate fatal exception
//    handler for the OS.
// This class owns the all the tests and will only delete them upon
// destruction.
class DiagnosticsModelImpl : public DiagnosticsModel {
 public:
  DiagnosticsModelImpl() : tests_run_(0) {}

  virtual ~DiagnosticsModelImpl() { STLDeleteElements(&tests_); }

  virtual int GetTestRunCount() const OVERRIDE { return tests_run_; }

  virtual int GetTestAvailableCount() const OVERRIDE { return tests_.size(); }

  virtual void RunAll(DiagnosticsModel::Observer* observer) OVERRIDE {
    size_t test_count = tests_.size();
    bool continue_running = true;
    for (size_t i = 0; i != test_count; ++i) {
      // If one of the diagnostic steps returns false, we want to
      // mark the rest of them as "skipped" in the UMA stats.
      if (continue_running) {
        continue_running = RunTest(tests_[i], observer, i);
        ++tests_run_;
      } else {
#if defined(OS_CHROMEOS)  // Only collecting UMA stats on ChromeOS
        RecordUMATestResult(static_cast<DiagnosticsTestId>(tests_[i]->GetId()),
                            RESULT_SKIPPED);
#else
        // On other platforms, we can just bail out if a diagnostic step returns
        // false.
        break;
#endif
      }
    }
    if (observer)
      observer->OnAllTestsDone(this);
  }

  virtual void RecoverAll(DiagnosticsModel::Observer* observer) OVERRIDE {
    size_t test_count = tests_.size();
    bool continue_running = true;
    for (size_t i = 0; i != test_count; ++i) {
      // If one of the recovery steps returns false, we want to
      // mark the rest of them as "skipped" in the UMA stats.
      if (continue_running) {
        continue_running = RunRecovery(tests_[i], observer, i);
      } else {
#if defined(OS_CHROMEOS)  // Only collecting UMA stats on ChromeOS
        RecordUMARecoveryResult(
            static_cast<DiagnosticsTestId>(tests_[i]->GetId()), RESULT_SKIPPED);
#else
        // On other platforms, we can just bail out if a recovery step returns
        // false.
        break;
#endif
      }
    }
    if (observer)
      observer->OnAllRecoveryDone(this);
  }

  virtual const TestInfo& GetTest(size_t index) const OVERRIDE {
    return *tests_[index];
  }

  virtual bool GetTestInfo(int id, const TestInfo** result) const OVERRIDE {
    DCHECK(id < DIAGNOSTICS_TEST_ID_COUNT);
    DCHECK(id >= 0);
    for (size_t i = 0; i < tests_.size(); i++) {
      if (tests_[i]->GetId() == id) {
        *result = tests_[i];
        return true;
      }
    }
    return false;
  }

 protected:
  // Run a particular diagnostic test. Return false if no other tests should be
  // run.
  virtual bool RunTest(DiagnosticsTest* test,
                       Observer* observer,
                       size_t index) {
    return test->Execute(observer, this, index);
  }

  // Recover from a particular diagnostic test. Return false if no further
  // recovery should be run.
  virtual bool RunRecovery(DiagnosticsTest* test,
                           Observer* observer,
                           size_t index) {
    return test->Recover(observer, this, index);
  }

  typedef std::vector<DiagnosticsTest*> TestArray;
  TestArray tests_;
  int tests_run_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsModelImpl);
};

// Each platform can have their own tests. For the time being there is only
// one test that works on all platforms.
#if defined(OS_WIN)
class DiagnosticsModelWin : public DiagnosticsModelImpl {
 public:
  DiagnosticsModelWin() {
    tests_.push_back(MakeOperatingSystemTest());
    tests_.push_back(MakeConflictingDllsTest());
    tests_.push_back(MakeInstallTypeTest());
    tests_.push_back(MakeVersionTest());
    tests_.push_back(MakeUserDirTest());
    tests_.push_back(MakeLocalStateFileTest());
    tests_.push_back(MakeDictonaryDirTest());
    tests_.push_back(MakeResourcesFileTest());
    tests_.push_back(MakeDiskSpaceTest());
    tests_.push_back(MakePreferencesTest());
    tests_.push_back(MakeLocalStateTest());
    tests_.push_back(MakeBookMarksTest());
    tests_.push_back(MakeSqliteWebDataDbTest());
    tests_.push_back(MakeSqliteCookiesDbTest());
    tests_.push_back(MakeSqliteHistoryDbTest());
    tests_.push_back(MakeSqliteThumbnailsDbTest());
    tests_.push_back(MakeSqliteWebDatabaseTrackerDbTest());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsModelWin);
};

#elif defined(OS_MACOSX)
class DiagnosticsModelMac : public DiagnosticsModelImpl {
 public:
  DiagnosticsModelMac() {
    tests_.push_back(MakeInstallTypeTest());
    tests_.push_back(MakeUserDirTest());
    tests_.push_back(MakeLocalStateFileTest());
    tests_.push_back(MakeDictonaryDirTest());
    tests_.push_back(MakeDiskSpaceTest());
    tests_.push_back(MakePreferencesTest());
    tests_.push_back(MakeLocalStateTest());
    tests_.push_back(MakeBookMarksTest());
    tests_.push_back(MakeSqliteWebDataDbTest());
    tests_.push_back(MakeSqliteCookiesDbTest());
    tests_.push_back(MakeSqliteHistoryDbTest());
    tests_.push_back(MakeSqliteThumbnailsDbTest());
    tests_.push_back(MakeSqliteWebDatabaseTrackerDbTest());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsModelMac);
};

#elif defined(OS_POSIX)
class DiagnosticsModelPosix : public DiagnosticsModelImpl {
 public:
  DiagnosticsModelPosix() {
    tests_.push_back(MakeInstallTypeTest());
    tests_.push_back(MakeVersionTest());
    tests_.push_back(MakeUserDirTest());
    tests_.push_back(MakeLocalStateFileTest());
    tests_.push_back(MakeDictonaryDirTest());
    tests_.push_back(MakeResourcesFileTest());
    tests_.push_back(MakeDiskSpaceTest());
    tests_.push_back(MakePreferencesTest());
    tests_.push_back(MakeLocalStateTest());
    tests_.push_back(MakeBookMarksTest());
    tests_.push_back(MakeSqliteWebDataDbTest());
    tests_.push_back(MakeSqliteCookiesDbTest());
    tests_.push_back(MakeSqliteHistoryDbTest());
    tests_.push_back(MakeSqliteThumbnailsDbTest());
    tests_.push_back(MakeSqliteWebDatabaseTrackerDbTest());
#if defined(OS_CHROMEOS)
    tests_.push_back(MakeSqliteNssCertDbTest());
    tests_.push_back(MakeSqliteNssKeyDbTest());
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsModelPosix);
};

#endif

}  // namespace

DiagnosticsModel* MakeDiagnosticsModel(const CommandLine& cmdline) {
  base::FilePath user_data_dir =
      cmdline.GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty())
    PathService::Override(chrome::DIR_USER_DATA, user_data_dir);
#if defined(OS_WIN)
  return new DiagnosticsModelWin();
#elif defined(OS_MACOSX)
  return new DiagnosticsModelMac();
#elif defined(OS_POSIX)
  return new DiagnosticsModelPosix();
#endif
}

}  // namespace diagnostics
