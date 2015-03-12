// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/user_manager_screen_handler.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
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
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_service.h"
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
#include "ash/shell.h"
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

// JS API callback names.
const char kJsApiUserManagerInitialize[] = "userManagerInitialize";
const char kJsApiUserManagerAddUser[] = "addUser";
const char kJsApiUserManagerAuthLaunchUser[] = "authenticatedLaunchUser";
const char kJsApiUserManagerLaunchGuest[] = "launchGuest";
const char kJsApiUserManagerLaunchUser[] = "launchUser";
const char kJsApiUserManagerRemoveUser[] = "removeUser";
const char kJsApiUserManagerAttemptUnlock[] = "attemptUnlock";

const size_t kAvatarIconSize = 180;

void HandleAndDoNothing(const base::ListValue* args) {
}

// This callback is run if the only profile has been deleted, and a new
// profile has been created to replace it.
void OpenNewWindowForProfile(
    chrome::HostDesktopType desktop_type,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;
  profiles::FindOrCreateNewWindowForProfile(
    profile,
    chrome::startup::IS_PROCESS_STARTUP,
    chrome::startup::IS_FIRST_RUN,
    desktop_type,
    false);
}

std::string GetAvatarImageAtIndex(
    size_t index, const ProfileInfoCache& info_cache) {
  bool is_gaia_picture =
      info_cache.IsUsingGAIAPictureOfProfileAtIndex(index) &&
      info_cache.GetGAIAPictureOfProfileAtIndex(index);

  // If the avatar is too small (i.e. the old-style low resolution avatar),
  // it will be pixelated when displayed in the User Manager, so we should
  // return the placeholder avatar instead.
  gfx::Image avatar_image = info_cache.GetAvatarIconOfProfileAtIndex(index);
  if (avatar_image.Width() <= profiles::kAvatarIconWidth ||
      avatar_image.Height() <= profiles::kAvatarIconHeight ) {
    avatar_image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }
  gfx::Image resized_image = profiles::GetSizedAvatarIcon(
      avatar_image, is_gaia_picture, kAvatarIconSize, kAvatarIconSize);
  return webui::GetBitmapDataUrl(resized_image.AsBitmap());
}

size_t GetIndexOfProfileWithEmail(const ProfileInfoCache& info_cache,
                                  const std::string& email) {
  const base::string16& profile_email = base::UTF8ToUTF16(email);
  for (size_t i = 0; i < info_cache.GetNumberOfProfiles(); ++i) {
    if (info_cache.GetUserNameOfProfileAtIndex(i) == profile_email)
      return i;
  }
  return std::string::npos;
}

extensions::ScreenlockPrivateEventRouter* GetScreenlockRouter(
    const std::string& email) {
  const ProfileInfoCache& info_cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const size_t profile_index = GetIndexOfProfileWithEmail(info_cache, email);
  Profile* profile = g_browser_process->profile_manager()
      ->GetProfileByPath(info_cache.GetPathOfProfileAtIndex(profile_index));
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
  chrome::HostDesktopType desktop_type_;
  std::string hash_;

  DISALLOW_COPY_AND_ASSIGN(UrlHashHelper);
};

UrlHashHelper::UrlHashHelper(Browser* browser, const std::string& hash)
    : browser_(browser),
      profile_(browser->profile()),
      desktop_type_(browser->host_desktop_type()),
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
    AppListService* app_list_service = AppListService::Get(desktop_type_);
    app_list_service->ShowForProfile(profile_);
    return;
  }

  Browser* target_browser = browser_;
  if (!target_browser) {
    target_browser = chrome::FindLastActiveWithProfile(profile_, desktop_type_);
    if (!target_browser)
      return;
  }

  if (hash_ == profiles::kUserManagerSelectProfileTaskManager)
    chrome::OpenTaskManager(target_browser);
  else if (hash_ == profiles::kUserManagerSelectProfileAboutChrome)
    chrome::ShowAboutChrome(target_browser);
  else if (hash_ == profiles::kUserManagerSelectProfileChromeSettings)
    chrome::ShowSettings(target_browser);
  else if (hash_ == profiles::kUserManagerSelectProfileChromeMemory)
    chrome::ShowMemory(target_browser);
}

}  // namespace

// ProfileUpdateObserver ------------------------------------------------------

