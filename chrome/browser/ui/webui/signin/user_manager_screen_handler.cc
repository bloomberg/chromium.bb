// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/user_manager_screen_handler.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

#if defined(USE_ASH)
#include "ash/shell.h"  // nogncheck
#endif

namespace {
// User dictionary keys.
const char kKeyUsername[] = "username";
const char kKeyDisplayName[]= "displayName";
const char kKeyEmailAddress[] = "emailAddress";
const char kKeyProfilePath[] = "profilePath";
const char kKeyPublicAccount[] = "publicAccount";
const char kKeyLegacySupervisedUser[] = "legacySupervisedUser";
const char kKeyChildUser[] = "childUser";
const char kKeyCanRemove[] = "canRemove";
const char kKeyIsOwner[] = "isOwner";
const char kKeyIsDesktop[] = "isDesktopUser";
const char kKeyAvatarUrl[] = "userImage";
const char kKeyNeedsSignin[] = "needsSignin";
const char kKeyHasLocalCreds[] = "hasLocalCreds";
const char kKeyStatistics[] = "statistics";
const char kKeyIsProfileLoaded[] = "isProfileLoaded";

// JS API callback names.
const char kJsApiUserManagerInitialize[] = "userManagerInitialize";
const char kJsApiUserManagerAuthLaunchUser[] = "authenticatedLaunchUser";
const char kJsApiUserManagerLaunchGuest[] = "launchGuest";
const char kJsApiUserManagerLaunchUser[] = "launchUser";
const char kJsApiUserManagerRemoveUser[] = "removeUser";
const char kJsApiUserManagerAttemptUnlock[] = "attemptUnlock";
const char kJsApiUserManagerLogRemoveUserWarningShown[] =
    "logRemoveUserWarningShown";
const char kJsApiUserManagerRemoveUserWarningLoadStats[] =
    "removeUserWarningLoadStats";
const char kJsApiUserManagerGetRemoveWarningDialogMessage[] =
    "getRemoveWarningDialogMessage";
const char kJsApiUserManagerAreAllProfilesLocked[] =
    "areAllProfilesLocked";
const size_t kAvatarIconSize = 180;
const int kMaxOAuthRetries = 3;

void HandleAndDoNothing(const base::ListValue* args) {
}

std::string GetAvatarImage(const ProfileAttributesEntry* entry) {
  bool is_gaia_picture = entry->IsUsingGAIAPicture() &&
                         entry->GetGAIAPicture() != nullptr;

  // If the avatar is too small (i.e. the old-style low resolution avatar),
  // it will be pixelated when displayed in the User Manager, so we should
  // return the placeholder avatar instead.
  gfx::Image avatar_image = entry->GetAvatarIcon();
  if (avatar_image.Width() <= profiles::kAvatarIconWidth ||
      avatar_image.Height() <= profiles::kAvatarIconHeight ) {
    avatar_image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }
  gfx::Image resized_image = profiles::GetSizedAvatarIcon(
      avatar_image, is_gaia_picture, kAvatarIconSize, kAvatarIconSize);
  return webui::GetBitmapDataUrl(resized_image.AsBitmap());
}

extensions::ScreenlockPrivateEventRouter* GetScreenlockRouter(
    const std::string& email) {
  base::FilePath path =
      profiles::GetPathOfProfileWithEmail(g_browser_process->profile_manager(),
                                          email);
  Profile* profile = g_browser_process->profile_manager()
      ->GetProfileByPath(path);
  return extensions::ScreenlockPrivateEventRouter::GetFactoryInstance()->Get(
      profile);
}

bool IsGuestModeEnabled() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBrowserGuestModeEnabled);
}

bool IsAddPersonEnabled() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBrowserAddPersonEnabled);
}

// Executes the action specified by the URL's Hash parameter, if any. Deletes
// itself after the action would be performed.
class UrlHashHelper : public chrome::BrowserListObserver {
 public:
  UrlHashHelper(Browser* browser, const std::string& hash);
  ~UrlHashHelper() override;

  void ExecuteUrlHash();

  // chrome::BrowserListObserver overrides:
  void OnBrowserRemoved(Browser* browser) override;

 private:
  Browser* browser_;
  Profile* profile_;
  std::string hash_;

  DISALLOW_COPY_AND_ASSIGN(UrlHashHelper);
};

UrlHashHelper::UrlHashHelper(Browser* browser, const std::string& hash)
    : browser_(browser),
      profile_(browser->profile()),
      hash_(hash) {
  BrowserList::AddObserver(this);
}

UrlHashHelper::~UrlHashHelper() {
  BrowserList::RemoveObserver(this);
}

void UrlHashHelper::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    browser_ = nullptr;
}

