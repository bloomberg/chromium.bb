// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include <string>
#include <unordered_map>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/util/type_safety/strong_alias.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/ui/webui/help/test_version_updater.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/crx_file/id_util.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safety_check/test_update_check_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/devicetype_utils.h"
#endif

// Components for building event strings.
constexpr char kParent[] = "parent";
constexpr char kUpdates[] = "updates";
constexpr char kPasswords[] = "passwords";
constexpr char kSafeBrowsing[] = "safe-browsing";
constexpr char kExtensions[] = "extensions";

namespace {
using Enabled = util::StrongAlias<class EnabledTag, bool>;
using UserCanDisable = util::StrongAlias<class UserCanDisableTag, bool>;

class TestingSafetyCheckHandler : public SafetyCheckHandler {
 public:
  using SafetyCheckHandler::AllowJavascript;
  using SafetyCheckHandler::DisallowJavascript;
  using SafetyCheckHandler::set_web_ui;
  using SafetyCheckHandler::SetVersionUpdaterForTesting;

  TestingSafetyCheckHandler(
      std::unique_ptr<safety_check::UpdateCheckHelper> update_helper,
      std::unique_ptr<VersionUpdater> version_updater,
      password_manager::BulkLeakCheckService* leak_service,
      extensions::PasswordsPrivateDelegate* passwords_delegate,
      extensions::ExtensionPrefs* extension_prefs,
      extensions::ExtensionServiceInterface* extension_service)
      : SafetyCheckHandler(std::move(update_helper),
                           std::move(version_updater),
                           leak_service,
                           passwords_delegate,
                           extension_prefs,
                           extension_service) {}
};

class TestDestructionVersionUpdater : public TestVersionUpdater {
 public:
  ~TestDestructionVersionUpdater() override { destructor_invoked_ = true; }

  void CheckForUpdate(const StatusCallback& callback,
                      const PromoteCallback&) override {}

  static bool GetDestructorInvoked() { return destructor_invoked_; }

 private:
  static bool destructor_invoked_;
};

bool TestDestructionVersionUpdater::destructor_invoked_ = false;

class TestPasswordsDelegate : public extensions::TestPasswordsPrivateDelegate {
 public:
  void SetBulkLeakCheckService(
      password_manager::BulkLeakCheckService* leak_service) {
    leak_service_ = leak_service;
  }

  void SetNumCompromisedCredentials(int compromised_password_count) {
    compromised_password_count_ = compromised_password_count;
  }

  void SetPasswordCheckState(
      extensions::api::passwords_private::PasswordCheckState state) {
    state_ = state;
  }

  void SetProgress(int done, int total) {
    done_ = done;
    total_ = total;
  }

  std::vector<extensions::api::passwords_private::CompromisedCredential>
  GetCompromisedCredentials() override {
    std::vector<extensions::api::passwords_private::CompromisedCredential>
        compromised(compromised_password_count_);
    for (int i = 0; i < compromised_password_count_; ++i) {
      compromised[i].username = "test" + base::NumberToString(i);
    }
    return compromised;
  }

  extensions::api::passwords_private::PasswordCheckStatus
  GetPasswordCheckStatus() override {
    extensions::api::passwords_private::PasswordCheckStatus status;
    status.state = state_;
    if (total_ != 0) {
      status.already_processed = std::make_unique<int>(done_);
      status.remaining_in_queue = std::make_unique<int>(total_ - done_);
    }
    return status;
  }

 private:
  password_manager::BulkLeakCheckService* leak_service_ = nullptr;
  int compromised_password_count_ = 0;
  int done_ = 0;
  int total_ = 0;
  extensions::api::passwords_private::PasswordCheckState state_ =
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_IDLE;
};

class TestSafetyCheckExtensionService : public TestExtensionService {
 public:
  void AddExtensionState(const std::string& extension_id,
                         Enabled enabled,
                         UserCanDisable user_can_disable) {
    state_map_.emplace(extension_id, ExtensionState{enabled.value(),
                                                    user_can_disable.value()});
  }

  bool IsExtensionEnabled(const std::string& extension_id) const override {
    auto it = state_map_.find(extension_id);
    if (it == state_map_.end()) {
      return false;
    }
    return it->second.enabled;
  }

  bool UserCanDisableInstalledExtension(
      const std::string& extension_id) override {
    auto it = state_map_.find(extension_id);
    if (it == state_map_.end()) {
      return false;
    }
    return it->second.user_can_disable;
  }