class UserManagerScreenHandler::ProfileUpdateObserver
    : public ProfileInfoCacheObserver {
 public:
  ProfileUpdateObserver(
      ProfileManager* profile_manager, UserManagerScreenHandler* handler)
      : profile_manager_(profile_manager),
        user_manager_handler_(handler) {
    DCHECK(profile_manager_);
    DCHECK(user_manager_handler_);
    profile_manager_->GetProfileInfoCache().AddObserver(this);
  }

  ~ProfileUpdateObserver() override {
    DCHECK(profile_manager_);
    profile_manager_->GetProfileInfoCache().RemoveObserver(this);
  }

 private:
  // ProfileInfoCacheObserver implementation:
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

  ProfileManager* profile_manager_;

  UserManagerScreenHandler* user_manager_handler_;  // Weak; owns us.

  DISALLOW_COPY_AND_ASSIGN(ProfileUpdateObserver);
};

// UserManagerScreenHandler ---------------------------------------------------

UserManagerScreenHandler::UserManagerScreenHandler()
    : desktop_type_(chrome::GetActiveDesktop()),
      weak_ptr_factory_(this) {
  profileInfoCacheObserver_.reset(
      new UserManagerScreenHandler::ProfileUpdateObserver(
          g_browser_process->profile_manager(), this));
}

UserManagerScreenHandler::~UserManagerScreenHandler() {
  ScreenlockBridge::Get()->SetLockHandler(NULL);
}

void UserManagerScreenHandler::ShowBannerMessage(
    const base::string16& message) {
  web_ui()->CallJavascriptFunction(
      "login.AccountPickerScreen.showBannerMessage",
      base::StringValue(message));
}

void UserManagerScreenHandler::ShowUserPodCustomIcon(
    const std::string& user_email,
    const ScreenlockBridge::UserPodCustomIconOptions& icon_options) {
  scoped_ptr<base::DictionaryValue> icon = icon_options.ToDictionaryValue();
  if (!icon || icon->empty())
    return;
  web_ui()->CallJavascriptFunction(
      "login.AccountPickerScreen.showUserPodCustomIcon",
      base::StringValue(user_email),
      *icon);
}

void UserManagerScreenHandler::HideUserPodCustomIcon(
    const std::string& user_email) {
  web_ui()->CallJavascriptFunction(
      "login.AccountPickerScreen.hideUserPodCustomIcon",
      base::StringValue(user_email));
}

void UserManagerScreenHandler::EnableInput() {
  // Nothing here because UI is not disabled when starting to authenticate.
}

void UserManagerScreenHandler::SetAuthType(
    const std::string& user_email,
    ScreenlockBridge::LockHandler::AuthType auth_type,
    const base::string16& auth_value) {
  if (GetAuthType(user_email) ==
          ScreenlockBridge::LockHandler::FORCE_OFFLINE_PASSWORD)
    return;

  user_auth_type_map_[user_email] = auth_type;
  web_ui()->CallJavascriptFunction(
      "login.AccountPickerScreen.setAuthType",
      base::StringValue(user_email),
      base::FundamentalValue(auth_type),
      base::StringValue(auth_value));
}

ScreenlockBridge::LockHandler::AuthType UserManagerScreenHandler::GetAuthType(
      const std::string& user_email) const {
  UserAuthTypeMap::const_iterator it = user_auth_type_map_.find(user_email);
  if (it == user_auth_type_map_.end())
    return ScreenlockBridge::LockHandler::OFFLINE_PASSWORD;
  return it->second;
}

ScreenlockBridge::LockHandler::ScreenType
UserManagerScreenHandler::GetScreenType() const {
  return ScreenlockBridge::LockHandler::LOCK_SCREEN;
}

void UserManagerScreenHandler::Unlock(const std::string& user_email) {
  const ProfileInfoCache& info_cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const size_t profile_index =
      GetIndexOfProfileWithEmail(info_cache, user_email);
  DCHECK_LT(profile_index, info_cache.GetNumberOfProfiles());

  authenticating_profile_index_ = profile_index;
  ReportAuthenticationResult(true, ProfileMetrics::AUTH_LOCAL);
}

void UserManagerScreenHandler::AttemptEasySignin(
    const std::string& user_email,
    const std::string& secret,
    const std::string& key_label) {
  NOTREACHED();
}