void UrlHashHelper::ExecuteUrlHash() {
  if (hash_ == profiles::kUserManagerSelectProfileAppLauncher) {
    AppListService* app_list_service = AppListService::Get();
    app_list_service->ShowForProfile(profile_);
    return;
  }

  Browser* target_browser = browser_;
  if (!target_browser) {
    target_browser = chrome::FindLastActiveWithProfile(profile_);
    if (!target_browser)
      return;
  }

  if (hash_ == profiles::kUserManagerSelectProfileTaskManager)
    chrome::OpenTaskManager(target_browser);
  else if (hash_ == profiles::kUserManagerSelectProfileAboutChrome)
    chrome::ShowAboutChrome(target_browser);
  else if (hash_ == profiles::kUserManagerSelectProfileChromeSettings)
    chrome::ShowSettings(target_browser);
}

void HandleLogRemoveUserWarningShown(const base::ListValue* args) {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER_SHOW_WARNING);
}

}  // namespace

// ProfileUpdateObserver ------------------------------------------------------

class UserManagerScreenHandler::ProfileUpdateObserver
    : public ProfileAttributesStorage::Observer {
 public:
  ProfileUpdateObserver(
      ProfileManager* profile_manager, UserManagerScreenHandler* handler)
      : profile_manager_(profile_manager),
        user_manager_handler_(handler) {
    DCHECK(profile_manager_);
    DCHECK(user_manager_handler_);
    profile_manager_->GetProfileAttributesStorage().AddObserver(this);
  }

  ~ProfileUpdateObserver() override {
    DCHECK(profile_manager_);
    profile_manager_->GetProfileAttributesStorage().RemoveObserver(this);
  }

 private:
  // ProfileAttributesStorage::Observer implementation:
  // If any change has been made to a profile, propagate it to all the
  // visible user manager screens.
  void OnProfileAdded(const base::FilePath& profile_path) override {
    user_manager_handler_->SendUserList();
  }

  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override {
    // TODO(noms): Change 'SendUserList' to 'removeUser' JS-call when
    // UserManager is able to find pod belonging to removed user.
    user_manager_handler_->SendUserList();
  }

  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override {
    user_manager_handler_->SendUserList();
  }

  void OnProfileAvatarChanged(const base::FilePath& profile_path) override {
    user_manager_handler_->SendUserList();
  }

  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override {
    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461175
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "461175 UserManagerScreenHandler::OnProfileHighResAvatarLoaded"));
    user_manager_handler_->SendUserList();
  }

  void OnProfileSigninRequiredChanged(
      const base::FilePath& profile_path) override {
    user_manager_handler_->SendUserList();
  }

  void OnProfileIsOmittedChanged(
      const base::FilePath& profile_path) override {
    user_manager_handler_->SendUserList();
  }

  ProfileManager* profile_manager_;

  UserManagerScreenHandler* user_manager_handler_;  // Weak; owns us.

  DISALLOW_COPY_AND_ASSIGN(ProfileUpdateObserver);
};

// UserManagerScreenHandler ---------------------------------------------------

UserManagerScreenHandler::UserManagerScreenHandler() : weak_ptr_factory_(this) {
  profile_attributes_storage_observer_.reset(
      new UserManagerScreenHandler::ProfileUpdateObserver(
          g_browser_process->profile_manager(), this));

  // TODO(mahmadi): Remove the following once prefs are cleared for everyone.
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);

  const PrefService::Preference* guest_mode_enabled_pref =
      service->FindPreference(prefs::kBrowserGuestModeEnabled);
  const PrefService::Preference* add_person_enabled_pref =
      service->FindPreference(prefs::kBrowserAddPersonEnabled);

  if (base::FeatureList::IsEnabled(features::kMaterialDesignSettings) &&
      (guest_mode_enabled_pref->HasUserSetting() ||
       add_person_enabled_pref->HasUserSetting())) {
    service->ClearPref(guest_mode_enabled_pref->name());
    service->ClearPref(add_person_enabled_pref->name());
    content::RecordAction(
        base::UserMetricsAction("UserManager_Cleared_Legacy_User_Prefs"));
  }
}

UserManagerScreenHandler::~UserManagerScreenHandler() {
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(NULL);
}

void UserManagerScreenHandler::ShowBannerMessage(
    const base::string16& message) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "login.AccountPickerScreen.showBannerMessage",
      base::StringValue(message));
}

void UserManagerScreenHandler::ShowUserPodCustomIcon(
    const AccountId& account_id,
    const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions&
        icon_options) {
  std::unique_ptr<base::DictionaryValue> icon =
      icon_options.ToDictionaryValue();
  if (!icon || icon->empty())
    return;
  web_ui()->CallJavascriptFunctionUnsafe(
      "login.AccountPickerScreen.showUserPodCustomIcon",
      base::StringValue(account_id.GetUserEmail()), *icon);
}

void UserManagerScreenHandler::HideUserPodCustomIcon(
    const AccountId& account_id) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "login.AccountPickerScreen.hideUserPodCustomIcon",
      base::StringValue(account_id.GetUserEmail()));
}

void UserManagerScreenHandler::EnableInput() {
  // Nothing here because UI is not disabled when starting to authenticate.
}

