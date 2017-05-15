// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/extension_data_collection.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_signer.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/csd.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

using ::testing::StrictMock;

namespace {
class ExtensionTestingProfile : public TestingProfile {
 public:
  explicit ExtensionTestingProfile(TestingProfile* profile);
  void AddExtension(
      std::string extension_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      std::string extension_name = "Test Extension",
      base::Time install_time = base::Time::Now(),
      std::string version = "1",
      std::string description = "Test extension",
      std::string update_url =
          "https://clients2.google.com/service/update2/crx",
      int state_value = 0);

  void SetInstallSignature(extensions::InstallSignature signature);

 private:
  TestingProfile* profile_;
  ExtensionService* extension_service_;
  extensions::ExtensionPrefs* extension_prefs_;
};

ExtensionTestingProfile::ExtensionTestingProfile(TestingProfile* profile)
    : profile_(profile) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  extensions::TestExtensionSystem* test_extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile_));
  extension_service_ = test_extension_system->CreateExtensionService(
      &command_line, base::FilePath() /* install_directory */,
      false /* autoupdate_enabled */);
  extension_prefs_ = extensions::ExtensionPrefs::Get(profile_);
}

void ExtensionTestingProfile::AddExtension(std::string extension_id,
                                           std::string extension_name,
                                           base::Time install_time,
                                           std::string version,
                                           std::string description,
                                           std::string update_url,
                                           int state_value) {
  scoped_refptr<extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetID(extension_id)
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", extension_name)
                           .Set("version", version)
                           .Set("manifest_version", 2)
                           .Set("description", description)
                           .Set("update_url", update_url)
                           .Build())
          .Build();

  // Install the extension on the faked extension service.
  extension_service_->AddExtension(extension.get());

  extension_prefs_->UpdateExtensionPref(
      extension_id, "install_time",
      base::MakeUnique<base::Value>(
          base::Int64ToString(install_time.ToInternalValue())));
  extension_prefs_->UpdateExtensionPref(
      extension_id, "state", base::MakeUnique<base::Value>(state_value));
}

void ExtensionTestingProfile::SetInstallSignature(
    extensions::InstallSignature signature) {
  base::DictionaryValue signature_dict;
  signature.ToValue(&signature_dict);
  extension_prefs_->SetInstallSignature(&signature_dict);
}

}  // namespace

namespace safe_browsing {

class ExtensionDataCollectionTest : public testing::Test {
 protected:
  // A type for specifying whether a profile created by CreateProfile
  // participates in safe browsing and safe browsing extended reporting.
  enum SafeBrowsingDisposition {
    OPT_OUT,
    SAFE_BROWSING_ONLY,
    EXTENDED_REPORTING_ONLY,
    SAFE_BROWSING_AND_EXTENDED_REPORTING,
  };

  ExtensionDataCollectionTest() : profile_number_() {}

  void SetUp() override {
    testing::Test::SetUp();
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  void TearDown() override {
    profile_manager_.reset();
    TestingBrowserProcess::DeleteInstance();
    testing::Test::TearDown();
  }

  std::unique_ptr<ExtensionTestingProfile> CreateProfile(
      SafeBrowsingDisposition safe_browsing_opt_in) {
    std::string profile_name("profile");
    profile_name.append(base::IntToString(++profile_number_));

    // Create prefs for the profile and safe browsing preferences accordingly.
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs(
        new sync_preferences::TestingPrefServiceSyncable);
    chrome::RegisterUserProfilePrefs(prefs->registry());
    prefs->SetBoolean(
        prefs::kSafeBrowsingEnabled,
        safe_browsing_opt_in == SAFE_BROWSING_ONLY ||
            safe_browsing_opt_in == SAFE_BROWSING_AND_EXTENDED_REPORTING);
    safe_browsing::SetExtendedReportingPref(
        prefs.get(),
        safe_browsing_opt_in == EXTENDED_REPORTING_ONLY ||
            safe_browsing_opt_in == SAFE_BROWSING_AND_EXTENDED_REPORTING);
    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        profile_name, std::move(prefs),
        base::UTF8ToUTF16(profile_name),  // user_name
        0,                                // avatar_id
        std::string(),                    // supervised_user_id
        TestingProfile::TestingFactories());

    return base::MakeUnique<ExtensionTestingProfile>(profile);
  }