void UserManagerScreenHandler::HandleInitialize(const base::ListValue* args) {
  // If the URL has a hash parameter, store it for later.
  args->GetString(0, &url_hash_);

  SendUserList();
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.showUserManagerScreen",
      base::FundamentalValue(IsGuestModeEnabled()),
      base::FundamentalValue(IsAddPersonEnabled()));
  desktop_type_ = chrome::GetHostDesktopTypeForNativeView(
      web_ui()->GetWebContents()->GetNativeView());

  ScreenlockBridge::Get()->SetLockHandler(this);
}

void UserManagerScreenHandler::HandleAddUser(const base::ListValue* args) {
  if (!IsAddPersonEnabled()) {
    // The 'Add User' UI should not be showing.
    NOTREACHED();
    return;
  }
  profiles::CreateAndSwitchToNewProfile(
      desktop_type_,
      base::Bind(&UserManagerScreenHandler::OnSwitchToProfileComplete,
                 weak_ptr_factory_.GetWeakPtr()),
      ProfileMetrics::ADD_NEW_USER_MANAGER);
}

void UserManagerScreenHandler::HandleAuthenticatedLaunchUser(
    const base::ListValue* args) {
  const base::Value* profile_path_value;
  if (!args->Get(0, &profile_path_value))
    return;

  base::FilePath profile_path;
  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path))
    return;

  base::string16 email_address;
  if (!args->GetString(1, &email_address))
    return;

  std::string password;
  if (!args->GetString(2, &password))
    return;

  const ProfileInfoCache& info_cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = info_cache.GetIndexOfProfileWithPath(profile_path);

  if (profile_index == std::string::npos) {
    NOTREACHED();
    return;
  }

  authenticating_profile_index_ = profile_index;
  if (!LocalAuth::ValidateLocalAuthCredentials(profile_index, password)) {
    // Make a second attempt via an on-line authentication call.  This handles
    // profiles that are missing sign-in credentials and also cases where the
    // password has been changed externally.
    client_login_.reset(new GaiaAuthFetcher(
        this,
        GaiaConstants::kChromeSource,
        web_ui()->GetWebContents()->GetBrowserContext()->GetRequestContext()));

    client_login_->StartClientLogin(
        base::UTF16ToUTF8(email_address),
        password,
        GaiaConstants::kSyncService,
        std::string(),
        std::string(),
        GaiaAuthFetcher::HostedAccountsAllowed);
    password_attempt_ = password;
    return;
  }

  ReportAuthenticationResult(true, ProfileMetrics::AUTH_LOCAL);
}

void UserManagerScreenHandler::HandleRemoveUser(const base::ListValue* args) {
  DCHECK(args);
  const base::Value* profile_path_value;
  if (!args->Get(0, &profile_path_value))
    return;

  base::FilePath profile_path;
  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path))
    return;

  if (!profiles::IsMultipleProfilesEnabled())
    return;

  g_browser_process->profile_manager()->ScheduleProfileForDeletion(
      profile_path,
      base::Bind(&OpenNewWindowForProfile, desktop_type_));
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
}

void UserManagerScreenHandler::HandleLaunchGuest(const base::ListValue* args) {
  if (IsGuestModeEnabled()) {
    profiles::SwitchToGuestProfile(
        desktop_type_,
        base::Bind(&UserManagerScreenHandler::OnSwitchToProfileComplete,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    // The UI should have prevented the user from allowing the selection of
    // guest mode.
    NOTREACHED();
  }
}

void UserManagerScreenHandler::HandleLaunchUser(const base::ListValue* args) {
  const base::Value* profile_path_value = NULL;
  if (!args->Get(0, &profile_path_value))
    return;

  base::FilePath profile_path;
  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path))
    return;

  const ProfileInfoCache& info_cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = info_cache.GetIndexOfProfileWithPath(profile_path);

  if (profile_index == std::string::npos) {
    NOTREACHED();
    return;
  }

  // It's possible that a user breaks into the user-manager page using the
  // JavaScript Inspector and causes a "locked" profile to call this
  // unauthenticated version of "launch" instead of the proper one.  Thus,
  // we have to validate in (secure) C++ code that it really is a profile
  // not needing authentication.  If it is, just ignore the "launch" request.
  if (info_cache.ProfileIsSigninRequiredAtIndex(profile_index))
    return;
  ProfileMetrics::LogProfileAuthResult(ProfileMetrics::AUTH_UNNECESSARY);

  profiles::SwitchToProfile(
      profile_path,
      desktop_type_,
      false,  /* reuse any existing windows */
      base::Bind(&UserManagerScreenHandler::OnSwitchToProfileComplete,
                 weak_ptr_factory_.GetWeakPtr()),
      ProfileMetrics::SWITCH_PROFILE_MANAGER);
}

