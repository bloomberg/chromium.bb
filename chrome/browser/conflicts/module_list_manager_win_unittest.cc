// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_list_manager_win.h"

#include "base/test/scoped_task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockObserver : public ModuleListManager::Observer {
 public:
  MockObserver() : called_(false) {}
  ~MockObserver() override = default;

  // ModuleListManager::Observer implementation:
  void OnNewModuleList(const base::Version& version,
                       const base::FilePath& path) override {
    called_ = true;
    version_ = version;
    path_ = path;
  }

  bool called() const { return called_; }
  base::Version version() const { return version_; }
  base::FilePath path() const { return path_; }

 private:
  bool called_;
  base::Version version_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

}  // namespace

// This is not in the anonymous namespace as it is friends with the class being
// tested.
class ModuleListManagerTest : public ::testing::Test {
 public:
  ModuleListManagerTest() = default;
  ~ModuleListManagerTest() override = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  void NotifyNewModuleList(ModuleListManager* mlm) {
    // Add an observer.
    MockObserver observer;
    mlm->SetObserver(&observer);
    ASSERT_FALSE(observer.called());
    ASSERT_FALSE(observer.version().IsValid());
    ASSERT_TRUE(observer.path().empty());

    // Notify the manager of a new module list and expect it to be updated.
    // Also expect the observer to be called.
    base::Version version("1.2.3.4");
    base::FilePath path(L"C:\\foo\\bar.txt");
    mlm->LoadModuleList(version, path);
    EXPECT_EQ(version, mlm->module_list_version());
    EXPECT_EQ(path, mlm->module_list_path());
    EXPECT_TRUE(observer.called());
    EXPECT_EQ(version, observer.version());
    EXPECT_EQ(path, observer.path());
    mlm->SetObserver(nullptr);

    // Expect the registry to be updated too.
    base::win::RegKey reg_key(ModuleListManager::GetRegistryHive(),
                              ModuleListManager::GetRegistryPath().c_str(),
                              KEY_READ);
    ASSERT_TRUE(reg_key.Valid());
    std::wstring s;
    ASSERT_EQ(ERROR_SUCCESS,
              reg_key.ReadValue(ModuleListManager::kModuleListPathKeyName, &s));
    EXPECT_EQ(path.value(), s);
    ASSERT_EQ(
        ERROR_SUCCESS,
        reg_key.ReadValue(ModuleListManager::kModuleListVersionKeyName, &s));
    EXPECT_EQ(L"1.2.3.4", s);
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager registry_override_manager_;

  DISALLOW_COPY_AND_ASSIGN(ModuleListManagerTest);
};

TEST_F(ModuleListManagerTest, FindsNoPathInRegistry) {
  ModuleListManager mlm(base::SequencedTaskRunnerHandle::Get());
  EXPECT_TRUE(mlm.module_list_path().empty());
  EXPECT_FALSE(mlm.module_list_version().IsValid());

  ASSERT_NO_FATAL_FAILURE(NotifyNewModuleList(&mlm));
}

TEST_F(ModuleListManagerTest, FindsInvalidVersionInRegistry) {
  base::win::RegKey reg_key(ModuleListManager::GetRegistryHive(),
                            ModuleListManager::GetRegistryPath().c_str(),
                            KEY_WRITE);
  ASSERT_TRUE(reg_key.Valid());
  ASSERT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(ModuleListManager::kModuleListPathKeyName,
                               L"C:\\foo\\bar.txt"));
  ASSERT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(ModuleListManager::kModuleListVersionKeyName,
                               L"invalid-version-string"));

  ModuleListManager mlm(base::SequencedTaskRunnerHandle::Get());
  EXPECT_TRUE(mlm.module_list_path().empty());
  EXPECT_FALSE(mlm.module_list_version().IsValid());

  ASSERT_NO_FATAL_FAILURE(NotifyNewModuleList(&mlm));
}

TEST_F(ModuleListManagerTest, FindsExistingPathInRegistry) {
  base::win::RegKey reg_key(ModuleListManager::GetRegistryHive(),
                            ModuleListManager::GetRegistryPath().c_str(),
                            KEY_WRITE);
  ASSERT_TRUE(reg_key.Valid());
  ASSERT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(ModuleListManager::kModuleListPathKeyName,
                               L"C:\\foo\\bar.txt"));
  ASSERT_EQ(ERROR_SUCCESS,
            reg_key.WriteValue(ModuleListManager::kModuleListVersionKeyName,
                               L"1.0.0.0"));

  ModuleListManager mlm(base::SequencedTaskRunnerHandle::Get());
  EXPECT_TRUE(mlm.module_list_path().empty());
  EXPECT_FALSE(mlm.module_list_version().IsValid());

  ASSERT_NO_FATAL_FAILURE(NotifyNewModuleList(&mlm));
}