void UserManagerScreenHandler::SetAuthType(
    const AccountId& account_id,
    proximity_auth::ScreenlockBridge::LockHandler::AuthType auth_type,
    const base::string16& auth_value) {
  if (GetAuthType(account_id) ==
      proximity_auth::ScreenlockBridge::LockHandler::FORCE_OFFLINE_PASSWORD)
    return;

  user_auth_type_map_[account_id.GetUserEmail()] = auth_type;
  web_ui()->CallJavascriptFunctionUnsafe(
      "login.AccountPickerScreen.setAuthType",
      base::StringValue(account_id.GetUserEmail()), base::Value(auth_type),
      base::StringValue(auth_value));
}

proximity_auth::ScreenlockBridge::LockHandler::AuthType
UserManagerScreenHandler::GetAuthType(const AccountId& account_id) const {
  const auto it = user_auth_type_map_.find(account_id.GetUserEmail());
  if (it == user_auth_type_map_.end())
    return proximity_auth::ScreenlockBridge::LockHandler::OFFLINE_PASSWORD;
  return it->second;
}

proximity_auth::ScreenlockBridge::LockHandler::ScreenType
UserManagerScreenHandler::GetScreenType() const {
  return proximity_auth::ScreenlockBridge::LockHandler::LOCK_SCREEN;
}

void UserManagerScreenHandler::Unlock(const AccountId& account_id) {
  const base::FilePath path = profiles::GetPathOfProfileWithEmail(
      g_browser_process->profile_manager(), account_id.GetUserEmail());
  if (!path.empty()) {
    authenticating_profile_path_ = path;
    ReportAuthenticationResult(true, ProfileMetrics::AUTH_LOCAL);
  }
}

void UserManagerScreenHandler::AttemptEasySignin(const AccountId& account_id,
                                                 const std::string& secret,
                                                 const std::string& key_label) {
  NOTREACHED();
}

void UserManagerScreenHandler::HandleInitialize(const base::ListValue* args) {
  // If the URL has a hash parameter, store it for later.
  args->GetString(0, &url_hash_);

  SendUserList();
  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.ui.UserManager.showUserManagerScreen",
      base::Value(IsGuestModeEnabled()), base::Value(IsAddPersonEnabled()));

  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(this);
}

void UserManagerScreenHandler::HandleAuthenticatedLaunchUser(
    const base::ListValue* args) {
  const base::Value* profile_path_value;
  if (!args->Get(0, &profile_path_value))
    return;

  base::FilePath profile_path;
  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path))
    return;

  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_path, &entry)) {
    return;
  }

  base::string16 email_address;
  if (!args->GetString(1, &email_address))
    return;

  std::string password;
  if (!args->GetString(2, &password))
    return;

  authenticating_profile_path_ = profile_path;
  email_address_ = base::UTF16ToUTF8(email_address);

  // Only try to validate locally or check the password change detection
  // if we actually have a local credential saved.
  if (!entry->GetLocalAuthCredentials().empty()) {
    if (LocalAuth::ValidateLocalAuthCredentials(entry, password)) {
      ReportAuthenticationResult(true, ProfileMetrics::AUTH_LOCAL);
      return;
    }

    // This could be a mis-typed password or typing a new password while we
    // still have a hash of the old one.  The new way of checking a password
    // change makes use of a token so we do that... if it's available.
    if (!oauth_client_) {
      oauth_client_.reset(new gaia::GaiaOAuthClient(
          content::BrowserContext::GetDefaultStoragePartition(
              web_ui()->GetWebContents()->GetBrowserContext())->
                  GetURLRequestContext()));
    }

    const std::string token = entry->GetPasswordChangeDetectionToken();
    if (!token.empty()) {
      oauth_client_->GetTokenHandleInfo(token, kMaxOAuthRetries, this);
      return;
    }
  }

  content::BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  if (!email_address_.empty()) {
    // In order to support the upgrade case where we have a local hash but no
    // password token, the user must perform a full online reauth.
    UserManagerProfileDialog::ShowReauthDialog(
        browser_context, email_address_, signin_metrics::Reason::REASON_UNLOCK);
  } else if (entry->IsSigninRequired() && entry->IsSupervised()) {
    // Supervised profile will only be locked when force-sign-in is enabled
    // and it shouldn't be unlocked. Display the error message directly via
    // the system profile to avoid profile creation.
    LoginUIServiceFactory::GetForProfile(
        Profile::FromWebUI(web_ui())->GetOriginalProfile())
        ->DisplayLoginResult(nullptr,
                             l10n_util::GetStringUTF16(
                                 IDS_SUPERVISED_USER_NOT_ALLOWED_BY_POLICY),
                             base::string16());
    UserManagerProfileDialog::ShowDialogAndDisplayErrorMessage(browser_context);
  } else {
    // Fresh sign in via user manager without existing email address.
    UserManagerProfileDialog::ShowSigninDialog(browser_context, profile_path);
  }
}