 private:
  struct ExtensionState {
    bool enabled;
    bool user_can_disable;
  };

  std::unordered_map<std::string, ExtensionState> state_map_;
};

}  // namespace

class SafetyCheckHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;

  // Returns a |base::DictionaryValue| for safety check status update that
  // has the specified |component| and |new_state| if it exists; nullptr
  // otherwise.
  const base::DictionaryValue* GetSafetyCheckStatusChangedWithDataIfExists(
      const std::string& component,
      int new_state);

  std::string GenerateExtensionId(char char_to_repeat);

  void VerifyDisplayString(const base::DictionaryValue* event,
                           const base::string16& expected);
  void VerifyDisplayString(const base::DictionaryValue* event,
                           const std::string& expected);

 protected:
  safety_check::TestUpdateCheckHelper* update_helper_ = nullptr;
  TestVersionUpdater* version_updater_ = nullptr;
  std::unique_ptr<password_manager::BulkLeakCheckService> test_leak_service_;
  TestPasswordsDelegate test_passwords_delegate_;
  extensions::ExtensionPrefs* test_extension_prefs_ = nullptr;
  TestSafetyCheckExtensionService test_extension_service_;
  content::TestWebUI test_web_ui_;
  std::unique_ptr<TestingSafetyCheckHandler> safety_check_;
  base::HistogramTester histogram_tester_;

 private:
  // Replaces any instances of browser name (e.g. Google Chrome, Chromium,
  // etc) with "browser" to make sure tests work both on Chromium and
  // Google Chrome.
  void ReplaceBrowserName(base::string16* s);
};

void SafetyCheckHandlerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // The unique pointer to a TestVersionUpdater gets moved to
  // SafetyCheckHandler, but a raw pointer is retained here to change its
  // state.
  auto update_helper = std::make_unique<safety_check::TestUpdateCheckHelper>();
  update_helper_ = update_helper.get();
  auto version_updater = std::make_unique<TestVersionUpdater>();
  version_updater_ = version_updater.get();
  test_leak_service_ = std::make_unique<password_manager::BulkLeakCheckService>(
      nullptr, nullptr);
  test_passwords_delegate_.SetBulkLeakCheckService(test_leak_service_.get());
  test_web_ui_.set_web_contents(web_contents());
  test_extension_prefs_ = extensions::ExtensionPrefs::Get(profile());
  safety_check_ = std::make_unique<TestingSafetyCheckHandler>(
      std::move(update_helper), std::move(version_updater),
      test_leak_service_.get(), &test_passwords_delegate_,
      test_extension_prefs_, &test_extension_service_);
  test_web_ui_.ClearTrackedCalls();
  safety_check_->set_web_ui(&test_web_ui_);
  safety_check_->AllowJavascript();
}

const base::DictionaryValue*
SafetyCheckHandlerTest::GetSafetyCheckStatusChangedWithDataIfExists(
    const std::string& component,
    int new_state) {
  // Return the latest update if multiple, so iterate from the end.
  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>& call_data =
      test_web_ui_.call_data();
  for (int i = call_data.size() - 1; i >= 0; --i) {
    const content::TestWebUI::CallData& data = *(call_data[i]);
    if (data.function_name() != "cr.webUIListenerCallback") {
      continue;
    }
    std::string event;
    if ((!data.arg1()->GetAsString(&event)) ||
        event != "safety-check-" + component + "-status-changed") {
      continue;
    }
    const base::DictionaryValue* dictionary = nullptr;
    if (!data.arg2()->GetAsDictionary(&dictionary)) {
      continue;
    }
    int cur_new_state;
    if (dictionary->GetInteger("newState", &cur_new_state) &&
        cur_new_state == new_state) {
      return dictionary;
    }
  }
  return nullptr;
}

std::string SafetyCheckHandlerTest::GenerateExtensionId(char char_to_repeat) {
  return std::string(crx_file::id_util::kIdSize * 2, char_to_repeat);
}

void SafetyCheckHandlerTest::VerifyDisplayString(
    const base::DictionaryValue* event,
    const base::string16& expected) {
  base::string16 display;
  ASSERT_TRUE(event->GetString("displayString", &display));
  ReplaceBrowserName(&display);
  // Need to also replace any instances of Chrome and Chromium in the
  // expected string due to an edge case on ChromeOS, where a device name
  // is "Chrome", which gets replaced in the display string.
  base::string16 expected_replaced = expected;
  ReplaceBrowserName(&expected_replaced);
  EXPECT_EQ(expected_replaced, display);
}

