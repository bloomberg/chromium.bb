// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/installed_programs_win.h"

#include <map>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/conflicts/msi_util_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const wchar_t kRegistryKeyPathFormat[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%ls";

struct CommonInfo {
  base::string16 product_id;
  bool is_system_component;
  bool is_microsoft_published;
  base::string16 display_name;
  base::string16 uninstall_string;
};

struct InstallLocationProgramInfo {
  CommonInfo common_info;
  base::string16 install_location;
};

struct MsiProgramInfo {
  CommonInfo common_info;
  std::vector<base::string16> components;
};

class MockMsiUtil : public MsiUtil {
 public:
  MockMsiUtil(const std::map<base::string16, std::vector<base::string16>>&
                  component_paths_map)
      : component_paths_map_(component_paths_map) {}

  bool GetMsiComponentPaths(
      const base::string16& product_guid,
      std::vector<base::string16>* component_paths) const override {
    auto iter = component_paths_map_.find(product_guid);
    if (iter == component_paths_map_.end())
      return false;

    *component_paths = iter->second;
    return true;
  }

 private:
  const std::map<base::string16, std::vector<base::string16>>&
      component_paths_map_;
};

class TestInstalledPrograms : public InstalledPrograms {
 public:
  explicit TestInstalledPrograms(std::unique_ptr<MsiUtil> msi_util)
      : InstalledPrograms(std::move(msi_util)) {}
};

class InstalledProgramsTest : public testing::Test {
 public:
  InstalledProgramsTest() = default;
  ~InstalledProgramsTest() override = default;

  // ASSERT_NO_FATAL_FAILURE cannot be used in a constructor so the registry
  // hive overrides are done here.
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  void AddCommonInfo(const CommonInfo& common_info,
                     base::win::RegKey* registry_key) {
    registry_key->WriteValue(L"SystemComponent",
                             common_info.is_system_component ? 1 : 0);
    registry_key->WriteValue(L"UninstallString",
                             common_info.uninstall_string.c_str());
    if (common_info.is_microsoft_published)
      registry_key->WriteValue(L"Publisher", L"Microsoft Corporation");
    registry_key->WriteValue(L"DisplayName", common_info.display_name.c_str());
  }

  void AddFakeProgram(const MsiProgramInfo& program_info) {
    const base::string16 registry_key_path = base::StringPrintf(
        kRegistryKeyPathFormat, program_info.common_info.product_id.c_str());
    base::win::RegKey registry_key(HKEY_CURRENT_USER, registry_key_path.c_str(),
                                   KEY_WRITE);

    AddCommonInfo(program_info.common_info, &registry_key);

    component_paths_map_.insert(
        {program_info.common_info.product_id, program_info.components});
  }

  void AddFakeProgram(const InstallLocationProgramInfo& program_info) {
    const base::string16 registry_key_path = base::StringPrintf(
        kRegistryKeyPathFormat, program_info.common_info.product_id.c_str());
    base::win::RegKey registry_key(HKEY_CURRENT_USER, registry_key_path.c_str(),
                                   KEY_WRITE);

    AddCommonInfo(program_info.common_info, &registry_key);

    registry_key.WriteValue(L"InstallLocation",
                            program_info.install_location.c_str());
  }

  TestInstalledPrograms& installed_programs() { return *installed_programs_; }

  void InitializeInstalledPrograms() {
    installed_programs_ = std::make_unique<TestInstalledPrograms>(
        std::make_unique<MockMsiUtil>(component_paths_map_));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;

  std::unique_ptr<TestInstalledPrograms> installed_programs_;

  std::map<base::string16, std::vector<base::string16>> component_paths_map_;

  DISALLOW_COPY_AND_ASSIGN(InstalledProgramsTest);
};

}  // namespace

// Checks that registry entries with invalid information are skipped.
TEST_F(InstalledProgramsTest, InvalidEntries) {
  const wchar_t kValidDisplayName[] = L"ADisplayName";
  const wchar_t kValidUninstallString[] = L"c:\\an\\UninstallString.exe";
  const wchar_t kInstallLocation[] = L"c:\\program files\\program\\";

  InstallLocationProgramInfo kTestCases[] = {
      {
          {
              L"Is SystemComponent", true, false, kValidDisplayName,
              kValidUninstallString,
          },
          kInstallLocation,
      },
      {
          {
              L"Is Microsoft published", false, true, kValidDisplayName,
              kValidUninstallString,
          },
          kInstallLocation,
      },
      {
          {
              L"Missing DisplayName", false, false, L"", kValidUninstallString,
          },
          kInstallLocation,
      },
      {
          {
              L"Missing UninstallString", false, false, kValidDisplayName, L"",
          },
          kInstallLocation,
      },
  };

  for (const auto& test_case : kTestCases)
    AddFakeProgram(test_case);

  InitializeInstalledPrograms();

  // None of the invalid entries were picked up.
  const base::FilePath valid_child_file =
      base::FilePath(kInstallLocation).Append(L"file.dll");
  std::vector<InstalledPrograms::ProgramInfo> programs;
  EXPECT_FALSE(
      installed_programs().GetInstalledPrograms(valid_child_file, &programs));
}