void UserManagerScreenHandler::HandleRemoveUser(const base::ListValue* args) {
  DCHECK(args);
  const base::Value* profile_path_value;
  if (!args->Get(0, &profile_path_value)) {
    NOTREACHED();
    return;
  }

  base::FilePath profile_path;
  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path)) {
    NOTREACHED();
    return;
  }

  DCHECK(profiles::IsMultipleProfilesEnabled());

  if (profiles::AreAllNonChildNonSupervisedProfilesLocked()) {
    web_ui()->CallJavascriptFunctionUnsafe(
        "cr.webUIListenerCallback",
        base::StringValue("show-error-dialog"),
        base::StringValue(l10n_util::GetStringUTF8(
            IDS_USER_MANAGER_REMOVE_PROFILE_PROFILES_LOCKED_ERROR)));
    return;
  }

  // The callback is run if the only profile has been deleted, and a new
  // profile has been created to replace it.
  webui::DeleteProfileAtPath(profile_path,
                             web_ui(),
                             ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
}

void UserManagerScreenHandler::HandleLaunchGuest(const base::ListValue* args) {
  if (IsGuestModeEnabled()) {
    profiles::SwitchToGuestProfile(
        base::Bind(&UserManagerScreenHandler::OnSwitchToProfileComplete,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    // The UI should have prevented the user from allowing the selection of
    // guest mode.
    NOTREACHED();
  }
}

void UserManagerScreenHandler::HandleAreAllProfilesLocked(
    const base::ListValue* args) {
  std::string webui_callback_id;
  CHECK_EQ(1U, args->GetSize());
  bool success = args->GetString(0, &webui_callback_id);
  DCHECK(success);

  AllowJavascript();
  ResolveJavascriptCallback(
      base::StringValue(webui_callback_id),
      base::Value(profiles::AreAllNonChildNonSupervisedProfilesLocked()));
}

void UserManagerScreenHandler::HandleLaunchUser(const base::ListValue* args) {
  const base::Value* profile_path_value = NULL;
  if (!args->Get(0, &profile_path_value))
    return;

  base::FilePath profile_path;
  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path))
    return;

  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_path, &entry)) {
    NOTREACHED();
    return;
  }

  // It's possible that a user breaks into the user-manager page using the
  // JavaScript Inspector and causes a "locked" profile to call this
  // unauthenticated version of "launch" instead of the proper one.  Thus,
  // we have to validate in (secure) C++ code that it really is a profile
  // not needing authentication.  If it is, just ignore the "launch" request.
  if (entry->IsSigninRequired())
    return;
  ProfileMetrics::LogProfileAuthResult(ProfileMetrics::AUTH_UNNECESSARY);

  profiles::SwitchToProfile(
      profile_path, false, /* reuse any existing windows */
      base::Bind(&UserManagerScreenHandler::OnSwitchToProfileComplete,
                 weak_ptr_factory_.GetWeakPtr()),
      ProfileMetrics::SWITCH_PROFILE_MANAGER);
}

void UserManagerScreenHandler::HandleAttemptUnlock(
    const base::ListValue* args) {
  std::string email;
  CHECK(args->GetString(0, &email));
  GetScreenlockRouter(email)
      ->OnAuthAttempted(GetAuthType(AccountId::FromUserEmail(email)), "");
}

void UserManagerScreenHandler::HandleHardlockUserPod(
    const base::ListValue* args) {
  std::string email;
  CHECK(args->GetString(0, &email));
  const AccountId account_id = AccountId::FromUserEmail(email);
  SetAuthType(
      account_id,
      proximity_auth::ScreenlockBridge::LockHandler::FORCE_OFFLINE_PASSWORD,
      base::string16());
  HideUserPodCustomIcon(account_id);
}

void UserManagerScreenHandler::HandleRemoveUserWarningLoadStats(
    const base::ListValue* args) {
  const base::Value* profile_path_value;

  if (!args->Get(0, &profile_path_value))
    return;

  base::FilePath profile_path;

  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path))
    return;

  base::StringValue return_profile_path(profile_path.value());
  Profile* profile = g_browser_process->profile_manager()->
      GetProfileByPath(profile_path);

  if (!profile)
    return;

  if (!chrome::FindAnyBrowser(profile, true)) {
    // If no windows are open for that profile, the statistics in
    // ProfileAttributesStorage are up to date. The statistics in
    // ProfileAttributesStorage are returned because the copy in user_pod_row.js
    // may be outdated. However, if some statistics are missing in
    // ProfileAttributesStorage (i.e. |item.success| is false), then the actual
    // statistics are queried instead.
    base::DictionaryValue return_value;
    profiles::ProfileCategoryStats stats =
        ProfileStatistics::GetProfileStatisticsFromAttributesStorage(
            profile_path);
    bool stats_success = true;
    for (const auto& item : stats) {
      std::unique_ptr<base::DictionaryValue> stat(new base::DictionaryValue);
      stat->SetIntegerWithoutPathExpansion("count", item.count);
      stat->SetBooleanWithoutPathExpansion("success", item.success);
      return_value.SetWithoutPathExpansion(item.category, std::move(stat));
      stats_success &= item.success;
    }
    if (stats_success) {
      web_ui()->CallJavascriptFunctionUnsafe(
          "updateRemoveWarningDialog", base::StringValue(profile_path.value()),
          return_value);
      return;
    }
  }

  ProfileStatisticsFactory::GetForProfile(profile)->GatherStatistics(
      base::Bind(
          &UserManagerScreenHandler::RemoveUserDialogLoadStatsCallback,
          weak_ptr_factory_.GetWeakPtr(), profile_path));
}