void SafetyCheckHandlerTest::VerifyDisplayString(
    const base::DictionaryValue* event,
    const std::string& expected) {
  VerifyDisplayString(event, base::ASCIIToUTF16(expected));
}

void SafetyCheckHandlerTest::ReplaceBrowserName(base::string16* s) {
  base::ReplaceSubstringsAfterOffset(s, 0, base::ASCIIToUTF16("Google Chrome"),
                                     base::ASCIIToUTF16("Browser"));
  base::ReplaceSubstringsAfterOffset(s, 0, base::ASCIIToUTF16("Chrome"),
                                     base::ASCIIToUTF16("Browser"));
  base::ReplaceSubstringsAfterOffset(s, 0, base::ASCIIToUTF16("Chromium"),
                                     base::ASCIIToUTF16("Browser"));
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Checking) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::CHECKING);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16(""));
  // Checking state should not get recorded.
  histogram_tester_.ExpectTotalCount("Settings.SafetyCheck.UpdatesResult", 0);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Updated) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::UPDATED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kUpdated));
  ASSERT_TRUE(event);
#if defined(OS_CHROMEOS)
  base::string16 expected = base::ASCIIToUTF16("Your ") +
                            ui::GetChromeOSDeviceName() +
                            base::ASCIIToUTF16(" is up to date");
  VerifyDisplayString(event, expected);
#else
  VerifyDisplayString(event, "Browser is up to date");
#endif
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kUpdated, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Updating) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::UPDATING);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kUpdating));
  ASSERT_TRUE(event);
#if defined(OS_CHROMEOS)
  VerifyDisplayString(event, "Updating your device");
#else
  VerifyDisplayString(event, "Updating Browser");
#endif
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kUpdating, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Relaunch) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::NEARLY_UPDATED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kRelaunch));
  ASSERT_TRUE(event);
#if defined(OS_CHROMEOS)
  VerifyDisplayString(
      event, "Nearly up to date! Restart your device to finish updating.");
#else
  VerifyDisplayString(event,
                      "Nearly up to date! Relaunch Browser to finish "
                      "updating. Incognito windows won't reopen.");
