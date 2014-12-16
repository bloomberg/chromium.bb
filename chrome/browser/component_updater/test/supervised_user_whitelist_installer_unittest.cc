// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/component_updater_utils.h"
#include "components/crx_file/id_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

const char kCrxId[] = "abcdefghijklmnopponmlkjihgfedcba";
const char kName[] = "Some Whitelist";
const char kVersion[] = "1.2.3.4";
const char kWhitelistContents[] = "{\"foo\": \"bar\"}";
const char kWhitelistFile[] = "whitelist.json";

std::string CrxIdToHashToCrxId(const std::string& kCrxId) {
  CrxComponent component;
  component.pk_hash =
      SupervisedUserWhitelistInstaller::GetHashFromCrxId(kCrxId);
  EXPECT_EQ(16u, component.pk_hash.size());
  return GetCrxComponentID(component);
}

class MockOnDemandUpdater : public OnDemandUpdater {
 public:
  MOCK_METHOD1(OnDemandUpdate,
               ComponentUpdateService::Status(const std::string&));
};

class MockComponentUpdateService : public ComponentUpdateService {
 public:
  MockComponentUpdateService(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : task_runner_(task_runner) {}

  ~MockComponentUpdateService() override {
    if (component_)
      delete component_->installer;
  }

  MockOnDemandUpdater& on_demand_updater() { return on_demand_updater_; }

  const CrxComponent* registered_component() { return component_.get(); }

  void set_registration_callback(const base::Closure& registration_callback) {
    registration_callback_ = registration_callback;
  }

  // ComponentUpdateService implementation:
  void AddObserver(Observer* observer) override { ADD_FAILURE(); }
  void RemoveObserver(Observer* observer) override { ADD_FAILURE(); }

  Status Start() override {
    ADD_FAILURE();
    return kError;
  }

  Status Stop() override {
    ADD_FAILURE();
    return kError;
  }

  std::vector<std::string> GetComponentIDs() const override {
    ADD_FAILURE();
    return std::vector<std::string>();
  }

  Status RegisterComponent(const CrxComponent& component) override {
    EXPECT_EQ(nullptr, component_.get());
    component_.reset(new CrxComponent(component));
    if (!registration_callback_.is_null())
      registration_callback_.Run();

    return kOk;
  }

  OnDemandUpdater& GetOnDemandUpdater() override { return on_demand_updater_; }

  void MaybeThrottle(const std::string& kCrxId,
                     const base::Closure& callback) override {
    ADD_FAILURE();
  }

  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() override {
    return task_runner_;
  }

  bool GetComponentDetails(const std::string& component_id,
                           CrxUpdateItem* item) const override {
    ADD_FAILURE();
    return false;
  }

 private:
  testing::StrictMock<MockOnDemandUpdater> on_demand_updater_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<CrxComponent> component_;
  base::Closure registration_callback_;
};

void OnWhitelistReady(const base::Closure& callback,
                      const base::FilePath& expected_whitelist_file,
                      const base::FilePath& actual_whitelist_file) {
  EXPECT_EQ(expected_whitelist_file.value(), actual_whitelist_file.value());
  callback.Run();
}

}  // namespace

class SupervisedUserWhitelistInstallerTest : public testing::Test {
 public:
  SupervisedUserWhitelistInstallerTest()
      : path_override_(DIR_SUPERVISED_USER_WHITELISTS),
        component_update_service_(message_loop_.task_runner()),
        installer_(SupervisedUserWhitelistInstaller::Create(
            &component_update_service_)) {}
  ~SupervisedUserWhitelistInstallerTest() override {}

  void SetUp() override {
    base::FilePath whitelist_base_directory;
    ASSERT_TRUE(PathService::Get(DIR_SUPERVISED_USER_WHITELISTS,
                                 &whitelist_base_directory));
    whitelist_directory_ = whitelist_base_directory.AppendASCII(kVersion);
    whitelist_path_ = whitelist_directory_.AppendASCII(kWhitelistFile);

    scoped_ptr<base::DictionaryValue> contentPackDict(
        new base::DictionaryValue);
    contentPackDict->SetString("sites", kWhitelistFile);
    manifest_.Set("content_pack", contentPackDict.release());
    manifest_.SetString("version", kVersion);
  }