void UserManagerScreenHandler::RemoveUserDialogLoadStatsCallback(
    base::FilePath profile_path,
    profiles::ProfileCategoryStats result) {
  // Copy result into return_value.
  base::DictionaryValue return_value;
  for (const auto& item : result) {
    std::unique_ptr<base::DictionaryValue> stat(new base::DictionaryValue);
    stat->SetIntegerWithoutPathExpansion("count", item.count);
    stat->SetBooleanWithoutPathExpansion("success", item.success);
    return_value.SetWithoutPathExpansion(item.category, std::move(stat));
  }
  web_ui()->CallJavascriptFunctionUnsafe(
      "updateRemoveWarningDialog", base::StringValue(profile_path.value()),
      return_value);
}

void UserManagerScreenHandler::HandleGetRemoveWarningDialogMessage(
    const base::ListValue* args) {
  const base::DictionaryValue* arg;
  if (!args->GetDictionary(0, &arg))
    return;

  std::string profile_path("");
  bool is_synced_user = false;
  bool has_errors = false;

  if (!arg->GetString("profilePath", &profile_path) ||
      !arg->GetBoolean("isSyncedUser", &is_synced_user) ||
      !arg->GetBoolean("hasErrors", &has_errors))
    return;

  int total_count = 0;
  if (!arg->GetInteger("totalCount", &total_count))
    return;

  int message_id = is_synced_user ?
      (has_errors ? IDS_LOGIN_POD_USER_REMOVE_WARNING_SYNC_WITH_ERRORS :
                    IDS_LOGIN_POD_USER_REMOVE_WARNING_SYNC) :
      (has_errors ? IDS_LOGIN_POD_USER_REMOVE_WARNING_NONSYNC_WITH_ERRORS :
                    IDS_LOGIN_POD_USER_REMOVE_WARNING_NONSYNC);

  base::StringValue message = base::StringValue(
      l10n_util::GetPluralStringFUTF16(message_id, total_count));

  web_ui()->CallJavascriptFunctionUnsafe("updateRemoveWarningDialogSetMessage",
                                         base::StringValue(profile_path),
                                         message, base::Value(total_count));
}

void UserManagerScreenHandler::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  // Password is unchanged so user just mistyped it.  Ask again.
  ReportAuthenticationResult(false, ProfileMetrics::AUTH_FAILED);
}

void UserManagerScreenHandler::OnOAuthError() {
  // Password has changed.  Go through online signin flow.
  DCHECK(!email_address_.empty());
  oauth_client_.reset();
  UserManagerProfileDialog::ShowReauthDialog(
      web_ui()->GetWebContents()->GetBrowserContext(), email_address_,
      signin_metrics::Reason::REASON_UNLOCK);
}

void UserManagerScreenHandler::OnNetworkError(int response_code) {
  // Inconclusive but can't do real signin without being online anyway.
    oauth_client_.reset();
    ReportAuthenticationResult(false, ProfileMetrics::AUTH_FAILED_OFFLINE);
}

void UserManagerScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kJsApiUserManagerInitialize,
      base::Bind(&UserManagerScreenHandler::HandleInitialize,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiUserManagerAuthLaunchUser,
      base::Bind(&UserManagerScreenHandler::HandleAuthenticatedLaunchUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiUserManagerLaunchGuest,
      base::Bind(&UserManagerScreenHandler::HandleLaunchGuest,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiUserManagerLaunchUser,
      base::Bind(&UserManagerScreenHandler::HandleLaunchUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiUserManagerRemoveUser,
      base::Bind(&UserManagerScreenHandler::HandleRemoveUser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiUserManagerAttemptUnlock,
      base::Bind(&UserManagerScreenHandler::HandleAttemptUnlock,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiUserManagerLogRemoveUserWarningShown,
      base::Bind(&HandleLogRemoveUserWarningShown));
  web_ui()->RegisterMessageCallback(kJsApiUserManagerRemoveUserWarningLoadStats,
      base::Bind(&UserManagerScreenHandler::HandleRemoveUserWarningLoadStats,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiUserManagerGetRemoveWarningDialogMessage,
      base::Bind(&UserManagerScreenHandler::HandleGetRemoveWarningDialogMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiUserManagerAreAllProfilesLocked,
      base::Bind(&UserManagerScreenHandler::HandleAreAllProfilesLocked,
                 base::Unretained(this)));

  const content::WebUI::MessageCallback& kDoNothingCallback =
      base::Bind(&HandleAndDoNothing);

  // Unused callbacks from screen_account_picker.js
  web_ui()->RegisterMessageCallback("accountPickerReady", kDoNothingCallback);
  web_ui()->RegisterMessageCallback("loginUIStateChanged", kDoNothingCallback);
  web_ui()->RegisterMessageCallback("hideCaptivePortal", kDoNothingCallback);
  web_ui()->RegisterMessageCallback("getTouchViewState", kDoNothingCallback);
  // Unused callbacks from display_manager.js
  web_ui()->RegisterMessageCallback("showAddUser", kDoNothingCallback);
  web_ui()->RegisterMessageCallback("loadWallpaper", kDoNothingCallback);
  web_ui()->RegisterMessageCallback("updateCurrentScreen", kDoNothingCallback);
  web_ui()->RegisterMessageCallback("loginVisible", kDoNothingCallback);
  // Unused callbacks from user_pod_row.js
  web_ui()->RegisterMessageCallback("focusPod", kDoNothingCallback);
  web_ui()->RegisterMessageCallback("noPodFocused", kDoNothingCallback);
}

void UserManagerScreenHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  // For Control Bar.
  localized_strings->SetString("signedIn",
      l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_ACTIVE_USER));
  localized_strings->SetString("addUser",
      l10n_util::GetStringUTF16(IDS_ADD_USER_BUTTON));
  localized_strings->SetString("cancel", l10n_util::GetStringUTF16(IDS_CANCEL));
  localized_strings->SetString("browseAsGuest",
      l10n_util::GetStringUTF16(IDS_GO_INCOGNITO_BUTTON));
  localized_strings->SetString("signOutUser",
      l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT));
  localized_strings->SetString("addSupervisedUser",
      l10n_util::GetStringUTF16(IDS_CREATE_LEGACY_SUPERVISED_USER_MENU_LABEL));

  // For AccountPickerScreen.
  localized_strings->SetString("screenType", "login-add-user");
  localized_strings->SetString("highlightStrength", "normal");
  localized_strings->SetString("title",
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  localized_strings->SetString("passwordHint",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT));
  localized_strings->SetString("signingIn",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_SIGNING_IN));
  localized_strings->SetString("podMenuButtonAccessibleName",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_MENU_BUTTON_ACCESSIBLE_NAME));
  localized_strings->SetString("podMenuRemoveItemAccessibleName",
      l10n_util::GetStringUTF16(
          IDS_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME));
  localized_strings->SetString("removeUser",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_BUTTON));
  localized_strings->SetString("passwordFieldAccessibleName",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME));
  localized_strings->SetString("bootIntoWallpaper", "off");

  // For AccountPickerScreen, the remove user warning overlay.
  localized_strings->SetString("removeUserWarningButtonTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_BUTTON));
  localized_strings->SetString("removeUserWarningTextNonSyncNoStats",
      l10n_util::GetStringUTF16(
          IDS_LOGIN_POD_USER_REMOVE_WARNING_NONSYNC_NOSTATS));
  localized_strings->SetString("removeUserWarningTextNonSyncCalculating",
      l10n_util::GetStringUTF16(
          IDS_LOGIN_POD_USER_REMOVE_WARNING_NONSYNC_CALCULATING));
  localized_strings->SetString("removeUserWarningTextHistory",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_HISTORY));
  localized_strings->SetString("removeUserWarningTextPasswords",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_PASSWORDS));
  localized_strings->SetString("removeUserWarningTextBookmarks",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_BOOKMARKS));
  localized_strings->SetString("removeUserWarningTextSettings",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_SETTINGS));
  localized_strings->SetString("removeUserWarningTextCalculating",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_CALCULATING));
  localized_strings->SetString("removeUserWarningTextSyncNoStats",
      l10n_util::GetStringUTF16(
          IDS_LOGIN_POD_USER_REMOVE_WARNING_SYNC_NOSTATS));
  localized_strings->SetString("removeUserWarningTextSyncCalculating",
      l10n_util::GetStringUTF16(
          IDS_LOGIN_POD_USER_REMOVE_WARNING_SYNC_CALCULATING));
  localized_strings->SetString("removeLegacySupervisedUserWarningText",
      l10n_util::GetStringFUTF16(
          IDS_LOGIN_POD_LEGACY_SUPERVISED_USER_REMOVE_WARNING,
          base::UTF8ToUTF16(
              chrome::kLegacySupervisedUserManagementDisplayURL)));
  localized_strings->SetString(
      "removeNonOwnerUserWarningText",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING));

  // Strings needed for the User Manager tutorial slides.
  localized_strings->SetString("tutorialNext",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_NEXT));
  localized_strings->SetString("tutorialDone",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_DONE));
  localized_strings->SetString("slideWelcomeTitle",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_INTRO_TITLE));
  localized_strings->SetString("slideWelcomeText",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_INTRO_TEXT));
  localized_strings->SetString("slideYourChromeTitle",
      l10n_util::GetStringUTF16(
          IDS_USER_MANAGER_TUTORIAL_SLIDE_YOUR_CHROME_TITLE));
  localized_strings->SetString("slideYourChromeText", l10n_util::GetStringUTF16(
      IDS_USER_MANAGER_TUTORIAL_SLIDE_YOUR_CHROME_TEXT));
  localized_strings->SetString("slideGuestsTitle",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_GUEST_TITLE));
  localized_strings->SetString("slideGuestsText",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_GUEST_TEXT));
  localized_strings->SetString("slideFriendsTitle",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_FRIENDS_TITLE));
  localized_strings->SetString("slideFriendsText",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_FRIENDS_TEXT));
  localized_strings->SetString("slideCompleteTitle",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_OUTRO_TITLE));
  localized_strings->SetString("slideCompleteText",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_OUTRO_TEXT));
  localized_strings->SetString("slideCompleteUserNotFound",
      l10n_util::GetStringUTF16(
          IDS_USER_MANAGER_TUTORIAL_SLIDE_OUTRO_USER_NOT_FOUND));
  localized_strings->SetString("slideCompleteAddUser",
      l10n_util::GetStringUTF16(
          IDS_USER_MANAGER_TUTORIAL_SLIDE_OUTRO_ADD_USER));

  // Strings needed for the user_pod_template public account div, but not ever
  // actually displayed for desktop users.
  localized_strings->SetString("publicAccountReminder", base::string16());
  localized_strings->SetString("publicSessionLanguageAndInput",
                               base::string16());
  localized_strings->SetString("publicAccountEnter", base::string16());
  localized_strings->SetString("publicAccountEnterAccessibleName",
                               base::string16());
  localized_strings->SetString("publicAccountMonitoringWarning",
                               base::string16());
  localized_strings->SetString("publicAccountLearnMore", base::string16());
  localized_strings->SetString("publicAccountMonitoringInfo", base::string16());
  localized_strings->SetString("publicAccountMonitoringInfoItem1",
                               base::string16());
  localized_strings->SetString("publicAccountMonitoringInfoItem2",
                               base::string16());
  localized_strings->SetString("publicAccountMonitoringInfoItem3",
                               base::string16());
  localized_strings->SetString("publicAccountMonitoringInfoItem4",
                               base::string16());
  localized_strings->SetString("publicSessionSelectLanguage", base::string16());
  localized_strings->SetString("publicSessionSelectKeyboard", base::string16());
  localized_strings->SetString("signinBannerText", base::string16());
  localized_strings->SetString("launchAppButton", base::string16());
  localized_strings->SetString("multiProfilesRestrictedPolicyTitle",
                               base::string16());
  localized_strings->SetString("multiProfilesNotAllowedPolicyMsg",
                                base::string16());
  localized_strings->SetString("multiProfilesPrimaryOnlyPolicyMsg",
                                base::string16());
  localized_strings->SetString("multiProfilesOwnerPrimaryOnlyMsg",
                                base::string16());

  // Error message when trying to add a profile while all profiles are locked.
  localized_strings->SetString("addProfileAllProfilesLockedError",
      l10n_util::GetStringUTF16(
          IDS_USER_MANAGER_ADD_PROFILE_PROFILES_LOCKED_ERROR));
  // Error message when trying to browse as guest while all profiles are locked.
  localized_strings->SetString("browseAsGuestAllProfilesLockedError",
      l10n_util::GetStringUTF16(
          IDS_USER_MANAGER_GO_GUEST_PROFILES_LOCKED_ERROR));
}