#endif
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kRelaunch, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Disabled) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::DISABLED);
  safety_check_->PerformSafetyCheck();
  // TODO(crbug/1072432): Since the UNKNOWN state is not present in JS in M83,
  // use FAILED_OFFLINE, which uses the same icon.
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailedOffline));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event, "Version " + version_info::GetVersionNumber() + " (" +
                 (version_info::IsOfficialBuild() ? "Official Build"
                                                  : "Developer Build") +
                 ") " + chrome::GetChannelName() +
                 (sizeof(void*) == 8 ? " (64-bit)" : " (32-bit)"));
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kUnknown, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_DisabledByAdmin) {
  version_updater_->SetReturnedStatus(
      VersionUpdater::Status::DISABLED_BY_ADMIN);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kDisabledByAdmin));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "Updates are managed by <a target=\"_blank\" "
      "href=\"https://support.google.com/chrome?p=your_administrator\">your "
      "administrator</a>");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kDisabledByAdmin, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_FailedOffline) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED_OFFLINE);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailedOffline));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check for updates. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kFailedOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Failed_ConnectivityOnline) {
  update_helper_->SetConnectivity(true);
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailed));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "Browser didn't update, something went wrong. <a target=\"_blank\" "
      "href=\"https://support.google.com/chrome?p=fix_chrome_updates\">Fix "
      "Browser update problems and failed updates.</a>");
  histogram_tester_.ExpectBucketCount("Settings.SafetyCheck.UpdatesResult",
                                      SafetyCheckHandler::UpdateStatus::kFailed,
                                      1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Failed_ConnectivityOffline) {
  update_helper_->SetConnectivity(false);
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailedOffline));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check for updates. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kFailedOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_DestroyedOnJavascriptDisallowed) {
  EXPECT_FALSE(TestDestructionVersionUpdater::GetDestructorInvoked());
  safety_check_->SetVersionUpdaterForTesting(
      std::make_unique<TestDestructionVersionUpdater>());
  safety_check_->PerformSafetyCheck();
  safety_check_->DisallowJavascript();
  EXPECT_TRUE(TestDestructionVersionUpdater::GetDestructorInvoked());
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_EnabledStandard) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(
              SafetyCheckHandler::SafeBrowsingStatus::kEnabledStandard));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "Standard Protection is on");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kEnabledStandard, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_EnabledEnhanced) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(
              SafetyCheckHandler::SafeBrowsingStatus::kEnabledEnhanced));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "Enhanced Protection is on");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kEnabledEnhanced, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_InconsistentEnhanced) {
  // Tests the corner case of SafeBrowsingEnhanced pref being on, while
  // SafeBrowsing enabled is off. This should be treated as SB off.
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kDisabled));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event, "Safe Browsing is off. Browser recommends turning it on.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabled, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_Disabled) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kDisabled));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event, "Safe Browsing is off. Browser recommends turning it on.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabled, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_DisabledByAdmin) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetManagedPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(false));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(
              SafetyCheckHandler::SafeBrowsingStatus::kDisabledByAdmin));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "<a target=\"_blank\" "
      "href=\"https://support.google.com/chrome?p=your_administrator\">Your "
      "administrator</a> has turned off Safe Browsing");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabledByAdmin, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_DisabledByExtension) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetExtensionPref(prefs::kSafeBrowsingEnabled,
                         std::make_unique<base::Value>(false));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(
              SafetyCheckHandler::SafeBrowsingStatus::kDisabledByExtension));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "An extension has turned off Safe Browsing");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabledByExtension, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_ObserverRemovedAfterError) {
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16(""));
  histogram_tester_.ExpectTotalCount("Settings.SafetyCheck.PasswordsResult", 0);
  // Second, an "offline" state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kNetworkError);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2,
                      "Browser can't check your passwords. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kOffline, 1);
  // Another error, but since the previous state is terminal, the handler
  // should no longer be observing the BulkLeakCheckService state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kServiceError);
  const base::DictionaryValue* event3 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  ASSERT_TRUE(event3);
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_InterruptedAndRefreshed) {
  safety_check_->PerformSafetyCheck();
  // Password check running.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16(""));
  // The check gets interrupted and the page is refreshed.
  safety_check_->DisallowJavascript();
  safety_check_->AllowJavascript();
  // Need to set the |TestVersionUpdater| instance again to prevent
  // |PerformSafetyCheck()| from creating a real |VersionUpdater| instance.
  safety_check_->SetVersionUpdaterForTesting(
      std::make_unique<TestVersionUpdater>());
  // Another run of the safety check.
  safety_check_->PerformSafetyCheck();
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event2);
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kSignedOut);
  const base::DictionaryValue* event3 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSignedOut));
  ASSERT_TRUE(event3);
  VerifyDisplayString(event3,
                      "Browser can't check your passwords because you're not "
                      "signed in");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kSignedOut, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_StartedTwice) {
  safety_check_->PerformSafetyCheck();
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  // Then, a network error.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kNetworkError);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  EXPECT_TRUE(event2);
  VerifyDisplayString(event2,
                      "Browser can't check your passwords. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_ObserverNotifiedTwice) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_.StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  // Another notification about the same state change.
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kError));
  ASSERT_TRUE(event);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Safe) {
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Second, a "safe" state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSafe));
  EXPECT_TRUE(event);
  VerifyDisplayString(event, "No compromised passwords found");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kSafe, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_CompromisedExist) {
  constexpr int kCompromised = 7;
  test_passwords_delegate_.SetNumCompromisedCredentials(kCompromised);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(
              SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(
      event2, base::NumberToString(kCompromised) + " compromised passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Error) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_.StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kError));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check your passwords. Try again "
                      "later.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kError, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_RunningOneCompromised) {
  test_passwords_delegate_.SetNumCompromisedCredentials(1);
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_.StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(password_manager::BulkLeakCheckService::State::kIdle);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(
              SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "1 compromised password");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_NoPasswords) {
  test_passwords_delegate_.ClearSavedPasswordsList();
  test_passwords_delegate_.SetStartPasswordCheckState(
      password_manager::BulkLeakCheckService::State::kIdle);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kNoPasswords));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "No saved passwords. Chrome can check your passwords "
                      "when you save them.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kNoPasswords, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Progress) {
  auto credential = password_manager::LeakCheckCredential(
      base::UTF8ToUTF16("test"), base::UTF8ToUTF16("test"));
  auto is_leaked = password_manager::IsLeaked(false);
  safety_check_->PerformSafetyCheck();
  test_passwords_delegate_.SetPasswordCheckState(
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_RUNNING);
  test_passwords_delegate_.SetProgress(1, 3);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone(credential, is_leaked);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  EXPECT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16("Checking passwords (1 of 3)…"));

  test_passwords_delegate_.SetProgress(2, 3);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone(credential, is_leaked);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  EXPECT_TRUE(event2);
  VerifyDisplayString(event2,
                      base::UTF8ToUTF16("Checking passwords (2 of 3)…"));

  // Final update comes after status change, so no new progress message should
  // be present.
  test_passwords_delegate_.SetPasswordCheckState(
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_IDLE);
  test_passwords_delegate_.SetProgress(3, 3);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone(credential, is_leaked);
  const base::DictionaryValue* event3 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  EXPECT_TRUE(event3);
  // Still 2/3 event.
  VerifyDisplayString(event3,
                      base::UTF8ToUTF16("Checking passwords (2 of 3)…"));
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_NoExtensions) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kExtensions,
      static_cast<int>(
          SafetyCheckHandler::ExtensionsStatus::kNoneBlocklisted)));
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kNoneBlocklisted, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_NoneBlocklisted) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlacklistState(
      extension_id, extensions::NOT_BLACKLISTED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions,
          static_cast<int>(
              SafetyCheckHandler::ExtensionsStatus::kNoneBlocklisted));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "You're protected from potentially harmful extensions");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kNoneBlocklisted, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedAllDisabled) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::DISABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlacklistState(
      extension_id, extensions::BLACKLISTED_MALWARE);
  test_extension_service_.AddExtensionState(extension_id, Enabled(false),
                                            UserCanDisable(false));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions,
          static_cast<int>(
              SafetyCheckHandler::ExtensionsStatus::kBlocklistedAllDisabled));
  EXPECT_TRUE(event);
  VerifyDisplayString(
      event, "1 potentially harmful extension is off. You can also remove it.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kBlocklistedAllDisabled, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedReenabledAllByUser) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlacklistState(
      extension_id, extensions::BLACKLISTED_POTENTIALLY_UNWANTED);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(true));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions, static_cast<int>(SafetyCheckHandler::ExtensionsStatus::
                                            kBlocklistedReenabledAllByUser));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "You turned 1 potentially harmful extension back on");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kBlocklistedReenabledAllByUser, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedReenabledAllByAdmin) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlacklistState(
      extension_id, extensions::BLACKLISTED_POTENTIALLY_UNWANTED);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(false));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions, static_cast<int>(SafetyCheckHandler::ExtensionsStatus::
                                            kBlocklistedReenabledAllByAdmin));
  VerifyDisplayString(event,
                      "Your administrator turned 1 potentially harmful "
                      "extension back on");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kBlocklistedReenabledAllByAdmin, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedReenabledSomeByUser) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlacklistState(
      extension_id, extensions::BLACKLISTED_POTENTIALLY_UNWANTED);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(true));

  std::string extension2_id = GenerateExtensionId('b');
  scoped_refptr<const extensions::Extension> extension2 =
      extensions::ExtensionBuilder("test1").SetID(extension2_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension2.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlacklistState(
      extension2_id, extensions::BLACKLISTED_POTENTIALLY_UNWANTED);
  test_extension_service_.AddExtensionState(extension2_id, Enabled(true),
                                            UserCanDisable(false));

  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions, static_cast<int>(SafetyCheckHandler::ExtensionsStatus::
                                            kBlocklistedReenabledSomeByUser));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "You turned 1 potentially harmful extension back "
                      "on. Your administrator "
                      "turned 1 potentially harmful extension back on.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kBlocklistedReenabledSomeByUser, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_Error) {
  // One extension in the error state.
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlacklistState(
      extension_id, extensions::BLACKLISTED_UNKNOWN);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(true));

  // Another extension blocklisted.
  std::string extension2_id = GenerateExtensionId('b');
  scoped_refptr<const extensions::Extension> extension2 =
      extensions::ExtensionBuilder("test1").SetID(extension2_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension2.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlacklistState(
      extension2_id, extensions::BLACKLISTED_POTENTIALLY_UNWANTED);
  test_extension_service_.AddExtensionState(extension2_id, Enabled(true),
                                            UserCanDisable(false));

  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions,
          static_cast<int>(SafetyCheckHandler::ExtensionsStatus::kError));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check your extensions. Try again later.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kError, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckParentRanDisplayString) {
  // 1 second before midnight Dec 31st 2020, so that -(24h-1s) is still on the
  // same day. This test time is hard coded to prevent DST flakiness, see
  // crbug.com/1066576.
  const base::Time system_time =
      base::Time::FromDoubleT(1609459199).LocalMidnight() -
      base::TimeDelta::FromSeconds(1);
  // Display strings for given time deltas in seconds.
  std::vector<std::tuple<std::string, int>> tuples{
      std::make_tuple("Safety check ran a moment ago", 1),
      std::make_tuple("Safety check ran a moment ago", 59),
      std::make_tuple("Safety check ran 1 minute ago", 60),
      std::make_tuple("Safety check ran 2 minutes ago", 60 * 2),
      std::make_tuple("Safety check ran 59 minutes ago", 60 * 60 - 1),
      std::make_tuple("Safety check ran 1 hour ago", 60 * 60),
      std::make_tuple("Safety check ran 2 hours ago", 60 * 60 * 2),
      std::make_tuple("Safety check ran 23 hours ago", 60 * 60 * 23),
      std::make_tuple("Safety check ran yesterday", 60 * 60 * 24),
      std::make_tuple("Safety check ran yesterday", 60 * 60 * 24 * 2 - 1),
      std::make_tuple("Safety check ran 2 days ago", 60 * 60 * 24 * 2),
      std::make_tuple("Safety check ran 2 days ago", 60 * 60 * 24 * 3 - 1),
      std::make_tuple("Safety check ran 3 days ago", 60 * 60 * 24 * 3),
      std::make_tuple("Safety check ran 3 days ago", 60 * 60 * 24 * 4 - 1)};
  // Test that above time deltas produce the corresponding display strings.
  for (auto tuple : tuples) {
    const base::Time time =
        system_time - base::TimeDelta::FromSeconds(std::get<1>(tuple));
    const base::string16 display_string =
        safety_check_->GetStringForParentRan(time, system_time);
    EXPECT_EQ(base::UTF8ToUTF16(std::get<0>(tuple)), display_string);
  }
}

