// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/recon_diagnostics.h"

#include <string>

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/string_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/path_service.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"
#include "chrome/browser/platform_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/json_value_serializer.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/enumerate_modules_model_win.h"
#include "chrome/installer/util/install_util.h"
#endif

// Reconnaissance diagnostics. These are the first and most critical
// diagnostic tests. Here we check for the existence of critical files.
// TODO(cpu): Define if it makes sense to localize strings.

// TODO(cpu): There are a few maximum file sizes hardcoded in this file
// that have little or no theoretical or experimental ground. Find a way
// to justify them.

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
    version = base::win::GetVersion();
    if (version < base::win::VERSION_XP) {
      RecordFailure(ASCIIToUTF16("Windows 2000 or earlier"));
      return false;
    }
    base::win::GetServicePackLevel(&major, &minor);
    if ((version == base::win::VERSION_XP) && (major < 2)) {
      RecordFailure(ASCIIToUTF16("XP Service Pack 1 or earlier"));
      return false;
    }
#else
    // TODO(port): define the OS criteria for Linux and Mac.
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

// Check if any conflicting DLLs are loaded.
class ConflictingDllsTest : public DiagnosticTest {
 public:
  ConflictingDllsTest() : DiagnosticTest(ASCIIToUTF16("Conflicting modules")) {}

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
#if defined(OS_WIN)
    EnumerateModulesModel* model = EnumerateModulesModel::GetInstance();
    model->set_limited_mode(true);
    model->ScanNow();
    ListValue* list = model->GetModuleList();
    if (!model->confirmed_bad_modules_detected() &&
        !model->suspected_bad_modules_detected()) {
      RecordSuccess(ASCIIToUTF16("No conflicting modules found"));
      return true;
    }

    string16 failures = ASCIIToUTF16("Possibly conflicting modules:");
    DictionaryValue* dictionary;
    for (size_t i = 0; i < list->GetSize(); ++i) {
      if (!list->GetDictionary(i, &dictionary))
        RecordFailure(ASCIIToUTF16("Dictionary lookup failed"));
      int status;
      string16 location;
      string16 name;
      if (!dictionary->GetInteger("status", &status))
        RecordFailure(ASCIIToUTF16("No 'status' field found"));
      if (status < ModuleEnumerator::SUSPECTED_BAD)
        continue;

      if (!dictionary->GetString("location", &location)) {
        RecordFailure(ASCIIToUTF16("No 'location' field found"));
        return true;
      }
      if (!dictionary->GetString("name", &name)) {
        RecordFailure(ASCIIToUTF16("No 'name' field found"));
        return true;
      }

      failures += ASCIIToUTF16("\n") + location + name;
    }
    RecordFailure(failures);
    return true;
#else
    RecordFailure(ASCIIToUTF16("Not implemented"));
    return true;
#endif  // defined(OS_WIN)
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConflictingDllsTest);
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
    user_level_ = InstallUtil::IsPerUserInstall(chrome_exe.value().c_str());
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
    chrome::VersionInfo version_info;
    if (!version_info.is_valid()) {
      RecordFailure(ASCIIToUTF16("No Version"));
      return true;
    }
    std::string current_version = version_info.Version();
    if (current_version.empty()) {
      RecordFailure(ASCIIToUTF16("Empty Version"));
      return true;
    }
    std::string version_modifier = platform_util::GetVersionStringModifier();
    if (!version_modifier.empty())
      current_version += " " + version_modifier;
#if defined(GOOGLE_CHROME_BUILD)
    current_version += " GCB";
#endif  // defined(GOOGLE_CHROME_BUILD)
    RecordSuccess(ASCIIToUTF16(current_version));
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VersionTest);
};

struct TestPathInfo {
  const char* test_name;
  int  path_id;
  bool is_directory;
  bool is_optional;
  bool test_writable;
  int64 max_size;
};

const int64 kOneKilo = 1024;
const int64 kOneMeg = 1024 * kOneKilo;

const TestPathInfo kPathsToTest[] = {
  {"User data Directory", chrome::DIR_USER_DATA,
      true, false, true, 850 * kOneMeg},
  {"Local state file", chrome::FILE_LOCAL_STATE,
      false, false, true, 500 * kOneKilo},
  {"Dictionaries Directory", chrome::DIR_APP_DICTIONARIES,
      true, true, false, 0},
  {"Inspector Directory", chrome::DIR_INSPECTOR,
      true, false, false, 0}
};