  content::TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  int profile_number_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

TEST_F(ExtensionDataCollectionTest, CollectExtensionDataNoExtensions) {
  std::unique_ptr<ExtensionTestingProfile> profile =
      CreateProfile(SAFE_BROWSING_AND_EXTENDED_REPORTING);

  ClientIncidentReport_ExtensionData data;
  CollectExtensionData(&data);

  ASSERT_FALSE(data.has_last_installed_extension());
}

TEST_F(ExtensionDataCollectionTest, CollectExtensionDataNoSafeBrowsing) {
  std::unique_ptr<ExtensionTestingProfile> profile =
      CreateProfile(EXTENDED_REPORTING_ONLY);
  profile->AddExtension();

  ClientIncidentReport_ExtensionData data;
  CollectExtensionData(&data);

  ASSERT_FALSE(data.has_last_installed_extension());
}

TEST_F(ExtensionDataCollectionTest, CollectExtensionDataNoExtendedReporting) {
  std::unique_ptr<ExtensionTestingProfile> profile =
      CreateProfile(SAFE_BROWSING_ONLY);
  profile->AddExtension();

  ClientIncidentReport_ExtensionData data;
  CollectExtensionData(&data);

  ASSERT_FALSE(data.has_last_installed_extension());
}

TEST_F(ExtensionDataCollectionTest, CollectExtensionDataWithExtension) {
  std::string extension_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  std::string extension_name = "my_test_extension";
  base::Time install_time = base::Time::Now();
  std::string version = "1.4.2";
  std::string description = "Test Extension";
  std::string update_url = "https://www.chromium.org";
  int state = extensions::Extension::State::ENABLED;

  std::unique_ptr<ExtensionTestingProfile> profile =
      CreateProfile(SAFE_BROWSING_AND_EXTENDED_REPORTING);
  profile->AddExtension(extension_id, extension_name, install_time, version,
                        description, update_url, state);

  extensions::ExtensionIdSet valid_ids;
  extensions::ExtensionIdSet invalid_ids;
  invalid_ids.insert(extension_id);
  extensions::InstallSignature signature = {};
  signature.ids = valid_ids;
  signature.invalid_ids = invalid_ids;
  profile->SetInstallSignature(signature);

  ClientIncidentReport_ExtensionData data;
  CollectExtensionData(&data);

  ASSERT_TRUE(data.has_last_installed_extension());
  ClientIncidentReport_ExtensionData_ExtensionInfo extension_info =
      data.last_installed_extension();

  ASSERT_EQ(extension_info.id(), extension_id);
  ASSERT_EQ(extension_info.name(), extension_name);
  ASSERT_EQ(extension_info.install_time_msec(), install_time.ToJavaTime());
  ASSERT_EQ(extension_info.version(), version);
  ASSERT_EQ(extension_info.description(), description);
  ASSERT_EQ(extension_info.update_url(), update_url);
  ASSERT_EQ(extension_info.has_signature_validation(), true);
  ASSERT_EQ(extension_info.signature_is_valid(), false);
  ASSERT_EQ(extension_info.state(), state);
  std::string expected_manifest =
      "{\"description\":\"Test Extension\",\""
      "manifest_version\":2,\"name\":\"my_test_extension\",\"update_url\":\""
      "https://www.chromium.org\",\"version\":\"1.4.2\"}";
  ASSERT_EQ(extension_info.manifest(), expected_manifest);
}

TEST_F(ExtensionDataCollectionTest, CollectsLastInstalledExtension) {
  std::string extension_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  std::string extension_name = "extension_2";
  base::Time install_time = base::Time::Now() - base::TimeDelta::FromMinutes(3);

  std::unique_ptr<ExtensionTestingProfile> profile =
      CreateProfile(SAFE_BROWSING_AND_EXTENDED_REPORTING);
  profile->AddExtension("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "extension_1",
                        base::Time::Now() - base::TimeDelta::FromDays(2));
  profile->AddExtension(extension_id, extension_name, install_time);
  profile->AddExtension("cccccccccccccccccccccccccccccccc", "extension_3",
                        base::Time::Now() - base::TimeDelta::FromHours(4));

  ClientIncidentReport_ExtensionData data;
  CollectExtensionData(&data);

  ASSERT_TRUE(data.has_last_installed_extension());
  ClientIncidentReport_ExtensionData_ExtensionInfo extension_info =
      data.last_installed_extension();

  ASSERT_EQ(extension_info.id(), extension_id);
  ASSERT_EQ(extension_info.name(), extension_name);
  ASSERT_EQ(extension_info.install_time_msec(), install_time.ToJavaTime());
}

TEST_F(ExtensionDataCollectionTest, IgnoresExtensionsIfNoExtendedSafeBrowsing) {
  std::string extension_id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  std::string extension_name = "extension_2";

  std::unique_ptr<ExtensionTestingProfile> profile =
      CreateProfile(SAFE_BROWSING_AND_EXTENDED_REPORTING);

  profile->AddExtension(extension_id, extension_name,
                        base::Time::Now() - base::TimeDelta::FromDays(3));

  std::unique_ptr<ExtensionTestingProfile> profile_without_safe_browsing =
      CreateProfile(SAFE_BROWSING_ONLY);
  profile_without_safe_browsing->AddExtension(
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "extension_1",
      base::Time::Now() - base::TimeDelta::FromDays(2));

  ClientIncidentReport_ExtensionData data;
  CollectExtensionData(&data);

  ASSERT_TRUE(data.has_last_installed_extension());
  ClientIncidentReport_ExtensionData_ExtensionInfo extension_info =
      data.last_installed_extension();

  ASSERT_EQ(extension_info.id(), extension_id);
  ASSERT_EQ(extension_info.name(), extension_name);
}

}  // namespace safe_browsing