TEST_F(SafetyCheckHandlerTest, CheckSafetyCheckStartedWebUiEvents) {
  safety_check_->SendSafetyCheckStartedWebUiUpdates();

  // Check that all initial updates ("running" states) are sent.
  const base::DictionaryValue* event_parent =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kParent,
          static_cast<int>(SafetyCheckHandler::ParentStatus::kChecking));
  ASSERT_TRUE(event_parent);
  VerifyDisplayString(event_parent, base::UTF8ToUTF16("Running…"));
  const base::DictionaryValue* event_updates =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kChecking));
  ASSERT_TRUE(event_updates);
  VerifyDisplayString(event_updates, base::UTF8ToUTF16(""));
  const base::DictionaryValue* event_pws =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event_pws);
  VerifyDisplayString(event_pws, base::UTF8ToUTF16(""));
  const base::DictionaryValue* event_sb =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kChecking));
  ASSERT_TRUE(event_sb);
  VerifyDisplayString(event_sb, base::UTF8ToUTF16(""));
  const base::DictionaryValue* event_extensions =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions,
          static_cast<int>(SafetyCheckHandler::ExtensionsStatus::kChecking));
  ASSERT_TRUE(event_extensions);
  VerifyDisplayString(event_extensions, base::UTF8ToUTF16(""));
}

TEST_F(SafetyCheckHandlerTest, CheckSafetyCheckCompletedWebUiEvents) {
  // Mock safety check invocation.
  safety_check_->PerformSafetyCheck();

  // Password mocks need to be triggered with a non-checking state to fire.
  // All other mocks fire automatically when invoked.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kSignedOut);

  // Check that the parent update is sent after all children checks completed.
  const base::DictionaryValue* event_parent =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kParent, static_cast<int>(SafetyCheckHandler::ParentStatus::kAfter));
  ASSERT_TRUE(event_parent);
  VerifyDisplayString(event_parent,
                      base::UTF8ToUTF16("Safety check ran a moment ago"));
}
