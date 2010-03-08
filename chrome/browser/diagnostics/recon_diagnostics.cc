// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/recon_diagnostics.h"

#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/path_service.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/platform_util.h"
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

// Check that the flavor of the operating system is supported.
class OperatingSystemTest : public DiagnosticTest {
 public:
  OperatingSystemTest() : DiagnosticTest(ASCIIToUTF16("Operating System")) {}

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
    int version = 0;
    int major = 0;
    int minor = 0;
#if defined(OS_WIN)
    version = win_util::GetWinVersion();
    if (version < win_util::WINVERSION_XP) {
      RecordFailure(ASCIIToUTF16("Windows 2000 or earlier"));
      return false;
    }
    win_util::GetServicePackLevel(&major, &minor);
    if ((version == win_util::WINVERSION_XP) && (major < 2)) {
      RecordFailure(ASCIIToUTF16("XP Service Pack 1 or earlier"));
      return false;
    }
#else
    // TODO(port): define the OS criteria for linux and mac.
#endif  // defined(OS_WIN)
    RecordSuccess(ASCIIToUTF16(StringPrintf("%s %s (%d [%d:%d])",
        base::SysInfo::OperatingSystemName().c_str(),
        base::SysInfo::OperatingSystemVersion().c_str(),
        version, major, minor)));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OperatingSystemTest);
};

// Check if it is system install or per-user install.
class InstallTypeTest : public DiagnosticTest {
 public:
  InstallTypeTest() : DiagnosticTest(ASCIIToUTF16("Install Type")),
                      user_level_(false) {}

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
#if defined(OS_WIN)
    FilePath chrome_exe;
    if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
      RecordFailure(ASCIIToUTF16("Path provider failure"));
      return false;
    }
    user_level_ = InstallUtil::IsPerUserInstall(
        chrome_exe.ToWStringHack().c_str());
    const char* type = user_level_ ? "User Level" : "System Level";
    string16 install_type(ASCIIToUTF16(type));
#else
    string16 install_type(ASCIIToUTF16("System Level"));
#endif  // defined(OS_WIN)
    RecordSuccess(install_type);
    g_install_type = this;
    return true;
  }

  bool system_level() const { return !user_level_; }

 private:
  bool user_level_;
  DISALLOW_COPY_AND_ASSIGN(InstallTypeTest);
};

// Check the version of Chrome.
class VersionTest : public DiagnosticTest {
 public:
  VersionTest() : DiagnosticTest(ASCIIToUTF16("Browser Version")) {}

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
    scoped_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfoForCurrentModule());
    if (!version_info.get()) {
      RecordFailure(ASCIIToUTF16("No Version"));
      return true;
    }
    string16 current_version = WideToUTF16(version_info->file_version());
    if (current_version.empty()) {
      RecordFailure(ASCIIToUTF16("Empty Version"));
      return true;
    }
    string16 version_modifier = platform_util::GetVersionStringModifier();
    if (!version_modifier.empty()) {
      current_version += ASCIIToUTF16(" ");
      current_version += version_modifier;
    }
#if defined(GOOGLE_CHROME_BUILD)
    current_version += ASCIIToUTF16(" GCB");
#endif  // defined(GOOGLE_CHROME_BUILD)
    RecordSuccess(current_version);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VersionTest);
};

struct TestPathInfo {
  const char* test_name;
  int dir_id;
  bool test_writable;
  int64 max_size;
};

const int64 kOneKilo = 1024;
const int64 kOneMeg = 1024 * kOneKilo;

const TestPathInfo kPathsToTest[] = {
  {"User data Directory", chrome::DIR_USER_DATA, true, 250 * kOneMeg},
  {"Local state file", chrome::FILE_LOCAL_STATE, true, 200 * kOneKilo},
  {"Dictionaries Directory", chrome::DIR_APP_DICTIONARIES, false, 0},
  {"Inspector Directory", chrome::DIR_INSPECTOR, false, 0}
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
    FilePath dir_or_file;
    if (!PathService::Get(path_info_.dir_id, &dir_or_file)) {
      RecordStopFailure(ASCIIToUTF16("Path provider failure"));
      return false;
    }
    if (!file_util::PathExists(dir_or_file)) {
      RecordFailure(ASCIIToUTF16("Path not found"));
      return true;
    }

    int64 dir_or_file_size;
    if (!file_util::GetFileSize(dir_or_file, &dir_or_file_size)) {
      RecordFailure(ASCIIToUTF16("Cannot obtain size"));
      return true;
    }
    DataUnits units = GetByteDisplayUnits(dir_or_file_size);
    string16 printable_size =
        WideToUTF16(FormatBytes(dir_or_file_size, units, true));

    if (path_info_.max_size > 0) {
      if (dir_or_file_size > path_info_.max_size) {
        RecordFailure(ASCIIToUTF16("Path is too big: ") + printable_size);
        return true;
      }
    }
    if (g_install_type->system_level() && !path_info_.test_writable) {
      RecordSuccess(ASCIIToUTF16("Path exists"));
      return true;
    }
    if (!file_util::PathIsWritable(dir_or_file)) {
      RecordFailure(ASCIIToUTF16("Path is not writable"));
      return true;
    }
    RecordSuccess(ASCIIToUTF16("Path exists and is writable: ")
                  + printable_size);
    return true;
  }

 private:
  TestPathInfo path_info_;
  DISALLOW_COPY_AND_ASSIGN(PathTest);
};

// Check that the disk space in the volume where the user data dir normally
// lives is not dangerosly low.
class DiskSpaceTest : public DiagnosticTest {
 public:
  DiskSpaceTest() : DiagnosticTest(ASCIIToUTF16("Disk Space")) {}

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
    FilePath data_dir;
    if (!PathService::Get(chrome::DIR_USER_DATA, &data_dir))
      return false;
    int64 disk_space = base::SysInfo::AmountOfFreeDiskSpace(data_dir);
    if (disk_space < 0) {
      RecordFailure(ASCIIToUTF16("Unable to query free space"));
      return true;
    }
    DataUnits units = GetByteDisplayUnits(disk_space);
    string16 printable_size =
        WideToUTF16(FormatBytes(disk_space, units, true));
    if (disk_space < 80 * kOneMeg) {
      RecordFailure(ASCIIToUTF16("Low disk space : ") + printable_size);
      return true;
    }
    RecordSuccess(ASCIIToUTF16("Free space : ") + printable_size);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DiskSpaceTest);
};

}  // namespace

DiagnosticTest* MakeUserDirTest() {
  return new PathTest(kPathsToTest[0]);
}

DiagnosticTest* MakeLocalStateFileTest() {
  return new PathTest(kPathsToTest[1]);
}

DiagnosticTest* MakeDictonaryDirTest() {
  return new PathTest(kPathsToTest[2]);
}

DiagnosticTest* MakeInspectorDirTest() {
  return new PathTest(kPathsToTest[3]);
}

DiagnosticTest* MakeVersionTest() {
  return new VersionTest();
}

DiagnosticTest* MakeDiskSpaceTest() {
  return new DiskSpaceTest();
}

DiagnosticTest* MakeOperatingSystemTest() {
  return new OperatingSystemTest();
}

DiagnosticTest* MakeInstallTypeTest() {
  return new InstallTypeTest();
}