void UserManagerScreenHandler::SendUserList() {
  base::ListValue users_list;
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetAllProfilesAttributesSortedByName();
  user_auth_type_map_.clear();

  // Profile deletion is not allowed in Metro mode.
  bool can_remove = true;
#if defined(USE_ASH)
  can_remove = !ash::Shell::HasInstance();
#endif

  for (const ProfileAttributesEntry* entry : entries) {
    // Don't show profiles still in the middle of being set up as new legacy
    // supervised users.
    if (entry->IsOmitted())
      continue;

    std::unique_ptr<base::DictionaryValue> profile_value(
        new base::DictionaryValue());
    base::FilePath profile_path = entry->GetPath();

    profile_value->SetString(kKeyUsername, entry->GetUserName());
    profile_value->SetString(kKeyEmailAddress, entry->GetUserName());
    profile_value->SetString(kKeyDisplayName,
                             profiles::GetAvatarNameForProfile(profile_path));
    profile_value->Set(kKeyProfilePath,
                       base::CreateFilePathValue(profile_path));
    profile_value->SetBoolean(kKeyPublicAccount, false);
    profile_value->SetBoolean(kKeyLegacySupervisedUser,
                              entry->IsLegacySupervised());
    profile_value->SetBoolean(kKeyChildUser, entry->IsChild());
    profile_value->SetBoolean(kKeyNeedsSignin, entry->IsSigninRequired());
    profile_value->SetBoolean(kKeyHasLocalCreds,
                              !entry->GetLocalAuthCredentials().empty());
    profile_value->SetBoolean(kKeyIsOwner, false);
    profile_value->SetBoolean(kKeyCanRemove, can_remove);
    profile_value->SetBoolean(kKeyIsDesktop, true);
    profile_value->SetString(kKeyAvatarUrl, GetAvatarImage(entry));

    profiles::ProfileCategoryStats stats =
        ProfileStatistics::GetProfileStatisticsFromAttributesStorage(
            profile_path);
    std::unique_ptr<base::DictionaryValue> stats_dict(
        new base::DictionaryValue);
    for (const auto& item : stats) {
      std::unique_ptr<base::DictionaryValue> stat(new base::DictionaryValue);
      stat->SetIntegerWithoutPathExpansion("count", item.count);
      stat->SetBooleanWithoutPathExpansion("success", item.success);
      stats_dict->SetWithoutPathExpansion(item.category, std::move(stat));
    }
    profile_value->SetWithoutPathExpansion(kKeyStatistics,
                                           std::move(stats_dict));

    // GetProfileByPath returns a pointer if the profile is fully loaded, NULL
    // otherwise.
    Profile* profile =
        g_browser_process->profile_manager()->GetProfileByPath(profile_path);
    profile_value->SetBoolean(kKeyIsProfileLoaded, profile != nullptr);

    users_list.Append(std::move(profile_value));
  }

  web_ui()->CallJavascriptFunctionUnsafe("login.AccountPickerScreen.loadUsers",
                                         users_list,
                                         base::Value(IsGuestModeEnabled()));

  // This is the latest C++ code we have in the flow to show the UserManager.
  // This may be invoked more than once per UserManager lifetime; the
  // UserManager will ensure all relevant logging only happens once.
  UserManager::OnUserManagerShown();
}

