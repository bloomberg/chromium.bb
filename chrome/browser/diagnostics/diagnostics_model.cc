// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_model.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/path_service.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"
#include "chrome/browser/diagnostics/recon_diagnostics.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"

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
      bool do_next = RunTest(tests_[ix], observer, ix);
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
  virtual bool RunTest(DiagnosticTest* test, Observer* observer, size_t index) {
    return test->Execute(observer, this, index);
  }

  typedef std::vector<DiagnosticTest*> TestArray;
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
    tests_.push_back(MakeSqliteWebDbTest());
    tests_.push_back(MakeSqliteCookiesDbTest());
    tests_.push_back(MakeSqliteHistoryDbTest());
    tests_.push_back(MakeSqliteArchivedHistoryDbTest());
    tests_.push_back(MakeSqliteThumbnailsDbTest());
    tests_.push_back(MakeSqliteAppCacheDbTest());
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
    tests_.push_back(MakeResourcesFileTest());
    tests_.push_back(MakeDiskSpaceTest());
    tests_.push_back(MakePreferencesTest());
    tests_.push_back(MakeLocalStateTest());
    tests_.push_back(MakeBookMarksTest());
    tests_.push_back(MakeSqliteWebDbTest());
    tests_.push_back(MakeSqliteCookiesDbTest());
    tests_.push_back(MakeSqliteHistoryDbTest());
    tests_.push_back(MakeSqliteArchivedHistoryDbTest());
    tests_.push_back(MakeSqliteThumbnailsDbTest());
    tests_.push_back(MakeSqliteAppCacheDbTest());
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
    tests_.push_back(MakeSqliteWebDbTest());
    tests_.push_back(MakeSqliteCookiesDbTest());
    tests_.push_back(MakeSqliteHistoryDbTest());
    tests_.push_back(MakeSqliteArchivedHistoryDbTest());
    tests_.push_back(MakeSqliteThumbnailsDbTest());
    tests_.push_back(MakeSqliteAppCacheDbTest());
    tests_.push_back(MakeSqliteWebDatabaseTrackerDbTest());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsModelPosix);
};

#endif

}  // namespace

DiagnosticsModel* MakeDiagnosticsModel(const CommandLine& cmdline) {
  FilePath user_data_dir = cmdline.GetSwitchValuePath(switches::kUserDataDir);
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