// Tests InstalledPrograms on a valid entry with an InstallLocation.
TEST_F(InstalledProgramsTest, InstallLocation) {
  const wchar_t kValidDisplayName[] = L"ADisplayName";
  const wchar_t kValidUninstallString[] = L"c:\\an\\UninstallString.exe";
  const wchar_t kInstallLocation[] = L"c:\\program files\\program\\";

  InstallLocationProgramInfo kTestCase = {
      {
          L"Completely valid", false, false, kValidDisplayName,
          kValidUninstallString,
      },
      kInstallLocation,
  };

  AddFakeProgram(kTestCase);

  InitializeInstalledPrograms();

  // Child file path.
  const base::FilePath valid_child_file =
      base::FilePath(kInstallLocation).Append(L"file.dll");
  std::vector<InstalledPrograms::ProgramInfo> programs;
  EXPECT_TRUE(
      installed_programs().GetInstalledPrograms(valid_child_file, &programs));
  ASSERT_EQ(1u, programs.size());
  EXPECT_EQ(kTestCase.common_info.display_name, programs[0].name);
  EXPECT_EQ(HKEY_CURRENT_USER, programs[0].registry_root);
  EXPECT_FALSE(programs[0].registry_key_path.empty());
  EXPECT_EQ(0u, programs[0].registry_wow64_access);

  // Non-child file path.
  const base::FilePath invalid_child_file(
      L"c:\\program files\\another program\\test.dll");
  EXPECT_FALSE(
      installed_programs().GetInstalledPrograms(invalid_child_file, &programs));
}

// Tests InstalledPrograms on a valid MSI entry.
TEST_F(InstalledProgramsTest, Msi) {
  const wchar_t kValidDisplayName[] = L"ADisplayName";
  const wchar_t kValidUninstallString[] = L"c:\\an\\UninstallString.exe";

  MsiProgramInfo kTestCase = {
      {
          L"Completely valid", false, false, kValidDisplayName,
          kValidUninstallString,
      },
      {
          L"c:\\program files\\program\\file1.dll",
          L"c:\\program files\\program\\file2.dll",
          L"c:\\program files\\program\\sub\\file3.dll",
          L"c:\\windows\\system32\\file4.dll",
      },
  };

  AddFakeProgram(kTestCase);

  InitializeInstalledPrograms();

  // Checks that all the files match the program.
  for (const auto& component : kTestCase.components) {
    std::vector<InstalledPrograms::ProgramInfo> programs;
    EXPECT_TRUE(installed_programs().GetInstalledPrograms(
        base::FilePath(component), &programs));
    ASSERT_EQ(1u, programs.size());
    EXPECT_EQ(kTestCase.common_info.display_name, programs[0].name);
    EXPECT_EQ(HKEY_CURRENT_USER, programs[0].registry_root);
    EXPECT_FALSE(programs[0].registry_key_path.empty());
    EXPECT_EQ(0u, programs[0].registry_wow64_access);
  }

  // Any other file shouldn't work.
  const base::FilePath invalid_child_file(
      L"c:\\program files\\another program\\test.dll");
  std::vector<InstalledPrograms::ProgramInfo> programs;
  EXPECT_FALSE(
      installed_programs().GetInstalledPrograms(invalid_child_file, &programs));
}

// Checks that if a file matches an InstallLocation and an MSI component, only
// the MSI program will be considered.
TEST_F(InstalledProgramsTest, PrioritizeMsi) {
  const wchar_t kValidUninstallString[] = L"c:\\an\\UninstallString.exe";
  const wchar_t kInstallLocationDisplayName[] = L"InstallLocation DisplayName";
  const wchar_t kMsiDisplayName[] = L"Msi DisplayName";
  const wchar_t kInstallLocation[] = L"c:\\program files\\program\\";
  const wchar_t kMsiComponent[] = L"c:\\program files\\program\\file.dll";

  InstallLocationProgramInfo kInstallLocationFakeProgram = {
      {
          L"GUID1", false, false, kInstallLocationDisplayName,
          kValidUninstallString,
      },
      kInstallLocation,
  };

  MsiProgramInfo kMsiFakeProgram = {
      {
          L"GUID2", false, false, kMsiDisplayName, kValidUninstallString,
      },
      {
          kMsiComponent,
      },
  };

  AddFakeProgram(kInstallLocationFakeProgram);
  AddFakeProgram(kMsiFakeProgram);

  InitializeInstalledPrograms();

  std::vector<InstalledPrograms::ProgramInfo> programs;
  EXPECT_TRUE(installed_programs().GetInstalledPrograms(
      base::FilePath(kMsiComponent), &programs));
  ASSERT_EQ(1u, programs.size());
  EXPECT_NE(kInstallLocationDisplayName, programs[0].name);
  EXPECT_EQ(kMsiDisplayName, programs[0].name);
}

// Tests that if 2 entries with conflicting InstallLocation exist, both are
// ignored.
TEST_F(InstalledProgramsTest, ConflictingInstallLocations) {
  const wchar_t kValidUninstallString[] = L"c:\\an\\UninstallString.exe";
  const wchar_t kDisplayName1[] = L"DisplayName1";
  const wchar_t kDisplayName2[] = L"DisplayName2";
  const wchar_t kInstallLocationParent[] = L"c:\\program files\\company\\";
  const wchar_t kInstallLocationChild[] =
      L"c:\\program files\\company\\program";
  const wchar_t kFile[] = L"c:\\program files\\company\\program\\file.dll";

  InstallLocationProgramInfo kFakeProgram1 = {
      {
          L"GUID1", false, false, kDisplayName1, kValidUninstallString,
      },
      kInstallLocationParent,
  };
  InstallLocationProgramInfo kFakeProgram2 = {
      {
          L"GUID2", false, false, kDisplayName2, kValidUninstallString,
      },
      kInstallLocationChild,
  };

  AddFakeProgram(kFakeProgram1);
  AddFakeProgram(kFakeProgram2);

  InitializeInstalledPrograms();

  std::vector<InstalledPrograms::ProgramInfo> programs;
  EXPECT_FALSE(installed_programs().GetInstalledPrograms(base::FilePath(kFile),
                                                         &programs));
}
