// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/problematic_programs_updater_win.h"

#include <map>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/conflicts/module_info_win.h"
#include "chrome/browser/conflicts/module_list_filter_win.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Mocks an empty whitelist and blacklist.
class MockModuleListFilter : public ModuleListFilter {
 public:
  MockModuleListFilter() = default;
  ~MockModuleListFilter() override = default;

  bool IsWhitelisted(const ModuleInfoKey& module_key,
                     const ModuleInfoData& module_data) const override {
    return false;
  }

  std::unique_ptr<chrome::conflicts::BlacklistAction> IsBlacklisted(
      const ModuleInfoKey& module_key,
      const ModuleInfoData& module_data) const override {
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModuleListFilter);
};

class MockInstalledPrograms : public InstalledPrograms {
 public:
  MockInstalledPrograms() = default;
  ~MockInstalledPrograms() override = default;

  void AddInstalledProgram(const base::FilePath& file_path,
                           InstalledPrograms::ProgramInfo program_info) {
    programs_.insert({file_path, std::move(program_info)});
  }

  // Given a |file|, checks if it matches with an installed program on the
  // user's machine and returns all the matching programs. Do not call this
  // before the initialization is done.
  // Virtual to allow mocking.
  bool GetInstalledPrograms(const base::FilePath& file,
                            std::vector<ProgramInfo>* programs) const override {
    auto iter = programs_.find(file);
    if (iter == programs_.end())
      return false;

    programs->push_back(iter->second);
    return true;
  }

 private:
  std::map<base::FilePath, ProgramInfo> programs_;

  DISALLOW_COPY_AND_ASSIGN(MockInstalledPrograms);
};

// Returns a new ModuleInfoData marked as loaded into the process but otherwise
// empty.
ModuleInfoData CreateLoadedModuleInfoData() {
  ModuleInfoData module_data;
  module_data.module_types |= ModuleInfoData::kTypeLoadedModule;
  return module_data;
}

constexpr wchar_t kDllPath1[] = L"c:\\path\\to\\module.dll";
constexpr wchar_t kDllPath2[] = L"c:\\some\\shellextension.dll";

}  // namespace

class ProblematicProgramsUpdaterTest : public testing::Test {
 protected:
  ProblematicProgramsUpdaterTest()
      : dll1_(kDllPath1),
        dll2_(kDllPath2),
        scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    scoped_feature_list_.InitAndEnableFeature(kIncompatibleApplicationsWarning);
  }

  enum class Option {
    ADD_REGISTRY_ENTRY,
    NO_REGISTRY_ENTRY,
  };
  void AddProblematicProgram(const base::FilePath& injected_module_path,
                             const base::string16& program_name,
                             Option option) {
    static constexpr wchar_t kUninstallRegKeyFormat[] =
        L"dummy\\uninstall\\%ls";

    const base::string16 registry_key_path =
        base::StringPrintf(kUninstallRegKeyFormat, program_name.c_str());

    installed_programs_.AddInstalledProgram(
        injected_module_path,
        {program_name, HKEY_CURRENT_USER, registry_key_path, KEY_WOW64_32KEY});

    if (option == Option::ADD_REGISTRY_ENTRY) {
      base::win::RegKey reg_key(HKEY_CURRENT_USER, registry_key_path.c_str(),
                                KEY_WOW64_32KEY | KEY_CREATE_SUB_KEY);
    }
  }

  MockModuleListFilter& module_list_filter() { return module_list_filter_; }
  MockInstalledPrograms& installed_programs() { return installed_programs_; }

  const base::FilePath dll1_;
  const base::FilePath dll2_;

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  base::test::ScopedFeatureList scoped_feature_list_;

  ScopedTestingLocalState scoped_testing_local_state_;

  registry_util::RegistryOverrideManager registry_override_manager_;

  MockModuleListFilter module_list_filter_;

  MockInstalledPrograms installed_programs_;

  DISALLOW_COPY_AND_ASSIGN(ProblematicProgramsUpdaterTest);
};

// Tests that when the Local State cache is empty, no problematic programs are
// returned.
TEST_F(ProblematicProgramsUpdaterTest, EmptyCache) {
  EXPECT_FALSE(ProblematicProgramsUpdater::HasCachedPrograms());
  EXPECT_TRUE(ProblematicProgramsUpdater::GetCachedPrograms().empty());
}