void UserManagerScreenHandler::ReportAuthenticationResult(
    bool success,
    ProfileMetrics::ProfileAuth auth) {
  ProfileMetrics::LogProfileAuthResult(auth);
  email_address_.clear();

  if (success) {
    profiles::SwitchToProfile(
        authenticating_profile_path_, true,
        base::Bind(&UserManagerScreenHandler::OnSwitchToProfileComplete,
                   weak_ptr_factory_.GetWeakPtr()),
        ProfileMetrics::SWITCH_PROFILE_UNLOCK);
  } else {
    web_ui()->CallJavascriptFunctionUnsafe(
        "cr.ui.UserManager.showSignInError", base::Value(0),
        base::StringValue(l10n_util::GetStringUTF8(
            auth == ProfileMetrics::AUTH_FAILED_OFFLINE
                ? IDS_LOGIN_ERROR_AUTHENTICATING_OFFLINE
                : IDS_LOGIN_ERROR_AUTHENTICATING)),
        base::StringValue(""), base::Value(0));
  }
}

void UserManagerScreenHandler::OnBrowserWindowReady(Browser* browser) {
  DCHECK(browser);
  DCHECK(browser->window());

  // Unlock the profile after browser opens so startup can read the lock bit.
  // Any necessary authentication must have been successful to reach this point.
  if (!browser->profile()->IsGuestSession()) {
    ProfileAttributesEntry* entry = nullptr;
    bool has_entry = g_browser_process->profile_manager()->
        GetProfileAttributesStorage().
        GetProfileAttributesWithPath(browser->profile()->GetPath(), &entry);
    DCHECK(has_entry);
    entry->SetIsSigninRequired(false);
  }

  if (!url_hash_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&UrlHashHelper::ExecuteUrlHash,
                   base::Owned(new UrlHashHelper(browser, url_hash_))));
  }

  // This call is last as it deletes this object.
  UserManager::Hide();
}

void UserManagerScreenHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_WINDOW_READY, type);

  // Only respond to one Browser Window Ready event.
  registrar_.Remove(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                    content::NotificationService::AllSources());
  OnBrowserWindowReady(content::Source<Browser>(source).ptr());
}

// This callback is run after switching to a new profile has finished. This
// means either a new browser has been created (but not the window), or an
// existing one has been found. The HideUserManager task needs to be posted
// since closing the User Manager before the window is created can flakily
// cause Chrome to close.
void UserManagerScreenHandler::OnSwitchToProfileComplete(
    Profile* profile, Profile::CreateStatus profile_create_status) {
  Browser* browser = chrome::FindAnyBrowser(profile, false);
  if (browser && browser->window()) {
    OnBrowserWindowReady(browser);
  } else {
    registrar_.Add(this,
                   chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                   content::NotificationService::AllSources());
  }
}