void UserManagerScreenHandler::HandleAttemptUnlock(
    const base::ListValue* args) {
  std::string email;
  CHECK(args->GetString(0, &email));
  GetScreenlockRouter(email)->OnAuthAttempted(GetAuthType(email), "");
}

void UserManagerScreenHandler::HandleHardlockUserPod(
    const base::ListValue* args) {
  std::string email;
  CHECK(args->GetString(0, &email));
  SetAuthType(email,
              ScreenlockBridge::LockHandler::FORCE_OFFLINE_PASSWORD,
              base::string16());
  HideUserPodCustomIcon(email);
}

void UserManagerScreenHandler::OnClientLoginSuccess(
    const ClientLoginResult& result) {
  LocalAuth::SetLocalAuthCredentials(authenticating_profile_index_,
                                     password_attempt_);
  ReportAuthenticationResult(true, ProfileMetrics::AUTH_ONLINE);
}

void UserManagerScreenHandler::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  const GoogleServiceAuthError::State state = error.state();
  // Some "error" results mean the password was correct but some other action
  // should be taken.  For our purposes, we only care that the password was
  // correct so count those as a success.
  bool success = (state == GoogleServiceAuthError::NONE ||
                  state == GoogleServiceAuthError::CAPTCHA_REQUIRED ||
                  state == GoogleServiceAuthError::TWO_FACTOR ||
                  state == GoogleServiceAuthError::ACCOUNT_DELETED ||
                  state == GoogleServiceAuthError::ACCOUNT_DISABLED ||
                  state == GoogleServiceAuthError::WEB_LOGIN_REQUIRED);

  // If the password was correct, the user must have changed it since the
  // profile was locked.  Save the password to streamline future unlocks.
  if (success) {
    DCHECK(!password_attempt_.empty());
    LocalAuth::SetLocalAuthCredentials(authenticating_profile_index_,
                                       password_attempt_);
  }

  bool offline = (state == GoogleServiceAuthError::CONNECTION_FAILED ||
                  state == GoogleServiceAuthError::SERVICE_UNAVAILABLE ||
                  state == GoogleServiceAuthError::REQUEST_CANCELED);
  ProfileMetrics::ProfileAuth failure_metric =
      offline ? ProfileMetrics::AUTH_FAILED_OFFLINE :
                ProfileMetrics::AUTH_FAILED;
  ReportAuthenticationResult(
      success, success ? ProfileMetrics::AUTH_ONLINE : failure_metric);
}

void UserManagerScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kJsApiUserManagerInitialize,
      base::Bind(&UserManagerScreenHandler::HandleInitialize,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiUserManagerAddUser,
      base::Bind(&UserManagerScreenHandler::HandleAddUser,
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
}

void UserManagerScreenHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  // For Control Bar.
  localized_strings->SetString("signedIn",
      l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_ACTIVE_USER));
  localized_strings->SetString("signinButton",
      l10n_util::GetStringUTF16(IDS_LOGIN_BUTTON));
  localized_strings->SetString("addUser",
      l10n_util::GetStringUTF16(IDS_ADD_USER_BUTTON));
  localized_strings->SetString("cancel", l10n_util::GetStringUTF16(IDS_CANCEL));
  localized_strings->SetString("browseAsGuest",
      l10n_util::GetStringUTF16(IDS_GO_INCOGNITO_BUTTON));
  localized_strings->SetString("signOutUser",
      l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT));

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
  localized_strings->SetString("removeUserWarningText",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING));
  localized_strings->SetString("removeLegacySupervisedUserWarningText",
      l10n_util::GetStringFUTF16(
          IDS_LOGIN_POD_LEGACY_SUPERVISED_USER_REMOVE_WARNING,
          base::UTF8ToUTF16(chrome::kSupervisedUserManagementDisplayURL)));

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
}