// Check that the user's data directory exists and the paths are writable.
// If it is a systemwide install some paths are not expected to be writable.
// This test depends on |InstallTypeTest| having run successfully.
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
    if (!PathService::Get(path_info_.path_id, &dir_or_file)) {
      RecordStopFailure(ASCIIToUTF16("Path provider failure"));
      return false;
    }
    if (!file_util::PathExists(dir_or_file)) {
      RecordFailure(ASCIIToUTF16("Path not found"));
      return true;
    }

    int64 dir_or_file_size = 0;
    if (path_info_.is_directory) {
      dir_or_file_size = file_util::ComputeDirectorySize(dir_or_file);
    } else {
      file_util::GetFileSize(dir_or_file, &dir_or_file_size);
    }
    if (!dir_or_file_size && !path_info_.is_optional) {
      RecordFailure(ASCIIToUTF16("Cannot obtain size"));
      return true;
    }
    DataUnits units = GetByteDisplayUnits(dir_or_file_size);
    string16 printable_size = FormatBytes(dir_or_file_size, units, true);

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
// lives is not dangerously low.
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
    string16 printable_size = FormatBytes(disk_space, units, true);
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

// Checks that a given json file can be correctly parsed.
class JSONTest : public DiagnosticTest {
 public:
  JSONTest(const FilePath& path, const string16 name, int64 max_file_size)
      : DiagnosticTest(name), path_(path), max_file_size_(max_file_size) {
  }

  virtual int GetId() { return 0; }

  virtual bool ExecuteImpl(DiagnosticsModel::Observer* observer) {
    if (!file_util::PathExists(path_)) {
      RecordFailure(ASCIIToUTF16("File not found"));
      return true;
    }
    int64 file_size;
    if (!file_util::GetFileSize(path_, &file_size)) {
      RecordFailure(ASCIIToUTF16("Cannot obtain file size"));
      return true;
    }

    if (file_size > max_file_size_) {
      RecordFailure(ASCIIToUTF16("File too big"));
      return true;
    }
    // Being small enough, we can process it in-memory.
    std::string json_data;
    if (!file_util::ReadFileToString(path_, &json_data)) {
      RecordFailure(ASCIIToUTF16(
          "Could not open file. Possibly locked by other process"));
      return true;
    }

    JSONStringValueSerializer json(json_data);
    int error_code = base::JSONReader::JSON_NO_ERROR;
    std::string error_message;
    scoped_ptr<Value> json_root(json.Deserialize(&error_code, &error_message));
    if (base::JSONReader::JSON_NO_ERROR != error_code) {
      if (error_message.empty()) {
        error_message = "Parse error " + base::IntToString(error_code);
      }
      RecordFailure(UTF8ToUTF16(error_message));
      return true;
    }

    RecordSuccess(ASCIIToUTF16("File parsed OK"));
    return true;
  }

 private:
  FilePath path_;
  int64 max_file_size_;
  DISALLOW_COPY_AND_ASSIGN(JSONTest);
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

DiagnosticTest* MakeConflictingDllsTest() {
  return new ConflictingDllsTest();
}

DiagnosticTest* MakeInstallTypeTest() {
  return new InstallTypeTest();
}

DiagnosticTest* MakePreferencesTest() {
  FilePath path = DiagnosticTest::GetUserDefaultProfileDir();
  path = path.Append(chrome::kPreferencesFilename);
  return new JSONTest(path, ASCIIToUTF16("Profile JSON"), 100 * kOneKilo);
}

DiagnosticTest* MakeBookMarksTest() {
  FilePath path = DiagnosticTest::GetUserDefaultProfileDir();
  path = path.Append(chrome::kBookmarksFileName);
  return new JSONTest(path, ASCIIToUTF16("BookMarks JSON"), 2 * kOneMeg);
}

DiagnosticTest* MakeLocalStateTest() {
  FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.Append(chrome::kLocalStateFilename);
  return new JSONTest(path, ASCIIToUTF16("Local State JSON"), 50 * kOneKilo);
}