 protected:
  void LoadWhitelist(bool new_installation,
                     const base::Closure& whitelist_ready_callback) {
    installer_->RegisterWhitelist(
        kCrxId, kName, new_installation,
        base::Bind(&OnWhitelistReady, whitelist_ready_callback,
                   whitelist_path_));
  }

  void PrepareWhitelistDirectory(const base::FilePath& whitelist_directory) {
    base::FilePath whitelist_path =
        whitelist_directory.AppendASCII(kWhitelistFile);
    size_t whitelist_contents_length = sizeof(kWhitelistContents) - 1;
    ASSERT_EQ(static_cast<int>(whitelist_contents_length),
              base::WriteFile(whitelist_path, kWhitelistContents,
                              whitelist_contents_length));
    base::FilePath manifest_file =
        whitelist_directory.AppendASCII("manifest.json");
    ASSERT_TRUE(JSONFileValueSerializer(manifest_file).Serialize(manifest_));
  }

  void CheckRegisteredComponent(const char* version) {
    const CrxComponent* component =
        component_update_service_.registered_component();
    ASSERT_TRUE(component);
    EXPECT_EQ(kName, component->name);
    EXPECT_EQ(kCrxId, GetCrxComponentID(*component));
    EXPECT_EQ(version, component->version.GetString());
  }

  base::MessageLoop message_loop_;
  base::ScopedPathOverride path_override_;
  MockComponentUpdateService component_update_service_;
  scoped_ptr<SupervisedUserWhitelistInstaller> installer_;
  base::FilePath whitelist_directory_;
  base::FilePath whitelist_path_;
  base::DictionaryValue manifest_;
};

TEST_F(SupervisedUserWhitelistInstallerTest, GetHashFromCrxId) {
  {
    std::string extension_id = "abcdefghijklmnopponmlkjihgfedcba";
    ASSERT_EQ(extension_id, CrxIdToHashToCrxId(extension_id));
  }

  {
    std::string extension_id = "aBcDeFgHiJkLmNoPpOnMlKjIhGfEdCbA";
    ASSERT_EQ(base::StringToLowerASCII(extension_id),
              CrxIdToHashToCrxId(extension_id));
  }

  {
    std::string extension_id = crx_file::id_util::GenerateId("Moose");
    ASSERT_EQ(extension_id, CrxIdToHashToCrxId(extension_id));
  }
}

TEST_F(SupervisedUserWhitelistInstallerTest, InstallNewWhitelist) {
  EXPECT_CALL(component_update_service_.on_demand_updater(),
              OnDemandUpdate(kCrxId))
      .WillOnce(testing::Return(ComponentUpdateService::kOk));

  base::RunLoop registration_run_loop;
  component_update_service_.set_registration_callback(
      registration_run_loop.QuitClosure());

  base::RunLoop installation_run_loop;
  bool new_installation = true;
  LoadWhitelist(new_installation, installation_run_loop.QuitClosure());
  registration_run_loop.Run();

  ASSERT_NO_FATAL_FAILURE(CheckRegisteredComponent("0.0.0.0"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath unpacked_path = temp_dir.path();
  ASSERT_NO_FATAL_FAILURE(PrepareWhitelistDirectory(unpacked_path));

  const CrxComponent* component =
      component_update_service_.registered_component();
  ASSERT_TRUE(component);
  ASSERT_TRUE(component->installer->Install(manifest_, unpacked_path));
  installation_run_loop.Run();

  std::string whitelist_contents;
  ASSERT_TRUE(base::ReadFileToString(whitelist_path_, &whitelist_contents));
  EXPECT_EQ(kWhitelistContents, whitelist_contents);
}

TEST_F(SupervisedUserWhitelistInstallerTest, RegisterExistingWhitelist) {
  ASSERT_TRUE(base::CreateDirectory(whitelist_directory_));
  ASSERT_NO_FATAL_FAILURE(PrepareWhitelistDirectory(whitelist_directory_));

  base::RunLoop run_loop;
  bool new_installation = false;
  LoadWhitelist(new_installation, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_NO_FATAL_FAILURE(CheckRegisteredComponent(kVersion));
}

}  // namespace component_updater