void UserManagerScreenHandler::SendUserList() {
  base::ListValue users_list;
  const ProfileInfoCache& info_cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  user_auth_type_map_.clear();

  // Profile deletion is not allowed in Metro mode.
  bool can_remove = true;
#if defined(USE_ASH)
  can_remove = !ash::Shell::HasInstance();
#endif

  for (size_t i = 0; i < info_cache.GetNumberOfProfiles(); ++i) {
    base::DictionaryValue* profile_value = new base::DictionaryValue();
    base::FilePath profile_path = info_cache.GetPathOfProfileAtIndex(i);

    profile_value->SetString(
        kKeyUsername, info_cache.GetUserNameOfProfileAtIndex(i));
    profile_value->SetString(
        kKeyEmailAddress, info_cache.GetUserNameOfProfileAtIndex(i));
    profile_value->SetString(
        kKeyDisplayName,
        profiles::GetAvatarNameForProfile(profile_path));
    profile_value->Set(
        kKeyProfilePath, base::CreateFilePathValue(profile_path));
    profile_value->SetBoolean(kKeyPublicAccount, false);
    profile_value->SetBoolean(kKeyLegacySupervisedUser,
                              info_cache.ProfileIsLegacySupervisedAtIndex(i));
    profile_value->SetBoolean(
        kKeyChildUser, info_cache.ProfileIsChildAtIndex(i));
    profile_value->SetBoolean(
        kKeyNeedsSignin, info_cache.ProfileIsSigninRequiredAtIndex(i));
    profile_value->SetBoolean(kKeyIsOwner, false);
    profile_value->SetBoolean(kKeyCanRemove, can_remove);
    profile_value->SetBoolean(kKeyIsDesktop, true);
    profile_value->SetString(
        kKeyAvatarUrl, GetAvatarImageAtIndex(i, info_cache));

    users_list.Append(profile_value);
  }

  web_ui()->CallJavascriptFunction("login.AccountPickerScreen.loadUsers",
      users_list, base::FundamentalValue(IsGuestModeEnabled()));
}

void UserManagerScreenHandler::ReportAuthenticationResult(
    bool success,
    ProfileMetrics::ProfileAuth auth) {
  ProfileMetrics::LogProfileAuthResult(auth);
  password_attempt_.clear();

  if (success) {
    const ProfileInfoCache& info_cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    base::FilePath path = info_cache.GetPathOfProfileAtIndex(
        authenticating_profile_index_);
    profiles::SwitchToProfile(
        path,
        desktop_type_,
        true,
        base::Bind(&UserManagerScreenHandler::OnSwitchToProfileComplete,
                   weak_ptr_factory_.GetWeakPtr()),
        ProfileMetrics::SWITCH_PROFILE_UNLOCK);
  } else {
    web_ui()->CallJavascriptFunction(
        "cr.ui.Oobe.showSignInError",
        base::FundamentalValue(0),
        base::StringValue(l10n_util::GetStringUTF8(
            auth == ProfileMetrics::AUTH_FAILED_OFFLINE ?
                IDS_LOGIN_ERROR_AUTHENTICATING_OFFLINE :
                IDS_LOGIN_ERROR_AUTHENTICATING)),
        base::StringValue(""),
        base::FundamentalValue(0));
  }
}

void UserManagerScreenHandler::OnBrowserWindowReady(Browser* browser) {
  DCHECK(browser);
  DCHECK(browser->window());

  // Unlock the profile after browser opens so startup can read the lock bit.
  // Any necessary authentication must have been successful to reach this point.
  if (!browser->profile()->IsGuestSession()) {
    ProfileInfoCache& info_cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t index = info_cache.GetIndexOfProfileWithPath(
        browser->profile()->GetPath());
    info_cache.SetProfileSigninRequiredAtIndex(index, false);
  }

  if (!url_hash_.empty()) {
    base::MessageLoop::current()->PostTask(
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
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_WINDOW_READY:
      // Only respond to one Browser Window Ready event.
      registrar_.Remove(this,
                        chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                        content::NotificationService::AllSources());
      OnBrowserWindowReady(content::Source<Browser>(source).ptr());
    break;
    default:
      NOTREACHED();
  }
}

// This callback is run after switching to a new profile has finished. This
// means either a new browser has been created (but not the window), or an
// existing one has been found. The HideUserManager task needs to be posted
// since closing the User Manager before the window is created can flakily
// cause Chrome to close.
void UserManagerScreenHandler::OnSwitchToProfileComplete(
    Profile* profile, Profile::CreateStatus profile_create_status) {
  Browser* browser = chrome::FindAnyBrowser(profile, false, desktop_type_);
  if (browser && browser->window()) {
    OnBrowserWindowReady(browser);
  } else {
    registrar_.Add(this,
                   chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                   content::NotificationService::AllSources());
  }
}
