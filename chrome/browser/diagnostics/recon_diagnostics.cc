// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/path_service.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"
#include "chrome/common/chrome_paths.h"
#include "app/app_paths.h"

#if defined(OS_WIN)
#include "base/win_util.h"
#include "chrome/installer/util/install_util.h"
#endif

// Reconnaissance diagnostics. These are the first and most critical
// diagnostic tests. Here we check for the existence of critical files.
// TODO(cpu): Define if it makes sense to localize strings.

namespace {

class InstallTypeTest;
InstallTypeTest* g_install_type = 0;

#if defined(OS_WIN)
// Check that we know what flavor of windows is and that is not an
// unsuported operating system.
class WinOSIdTest : public DiagnosticTest {
 public:
  WinOSIdTest() : DiagnosticTest(ASCIIToUTF16("Operating System")) {}

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
    win_util::WinVersion version = win_util::GetWinVersion();
    if (version < win_util::WINVERSION_XP) {
      RecordFailure(ASCIIToUTF16("Windows 2000 or earlier"));
      return false;
    }
    int major = 0;
    int minor = 0;
    string16 os_name;
    win_util::GetServicePackLevel(&major, &minor);
    if ((version == win_util::WINVERSION_XP) && (major < 2)) {
      RecordFailure(ASCIIToUTF16("XP Service Pack 1 or earlier"));
      return false;
    }
    RecordSuccess(ASCIIToUTF16(StringPrintf("Windows %d [%d:%d]",
                                            version, major, minor)));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WinOSIdTest);
};

// Check if it is system install or per-user install.
class InstallTypeTest : public DiagnosticTest {
 public:
  InstallTypeTest() : DiagnosticTest(ASCIIToUTF16("Install Type")),
                      user_level_(false) {}

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
    FilePath chrome_exe;
    if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
      RecordFailure(ASCIIToUTF16("Path provider failure"));
      return false;
    }
    user_level_ = InstallUtil::IsPerUserInstall(
        chrome_exe.ToWStringHack().c_str());
    string16 install_type(ASCIIToUTF16(user_level_ ? "User Level" :
                                                     "System Level"));
    RecordSuccess(install_type);
    g_install_type = this;
    return true;
  }

  bool system_level() const { return !user_level_; }

 private:
  bool user_level_;
  DISALLOW_COPY_AND_ASSIGN(InstallTypeTest);
};

#else
// For Mac and Linux we just assume is a system level install.
class InstallTypeTest : public DiagnosticTest {
 public:
  InstallTypeTest() : DiagnosticTest(ASCIIToUTF16("Install Type")) {}

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
    g_install_type = this;
    RecordSuccess(ASCIIToUTF16("OK"));
    return true;
  }

  bool system_level() const { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallTypeTest);
};
#endif  // defined(OS_WIN)

struct TestPathInfo {
  const char* test_name;
  int dir_id;
  bool test_writable;
};

const TestPathInfo kPathsToTest[] = {
  {"User data Directory", chrome::DIR_USER_DATA, true},
  {"Local state file", chrome::FILE_LOCAL_STATE, true},
  {"Resources module", chrome::FILE_RESOURCE_MODULE, false},
  {"Dictionaries Directory", chrome::DIR_APP_DICTIONARIES, false},
  {"Inspector Directory", chrome::DIR_INSPECTOR, false}
};

// Check that the user's data directory exists and the paths are writeable.
// If it is a systemwide install some paths are not expected to be writeable.
// This test depends on |InstallTypeTest| having run succesfuly.
class PathTest : public DiagnosticTest {
 public:
  explicit PathTest(const TestPathInfo& path_info)
      : DiagnosticTest(ASCIIToUTF16(path_info.test_name)),
        path_info_(path_info) {}

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
    if (!g_install_type) {
      RecordStopFailure(ASCIIToUTF16("dependency failure"));
      return false;
    }
    FilePath dir;
    if (!PathService::Get(path_info_.dir_id, &dir)) {
      RecordStopFailure(ASCIIToUTF16("Path provider failure"));
      return false;
    }
    if (!file_util::PathExists(dir)) {
      RecordFailure(ASCIIToUTF16("Path not found"));
      return true;
    }
    if (g_install_type->system_level() && !path_info_.test_writable) {
      RecordSuccess(ASCIIToUTF16("Path exists"));
      return true;
    }
    if (!file_util::PathIsWritable(dir)) {
      RecordFailure(ASCIIToUTF16("Path is not writable"));
      return true;
    }
    RecordSuccess(ASCIIToUTF16("Path exists and is writable"));
    return true;
  }

 private:
  TestPathInfo path_info_;
  DISALLOW_COPY_AND_ASSIGN(PathTest);
};

}  // namespace

DiagnosticTest* MakeUserDirTest() {
  return new PathTest(kPathsToTest[0]);
}

DiagnosticTest* MakeLocalStateFileTest() {
  return new PathTest(kPathsToTest[1]);
}

DiagnosticTest* MakeResourceFileTest() {
  return new PathTest(kPathsToTest[2]);
}

DiagnosticTest* MakeDictonaryDirTest() {
  return new PathTest(kPathsToTest[3]);
}

DiagnosticTest* MakeInspectorDirTest() {
  return new PathTest(kPathsToTest[4]);
}

#if defined(OS_WIN)
DiagnosticTest* MakeWinOsIdTest() {
  return new WinOSIdTest();
}
#endif  // defined(OS_WIN)

DiagnosticTest* MakeInstallTypeTest() {
  return new InstallTypeTest();
}