// ProblematicProgramsUpdater doesn't do anything when there is no registered
// installed programs.
TEST_F(ProblematicProgramsUpdaterTest, NoProblematicPrograms) {
  auto problematic_programs_updater = ProblematicProgramsUpdater::MaybeCreate(
      module_list_filter(), installed_programs());

  // Simulate some arbitrary module loading into the process.
  problematic_programs_updater->OnNewModuleFound(ModuleInfoKey(dll1_, 0, 0, 0),
                                                 CreateLoadedModuleInfoData());
  problematic_programs_updater->OnModuleDatabaseIdle();

  EXPECT_FALSE(ProblematicProgramsUpdater::HasCachedPrograms());
  EXPECT_TRUE(ProblematicProgramsUpdater::GetCachedPrograms().empty());
}

TEST_F(ProblematicProgramsUpdaterTest, OneConflict) {
  AddProblematicProgram(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);

  auto problematic_programs_updater = ProblematicProgramsUpdater::MaybeCreate(
      module_list_filter(), installed_programs());

  // Simulate the module loading into the process.
  problematic_programs_updater->OnNewModuleFound(ModuleInfoKey(dll1_, 0, 0, 0),
                                                 CreateLoadedModuleInfoData());
  problematic_programs_updater->OnModuleDatabaseIdle();

  EXPECT_TRUE(ProblematicProgramsUpdater::HasCachedPrograms());
  auto program_names = ProblematicProgramsUpdater::GetCachedPrograms();
  ASSERT_EQ(1u, program_names.size());
  EXPECT_EQ(L"Foo", program_names[0].info.name);
}

TEST_F(ProblematicProgramsUpdaterTest, MultipleCallsToOnModuleDatabaseIdle) {
  AddProblematicProgram(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);
  AddProblematicProgram(dll2_, L"Bar", Option::ADD_REGISTRY_ENTRY);

  auto problematic_programs_updater = ProblematicProgramsUpdater::MaybeCreate(
      module_list_filter(), installed_programs());

  // Simulate the module loading into the process.
  problematic_programs_updater->OnNewModuleFound(ModuleInfoKey(dll1_, 0, 0, 0),
                                                 CreateLoadedModuleInfoData());
  problematic_programs_updater->OnModuleDatabaseIdle();

  // Add an additional module.
  problematic_programs_updater->OnNewModuleFound(ModuleInfoKey(dll2_, 0, 0, 0),
                                                 CreateLoadedModuleInfoData());
  problematic_programs_updater->OnModuleDatabaseIdle();

  EXPECT_TRUE(ProblematicProgramsUpdater::HasCachedPrograms());
  auto program_names = ProblematicProgramsUpdater::GetCachedPrograms();
  ASSERT_EQ(2u, program_names.size());
}

// This is meant to test that cached problematic programs are persisted
// through browser restarts, via the Local State file.
//
// Since this isn't really doable in a unit test, this test at least check that
// the list isn't tied to the lifetime of the ProblematicProgramsUpdater
// instance. It is assumed that the Local State file works as intended.
TEST_F(ProblematicProgramsUpdaterTest, PersistsThroughRestarts) {
  AddProblematicProgram(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);

  auto problematic_programs_updater = ProblematicProgramsUpdater::MaybeCreate(
      module_list_filter(), installed_programs());

  // Simulate the module loading into the process.
  problematic_programs_updater->OnNewModuleFound(ModuleInfoKey(dll1_, 0, 0, 0),
                                                 CreateLoadedModuleInfoData());
  problematic_programs_updater->OnModuleDatabaseIdle();

  EXPECT_TRUE(ProblematicProgramsUpdater::HasCachedPrograms());

  // Delete the instance.
  problematic_programs_updater = nullptr;

  EXPECT_TRUE(ProblematicProgramsUpdater::HasCachedPrograms());
}

// Tests that programs that do not have a registry entry are removed.
TEST_F(ProblematicProgramsUpdaterTest, StaleEntriesRemoved) {
  AddProblematicProgram(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);
  AddProblematicProgram(dll2_, L"Bar", Option::NO_REGISTRY_ENTRY);

  auto problematic_programs_updater = ProblematicProgramsUpdater::MaybeCreate(
      module_list_filter(), installed_programs());

  // Simulate the modules loading into the process.
  problematic_programs_updater->OnNewModuleFound(ModuleInfoKey(dll1_, 0, 0, 0),
                                                 CreateLoadedModuleInfoData());
  problematic_programs_updater->OnNewModuleFound(ModuleInfoKey(dll2_, 0, 0, 0),
                                                 CreateLoadedModuleInfoData());
  problematic_programs_updater->OnModuleDatabaseIdle();

  EXPECT_TRUE(ProblematicProgramsUpdater::HasCachedPrograms());
  auto program_names = ProblematicProgramsUpdater::GetCachedPrograms();
  ASSERT_EQ(1u, program_names.size());
  EXPECT_EQ(L"Foo", program_names[0].info.name);
}
