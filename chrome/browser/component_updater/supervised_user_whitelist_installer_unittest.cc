// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_path_override.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_json/testing_json_parser.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using update_client::CrxComponent;
using update_client::CrxUpdateItem;

namespace component_updater {

namespace {

const char kClientId[] = "client-id";
const char kCrxId[] = "abcdefghijklmnopponmlkjihgfedcba";
const char kName[] = "Some Whitelist";
const char kOtherClientId[] = "other-client-id";
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

std::string JsonToString(const base::DictionaryValue& dict) {
  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

class MockComponentUpdateService : public ComponentUpdateService,
                                   public OnDemandUpdater {
 public:
  MockComponentUpdateService(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : task_runner_(task_runner), on_demand_update_called_(false) {}

  ~MockComponentUpdateService() override {}

  bool on_demand_update_called() const { return on_demand_update_called_; }

  const CrxComponent* registered_component() { return component_.get(); }

  void set_registration_callback(const base::Closure& registration_callback) {
    registration_callback_ = registration_callback;
  }

  // ComponentUpdateService implementation:
  void AddObserver(Observer* observer) override { ADD_FAILURE(); }
  void RemoveObserver(Observer* observer) override { ADD_FAILURE(); }

  std::vector<std::string> GetComponentIDs() const override {
    ADD_FAILURE();
    return std::vector<std::string>();
  }

  bool RegisterComponent(const CrxComponent& component) override {
    EXPECT_EQ(nullptr, component_.get());
    component_.reset(new CrxComponent(component));
    if (!registration_callback_.is_null())
      registration_callback_.Run();

    return true;
  }

  bool UnregisterComponent(const std::string& crx_id) override {
    if (!component_) {
      ADD_FAILURE();
      return false;
    }

    EXPECT_EQ(GetCrxComponentID(*component_), crx_id);
    if (!component_->installer->Uninstall()) {
      ADD_FAILURE();
      return false;
    }

    component_.reset();
    return true;
  }

  OnDemandUpdater& GetOnDemandUpdater() override { return *this; }

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

  // OnDemandUpdater implementation:
  bool OnDemandUpdate(const std::string& crx_id) override {
    on_demand_update_called_ = true;

    if (!component_) {
      ADD_FAILURE() << "Trying to update unregistered component " << crx_id;
      return false;
    }

    EXPECT_EQ(GetCrxComponentID(*component_), crx_id);
    return true;
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<CrxComponent> component_;
  base::Closure registration_callback_;
  bool on_demand_update_called_;
};

class WhitelistLoadObserver {
 public:
  explicit WhitelistLoadObserver(SupervisedUserWhitelistInstaller* installer)
      : weak_ptr_factory_(this) {
    installer->Subscribe(base::Bind(&WhitelistLoadObserver::OnWhitelistReady,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void Wait() { run_loop_.Run(); }

  const base::FilePath& whitelist_path() { return whitelist_path_; }

 private:
  void OnWhitelistReady(const std::string& crx_id,
                        const base::string16& title,
                        const base::FilePath& whitelist_path) {
    EXPECT_EQ(base::FilePath::StringType(), whitelist_path_.value());
    whitelist_path_ = whitelist_path;
    run_loop_.Quit();
  }

  base::FilePath whitelist_path_;

  base::RunLoop run_loop_;
  base::WeakPtrFactory<WhitelistLoadObserver> weak_ptr_factory_;
};

}  // namespace

class SupervisedUserWhitelistInstallerTest : public testing::Test {
 public:
  SupervisedUserWhitelistInstallerTest()
      : raw_whitelists_path_override_(DIR_SUPERVISED_USER_WHITELISTS),
        installed_whitelists_path_override_(
            chrome::DIR_SUPERVISED_USER_INSTALLED_WHITELISTS),
        component_update_service_(base::ThreadTaskRunnerHandle::Get()),
        installer_(
            SupervisedUserWhitelistInstaller::Create(&component_update_service_,
                                                     nullptr,
                                                     &local_state_)) {}

  ~SupervisedUserWhitelistInstallerTest() override {}

  void SetUp() override {
    SupervisedUserWhitelistInstaller::RegisterPrefs(local_state_.registry());

    ASSERT_TRUE(PathService::Get(DIR_SUPERVISED_USER_WHITELISTS,
                                 &whitelist_base_directory_));
    whitelist_directory_ = whitelist_base_directory_.AppendASCII(kCrxId);
    whitelist_version_directory_ = whitelist_directory_.AppendASCII(kVersion);

    ASSERT_TRUE(
        PathService::Get(chrome::DIR_SUPERVISED_USER_INSTALLED_WHITELISTS,
                         &installed_whitelist_directory_));
    std::string crx_id(kCrxId);
    whitelist_path_ =
        installed_whitelist_directory_.AppendASCII(crx_id + ".json");

    scoped_ptr<base::DictionaryValue> whitelist_dict(
        new base::DictionaryValue);
    whitelist_dict->SetString("sites", kWhitelistFile);
    manifest_.Set("whitelisted_content", whitelist_dict.release());
    manifest_.SetString("version", kVersion);

    scoped_ptr<base::DictionaryValue> crx_dict(new base::DictionaryValue);
    crx_dict->SetString("name", kName);
    scoped_ptr<base::ListValue> clients(new base::ListValue);
    clients->AppendString(kClientId);
    clients->AppendString(kOtherClientId);
    crx_dict->Set("clients", clients.release());
    pref_.Set(kCrxId, crx_dict.release());
  }

 protected:
  void PrepareWhitelistFile(const base::FilePath& whitelist_path) {
    size_t whitelist_contents_length = sizeof(kWhitelistContents) - 1;
    ASSERT_EQ(static_cast<int>(whitelist_contents_length),
              base::WriteFile(whitelist_path, kWhitelistContents,
                              whitelist_contents_length));
  }

  void PrepareWhitelistDirectory(const base::FilePath& whitelist_directory) {
    PrepareWhitelistFile(whitelist_directory.AppendASCII(kWhitelistFile));
    base::FilePath manifest_file =
        whitelist_directory.AppendASCII("manifest.json");
    ASSERT_TRUE(JSONFileValueSerializer(manifest_file).Serialize(manifest_));
  }

  void RegisterExistingComponents() {
    local_state_.Set(prefs::kRegisteredSupervisedUserWhitelists, pref_);
    installer_->RegisterComponents();
    base::RunLoop().RunUntilIdle();
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
  base::ScopedPathOverride raw_whitelists_path_override_;
  base::ScopedPathOverride installed_whitelists_path_override_;
  safe_json::TestingJsonParser::ScopedFactoryOverride json_parser_override_;
  MockComponentUpdateService component_update_service_;
  TestingPrefServiceSimple local_state_;
  scoped_ptr<SupervisedUserWhitelistInstaller> installer_;
  base::FilePath whitelist_base_directory_;
  base::FilePath whitelist_directory_;
  base::FilePath whitelist_version_directory_;
  base::FilePath installed_whitelist_directory_;
  base::FilePath whitelist_path_;
  base::DictionaryValue manifest_;
  base::DictionaryValue pref_;
};

TEST_F(SupervisedUserWhitelistInstallerTest, GetHashFromCrxId) {
  {
    std::string extension_id = "abcdefghijklmnopponmlkjihgfedcba";
    ASSERT_EQ(extension_id, CrxIdToHashToCrxId(extension_id));
  }

  {
    std::string extension_id = "aBcDeFgHiJkLmNoPpOnMlKjIhGfEdCbA";
    ASSERT_EQ(base::ToLowerASCII(extension_id),
              CrxIdToHashToCrxId(extension_id));
  }

  {
    std::string extension_id = crx_file::id_util::GenerateId("Moose");
    ASSERT_EQ(extension_id, CrxIdToHashToCrxId(extension_id));
  }
}

TEST_F(SupervisedUserWhitelistInstallerTest, InstallNewWhitelist) {
  base::RunLoop registration_run_loop;
  component_update_service_.set_registration_callback(
      registration_run_loop.QuitClosure());

  WhitelistLoadObserver observer(installer_.get());
  installer_->RegisterWhitelist(kClientId, kCrxId, kName);
  registration_run_loop.Run();

  ASSERT_NO_FATAL_FAILURE(CheckRegisteredComponent("0.0.0.0"));
  EXPECT_TRUE(component_update_service_.on_demand_update_called());

  // Registering the same whitelist for another client should not do anything.
  installer_->RegisterWhitelist(kOtherClientId, kCrxId, kName);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath unpacked_path = temp_dir.path();
  ASSERT_NO_FATAL_FAILURE(PrepareWhitelistDirectory(unpacked_path));

  const CrxComponent* component =
      component_update_service_.registered_component();
  ASSERT_TRUE(component);
  EXPECT_TRUE(component->installer->Install(manifest_, unpacked_path));

  observer.Wait();
  EXPECT_EQ(whitelist_path_.value(), observer.whitelist_path().value());

  std::string whitelist_contents;
  ASSERT_TRUE(base::ReadFileToString(whitelist_path_, &whitelist_contents));

  // The actual file contents don't have to be equal, but the parsed values
  // should be.
  EXPECT_TRUE(base::JSONReader::Read(kWhitelistContents)
                  ->Equals(base::JSONReader::Read(whitelist_contents).get()))
      << kWhitelistContents << " vs. " << whitelist_contents;

  EXPECT_EQ(JsonToString(pref_),
            JsonToString(*local_state_.GetDictionary(
                prefs::kRegisteredSupervisedUserWhitelists)));
}

TEST_F(SupervisedUserWhitelistInstallerTest,
       RegisterAndUninstallExistingWhitelist) {
  ASSERT_TRUE(base::CreateDirectory(whitelist_version_directory_));
  ASSERT_NO_FATAL_FAILURE(
      PrepareWhitelistDirectory(whitelist_version_directory_));
  ASSERT_NO_FATAL_FAILURE(PrepareWhitelistFile(whitelist_path_));

  // Create another whitelist directory, with an ID that is not registered.
  base::FilePath other_directory =
      whitelist_base_directory_.AppendASCII("paobncmdlekfjgihhigjfkeldmcnboap");
  ASSERT_TRUE(base::CreateDirectory(other_directory));
  ASSERT_NO_FATAL_FAILURE(PrepareWhitelistDirectory(other_directory));

  // Create a directory that is not a valid whitelist directory.
  base::FilePath non_whitelist_directory =
      whitelist_base_directory_.AppendASCII("Not a whitelist");
  ASSERT_TRUE(base::CreateDirectory(non_whitelist_directory));

  RegisterExistingComponents();

  ASSERT_NO_FATAL_FAILURE(CheckRegisteredComponent(kVersion));
  EXPECT_FALSE(component_update_service_.on_demand_update_called());

  // Check that unregistered whitelists have been removed:
  // The registered whitelist directory should still exist.
  EXPECT_TRUE(base::DirectoryExists(whitelist_directory_));

  // The other directory should be gone.
  EXPECT_FALSE(base::DirectoryExists(other_directory));

  // The non-whitelist directory should still exist as well.
  EXPECT_TRUE(base::DirectoryExists(non_whitelist_directory));

  // Unregistering for the first client should do nothing.
  {
    base::RunLoop run_loop;
    installer_->UnregisterWhitelist(kClientId, kCrxId);
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(component_update_service_.registered_component());
  EXPECT_TRUE(base::DirectoryExists(whitelist_version_directory_));
  EXPECT_TRUE(base::PathExists(whitelist_path_));

  // Unregistering for the second client should uninstall the whitelist.
  {
    base::RunLoop run_loop;
    installer_->UnregisterWhitelist(kOtherClientId, kCrxId);
    run_loop.RunUntilIdle();
  }
  EXPECT_FALSE(component_update_service_.registered_component());
  EXPECT_FALSE(base::DirectoryExists(whitelist_directory_));
  EXPECT_FALSE(base::PathExists(whitelist_path_));
}

}  // namespace component_updater
