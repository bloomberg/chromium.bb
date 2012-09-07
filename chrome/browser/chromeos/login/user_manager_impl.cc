// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager_impl.h"

#include <vector>

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/remove_user_delegate.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

using content::BrowserThread;

typedef GoogleServiceAuthError AuthError;

namespace chromeos {

namespace {

// Names of nodes with info about user image.
const char kImagePathNodeName[] = "path";
const char kImageIndexNodeName[] = "index";
const char kImageURLNodeName[] = "url";

// Delay betweeen user login and attempt to update user's profile data.
const long kProfileDataDownloadDelayMs = 10000;

// Delay betweeen subsequent profile refresh attempts (24 hrs).
const long kProfileRefreshIntervalMs = 24L * 3600 * 1000;

// Enum for reporting histograms about profile picture download.
enum ProfileDownloadResult {
  kDownloadSuccessChanged,
  kDownloadSuccess,
  kDownloadFailure,
  kDownloadDefault,
  kDownloadCached,

  // Must be the last, convenient count.
  kDownloadResultsCount
};

// Time histogram prefix for a cached profile image download.
const char kProfileDownloadCachedTime[] =
    "UserImage.ProfileDownloadTime.Cached";
// Time histogram prefix for the default profile image download.
const char kProfileDownloadDefaultTime[] =
    "UserImage.ProfileDownloadTime.Default";
// Time histogram prefix for a failed profile image download.
const char kProfileDownloadFailureTime[] =
    "UserImage.ProfileDownloadTime.Failure";
// Time histogram prefix for a successful profile image download.
const char kProfileDownloadSuccessTime[] =
    "UserImage.ProfileDownloadTime.Success";
// Time histogram suffix for a profile image download after login.
const char kProfileDownloadReasonLoggedIn[] = "LoggedIn";
// Time histogram suffix for a scheduled profile image download.
const char kProfileDownloadReasonScheduled[] = "Scheduled";

// Add a histogram showing the time it takes to download a profile image.
// Separate histograms are reported for each download |reason| and |result|.
void AddProfileImageTimeHistogram(ProfileDownloadResult result,
                                  const std::string& download_reason,
                                  const base::TimeDelta& time_delta) {
  std::string histogram_name;
  switch (result) {
    case kDownloadFailure:
      histogram_name = kProfileDownloadFailureTime;
      break;
    case kDownloadDefault:
      histogram_name = kProfileDownloadDefaultTime;
      break;
    case kDownloadSuccess:
      histogram_name = kProfileDownloadSuccessTime;
      break;
    case kDownloadCached:
      histogram_name = kProfileDownloadCachedTime;
      break;
    default:
      NOTREACHED();
  }
  if (!download_reason.empty()) {
    histogram_name += ".";
    histogram_name += download_reason;
  }

  static const base::TimeDelta min_time = base::TimeDelta::FromMilliseconds(1);
  static const base::TimeDelta max_time = base::TimeDelta::FromSeconds(50);
  const size_t bucket_count(50);

  base::Histogram* counter = base::Histogram::FactoryTimeGet(
      histogram_name, min_time, max_time, bucket_count,
      base::Histogram::kUmaTargetedHistogramFlag);
  counter->AddTime(time_delta);

  DVLOG(1) << "Profile image download time: " << time_delta.InSecondsF();
}

// Callback that is called after user removal is complete.
void OnRemoveUserComplete(const std::string& user_email,
                          bool success,
                          cryptohome::MountError return_code) {
  // Log the error, but there's not much we can do.
  if (!success) {
    LOG(ERROR) << "Removal of cryptohome for " << user_email
               << " failed, return code: " << return_code;
  }
}

// This method is used to implement UserManager::RemoveUser.
void RemoveUserInternal(const std::string& user_email,
                        chromeos::RemoveUserDelegate* delegate) {
  CrosSettings* cros_settings = CrosSettings::Get();

  // Ensure the value of owner email has been fetched.
  if (CrosSettingsProvider::TRUSTED != cros_settings->PrepareTrustedValues(
          base::Bind(&RemoveUserInternal, user_email, delegate))) {
    // Value of owner email is not fetched yet.  RemoveUserInternal will be
    // called again after fetch completion.
    return;
  }
  std::string owner;
  cros_settings->GetString(kDeviceOwner, &owner);
  if (user_email == owner) {
    // Owner is not allowed to be removed from the device.
    return;
  }

  if (delegate)
    delegate->OnBeforeUserRemoved(user_email);

  chromeos::UserManager::Get()->RemoveUserFromList(user_email);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      user_email, base::Bind(&OnRemoveUserComplete, user_email));

  if (delegate)
    delegate->OnUserRemoved(user_email);
}

}  // namespace

UserManagerImpl::UserManagerImpl()
    : image_loader_(new UserImageLoader(ImageDecoder::DEFAULT_CODEC)),
      logged_in_user_(NULL),
      session_started_(false),
      is_current_user_owner_(false),
      is_current_user_new_(false),
      is_current_user_ephemeral_(false),
      ephemeral_users_enabled_(false),
      observed_sync_service_(NULL),
      last_image_set_async_(false),
      downloaded_profile_image_data_url_(chrome::kAboutBlankURL),
      downloading_profile_image_(false) {
  // UserManager instance should be used only on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MigrateWallpaperData();
  registrar_.Add(this, chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
      content::NotificationService::AllSources());
  RetrieveTrustedDevicePolicies();
}

UserManagerImpl::~UserManagerImpl() {
  // Can't use STLDeleteElements because of the private destructor of User.
  for (size_t i = 0; i < users_.size(); ++i)
    delete users_[i];
  users_.clear();
  if (is_current_user_ephemeral_)
    delete logged_in_user_;
}

const UserList& UserManagerImpl::GetUsers() const {
  const_cast<UserManagerImpl*>(this)->EnsureUsersLoaded();
  return users_;
}

void UserManagerImpl::UserLoggedIn(const std::string& email,
                                   bool browser_restart) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!IsUserLoggedIn());

  if (email == kGuestUser) {
    GuestUserLoggedIn();
    return;
  }

  if (email == kDemoUser) {
    DemoUserLoggedIn();
    return;
  }

  if (IsEphemeralUser(email)) {
    EphemeralUserLoggedIn(email);
    return;
  }

  EnsureUsersLoaded();

  // Clear the prefs view of the users.
  PrefService* prefs = g_browser_process->local_state();
  ListPrefUpdate prefs_users_update(prefs, UserManager::kLoggedInUsers);
  prefs_users_update->Clear();

  // Make sure this user is first.
  prefs_users_update->Append(Value::CreateStringValue(email));
  UserList::iterator logged_in_user = users_.end();
  for (UserList::iterator it = users_.begin(); it != users_.end(); ++it) {
    std::string user_email = (*it)->email();
    // Skip the most recent user.
    if (email != user_email)
      prefs_users_update->Append(Value::CreateStringValue(user_email));
    else
      logged_in_user = it;
  }

  if (logged_in_user == users_.end()) {
    is_current_user_new_ = true;
    logged_in_user_ = CreateUser(email, /* is_ephemeral= */ false);
  } else {
    logged_in_user_ = *logged_in_user;
    users_.erase(logged_in_user);
  }
  // This user must be in the front of the user list.
  users_.insert(users_.begin(), logged_in_user_);

  if (is_current_user_new_) {
    SaveUserDisplayName(GetLoggedInUser().email(),
        UTF8ToUTF16(GetLoggedInUser().GetAccountName(true)));
    SetInitialUserImage(email);
    WallpaperManager::Get()->SetInitialUserWallpaper(email, true);
  } else {
    int image_index = logged_in_user_->image_index();
    // If current user image is profile image, it needs to be refreshed.
    bool download_profile_image = image_index == User::kProfileImageIndex;
    if (download_profile_image)
      InitDownloadedProfileImage();

    // Download user's profile data (full name and optionally image) to see if
    // it has changed.
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&UserManagerImpl::DownloadProfileData,
                   base::Unretained(this),
                   kProfileDownloadReasonLoggedIn,
                   download_profile_image),
        base::TimeDelta::FromMilliseconds(kProfileDataDownloadDelayMs));

    int histogram_index = image_index;
    switch (image_index) {
      case User::kExternalImageIndex:
        // TODO(avayvod): Distinguish this from selected from file.
        histogram_index = kHistogramImageFromCamera;
        break;

      case User::kProfileImageIndex:
        histogram_index = kHistogramImageFromProfile;
        break;
    }
    UMA_HISTOGRAM_ENUMERATION("UserImage.LoggedIn",
                              histogram_index,
                              kHistogramImagesCount);
  }

  // Set up a repeating timer for refreshing the profile data.
  profile_download_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kProfileRefreshIntervalMs),
      this, &UserManagerImpl::DownloadProfileDataScheduled);

  if (!browser_restart) {
    // For GAIA login flow, logged in user wallpaper may not be loaded.
    WallpaperManager::Get()->EnsureLoggedInUserWallpaperLoaded();
  }

  NotifyOnLogin();
}

void UserManagerImpl::DemoUserLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_new_ = true;
  is_current_user_ephemeral_ = true;
  logged_in_user_ = CreateUser(kDemoUser, /* is_ephemeral= */ true);
  SetInitialUserImage(kDemoUser);
  WallpaperManager::Get()->SetInitialUserWallpaper(kDemoUser, false);
  NotifyOnLogin();
}

void UserManagerImpl::GuestUserLoggedIn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_ephemeral_ = true;
  WallpaperManager::Get()->SetInitialUserWallpaper(kGuestUser, false);
  logged_in_user_ = CreateUser(kGuestUser, /* is_ephemeral= */ true);
  NotifyOnLogin();
}

void UserManagerImpl::EphemeralUserLoggedIn(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  is_current_user_new_ = true;
  is_current_user_ephemeral_ = true;
  logged_in_user_ = CreateUser(email, /* is_ephemeral= */ true);
  SetInitialUserImage(email);
  WallpaperManager::Get()->SetInitialUserWallpaper(email, false);
  NotifyOnLogin();
}

void UserManagerImpl::SessionStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  session_started_ = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void UserManagerImpl::RemoveUser(const std::string& email,
                                 RemoveUserDelegate* delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!IsKnownUser(email))
    return;

  // Sanity check: we must not remove single user. This check may seem
  // redundant at a first sight because this single user must be an owner and
  // we perform special check later in order not to remove an owner.  However
  // due to non-instant nature of ownership assignment this later check may
  // sometimes fail. See http://crosbug.com/12723
  if (users_.size() < 2)
    return;

  // Sanity check: do not allow the logged-in user to remove himself.
  if (logged_in_user_ && logged_in_user_->email() == email)
    return;

  RemoveUserInternal(email, delegate);
}

void UserManagerImpl::RemoveUserFromList(const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  EnsureUsersLoaded();
  RemoveUserFromListInternal(email);
}

bool UserManagerImpl::IsKnownUser(const std::string& email) const {
  return FindUser(email) != NULL;
}

const User* UserManagerImpl::FindUser(const std::string& email) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (logged_in_user_ && logged_in_user_->email() == email)
    return logged_in_user_;
  return FindUserInList(email);
}

const User& UserManagerImpl::GetLoggedInUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return *logged_in_user_;
}

User& UserManagerImpl::GetLoggedInUser() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return *logged_in_user_;
}

void UserManagerImpl::SaveUserOAuthStatus(
    const std::string& username,
    User::OAuthTokenStatus oauth_token_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "Saving user OAuth token status in Local State";
  User* user = const_cast<User*>(FindUser(username));
  if (user)
    user->set_oauth_token_status(oauth_token_status);

  // Do not update local store if the user is ephemeral.
  if (IsEphemeralUser(username))
    return;

  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate oauth_status_update(local_state,
                                           UserManager::kUserOAuthTokenStatus);
  oauth_status_update->SetWithoutPathExpansion(username,
      new base::FundamentalValue(static_cast<int>(oauth_token_status)));
}

User::OAuthTokenStatus UserManagerImpl::LoadUserOAuthStatus(
    const std::string& username) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSkipOAuthLogin)) {
    // Use OAUTH_TOKEN_STATUS_VALID flag if kSkipOAuthLogin is present.
    return User::OAUTH_TOKEN_STATUS_VALID;
  } else {
    PrefService* local_state = g_browser_process->local_state();
    const DictionaryValue* prefs_oauth_status =
        local_state->GetDictionary(UserManager::kUserOAuthTokenStatus);

    int oauth_token_status = User::OAUTH_TOKEN_STATUS_UNKNOWN;
    if (prefs_oauth_status &&
        prefs_oauth_status->GetIntegerWithoutPathExpansion(username,
            &oauth_token_status)) {
      return static_cast<User::OAuthTokenStatus>(oauth_token_status);
    }
  }

  return User::OAUTH_TOKEN_STATUS_UNKNOWN;
}

void UserManagerImpl::SaveUserDisplayName(const std::string& username,
                                          const string16& display_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  User* user = const_cast<User*>(FindUser(username));
  if (!user)
    return;  // Ignore if there is no such user.

  user->set_display_name(display_name);

  // Do not update local store if the user is ephemeral.
  if (IsEphemeralUser(username))
    return;

  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate display_name_update(local_state,
                                           UserManager::kUserDisplayName);
  display_name_update->SetWithoutPathExpansion(
      username,
      base::Value::CreateStringValue(display_name));
}

string16 UserManagerImpl::GetUserDisplayName(
    const std::string& username) const {
  const User* user = FindUser(username);
  return user ? user->display_name() : string16();
}

void UserManagerImpl::SaveUserDisplayEmail(const std::string& username,
                                           const std::string& display_email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  User* user = const_cast<User*>(FindUser(username));
  if (!user)
    return;  // Ignore if there is no such user.

  user->set_display_email(display_email);

  // Do not update local store if the user is ephemeral.
  if (IsEphemeralUser(username))
    return;

  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate display_email_update(local_state,
                                            UserManager::kUserDisplayEmail);
  display_email_update->SetWithoutPathExpansion(
      username,
      base::Value::CreateStringValue(display_email));
}

std::string UserManagerImpl::GetUserDisplayEmail(
    const std::string& username) const {
  const User* user = FindUser(username);
  return user ? user->display_email() : username;
}

void UserManagerImpl::SaveUserDefaultImageIndex(const std::string& username,
                                                int image_index) {
  DCHECK(image_index >= 0 && image_index < kDefaultImagesCount);
  SetUserImage(username, image_index, GURL(),
               UserImage(GetDefaultImage(image_index)));
  SaveImageToLocalState(username, "", image_index, GURL(), false);
}

void UserManagerImpl::SaveUserImage(const std::string& username,
                                    const UserImage& user_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SaveUserImageInternal(username, User::kExternalImageIndex,
                        GURL(), user_image);
}

void UserManagerImpl::SetLoggedInUserCustomWallpaperLayout(
    ash::WallpaperLayout layout) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(bshe): We current disabled the customized wallpaper feature for
  // Ephemeral user. As we dont want to keep a copy of customized wallpaper in
  // memory. Need a smarter way to solve this.
  if (IsCurrentUserEphemeral())
    return;
  const chromeos::User& user = GetLoggedInUser();
  std::string username = user.email();
  DCHECK(!username.empty());

  std::string file_path = WallpaperManager::Get()->
      GetWallpaperPathForUser(username, false).value();
  SaveWallpaperToLocalState(username, file_path, layout, User::CUSTOMIZED);
  // Load wallpaper from file.
  WallpaperManager::Get()->SetUserWallpaper(username);
}

void UserManagerImpl::SaveUserImageFromFile(const std::string& username,
                                            const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  image_loader_->Start(
      path.value(), login::kMaxUserImageSize,
      base::Bind(&UserManagerImpl::SaveUserImage,
                 base::Unretained(this), username));
}

void UserManagerImpl::SaveUserImageFromProfileImage(
    const std::string& username) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!downloaded_profile_image_.isNull()) {
    // Profile image has already been downloaded, so save it to file right now.
    DCHECK(profile_image_url_.is_valid());
    SaveUserImageInternal(
        username,
        User::kProfileImageIndex, profile_image_url_,
        UserImage::CreateAndEncode(downloaded_profile_image_));
  } else {
    // No profile image - use the stub image (gray avatar).
    SetUserImage(username, User::kProfileImageIndex, GURL(), UserImage());
    SaveImageToLocalState(username, "", User::kProfileImageIndex,
                          GURL(), false);
  }
}

void UserManagerImpl::DownloadProfileImage(const std::string& reason) {
  DownloadProfileData(reason, true);
}

void UserManagerImpl::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED:
      CheckOwnership();
      RetrieveTrustedDevicePolicies();
      break;
    case chrome::NOTIFICATION_PROFILE_ADDED:
      if (IsUserLoggedIn() && !IsLoggedInAsGuest()) {
        Profile* profile = content::Source<Profile>(source).ptr();
        if (!profile->IsOffTheRecord() &&
            profile == ProfileManager::GetDefaultProfile()) {
          DCHECK(NULL == observed_sync_service_);
          observed_sync_service_ =
              ProfileSyncServiceFactory::GetForProfile(profile);
          if (observed_sync_service_)
            observed_sync_service_->AddObserver(this);
        }
      }
      break;
    default:
      NOTREACHED();
  }
}

void UserManagerImpl::OnStateChanged() {
  DCHECK(IsUserLoggedIn() && !IsLoggedInAsGuest());
  AuthError::State state = observed_sync_service_->GetAuthError().state();
  if (state != AuthError::NONE &&
      state != AuthError::CONNECTION_FAILED &&
      state != AuthError::SERVICE_UNAVAILABLE &&
      state != AuthError::REQUEST_CANCELED) {
    // Invalidate OAuth token to force Gaia sign-in flow. This is needed
    // because sign-out/sign-in solution is suggested to the user.
    // TODO(altimofeev): this code isn't needed after crosbug.com/25978 is
    // implemented.
    DVLOG(1) << "Invalidate OAuth token because of a sync error.";
    SaveUserOAuthStatus(GetLoggedInUser().email(),
                        User::OAUTH_TOKEN_STATUS_INVALID);
  }
}

bool UserManagerImpl::IsCurrentUserOwner() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lk(is_current_user_owner_lock_);
  return is_current_user_owner_;
}

void UserManagerImpl::SetCurrentUserIsOwner(bool is_current_user_owner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lk(is_current_user_owner_lock_);
  is_current_user_owner_ = is_current_user_owner;
}

bool UserManagerImpl::IsCurrentUserNew() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return is_current_user_new_;
}

bool UserManagerImpl::IsCurrentUserEphemeral() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return is_current_user_ephemeral_;
}

bool UserManagerImpl::IsUserLoggedIn() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return logged_in_user_;
}

bool UserManagerImpl::IsLoggedInAsDemoUser() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() && logged_in_user_->email() == kDemoUser;
}

bool UserManagerImpl::IsLoggedInAsGuest() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() && logged_in_user_->email() == kGuestUser;
}

bool UserManagerImpl::IsLoggedInAsStub() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return IsUserLoggedIn() && logged_in_user_->email() == kStubUser;
}

bool UserManagerImpl::IsSessionStarted() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return session_started_;
}

bool UserManagerImpl::IsEphemeralUser(const std::string& email) const {
  // The guest and stub user always are ephemeral.
  if (email == kGuestUser || email == kStubUser)
    return true;

  // The currently logged-in user is ephemeral iff logged in as ephemeral.
  if (logged_in_user_ && (email == logged_in_user_->email()))
    return is_current_user_ephemeral_;

  // The owner and any users found in the persistent list are never ephemeral.
  if (email == owner_email_  || FindUserInList(email))
    return false;

  // Any other user is ephemeral when:
  // a) Going through the regular login flow and ephemeral users are enabled.
  //    - or -
  // b) The browser is restarting after a crash.
  return AreEphemeralUsersEnabled() ||
         (base::chromeos::IsRunningOnChromeOS() &&
          !CommandLine::ForCurrentProcess()->
              HasSwitch(switches::kLoginManager));
}

void UserManagerImpl::AddObserver(UserManager::Observer* obs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observer_list_.AddObserver(obs);
}

void UserManagerImpl::RemoveObserver(UserManager::Observer* obs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observer_list_.RemoveObserver(obs);
}

const gfx::ImageSkia& UserManagerImpl::DownloadedProfileImage() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return downloaded_profile_image_;
}

void UserManagerImpl::NotifyLocalStateChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(UserManager::Observer, observer_list_,
                    LocalStateChanged(this));
}

FilePath UserManagerImpl::GetImagePathForUser(const std::string& username) {
  std::string filename = username + ".png";
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.AppendASCII(filename);
}

void UserManagerImpl::EnsureUsersLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!users_.empty())
    return;
  if (!g_browser_process)
    return;

  PrefService* local_state = g_browser_process->local_state();
  const ListValue* prefs_users =
      local_state->GetList(UserManager::kLoggedInUsers);
  const DictionaryValue* prefs_images =
      local_state->GetDictionary(UserManager::kUserImages);
  const DictionaryValue* prefs_display_names =
      local_state->GetDictionary(UserManager::kUserDisplayName);
  const DictionaryValue* prefs_display_emails =
      local_state->GetDictionary(UserManager::kUserDisplayEmail);

  if (prefs_users) {
    for (ListValue::const_iterator it = prefs_users->begin();
         it != prefs_users->end(); ++it) {
      std::string email;
      if ((*it)->GetAsString(&email)) {
        User* user = CreateUser(email, /* is_ephemeral= */ false);
        users_.push_back(user);

        if (prefs_images) {
          // Get account image path.
          // TODO(avayvod): Reading image path as a string is here for
          // backward compatibility.
          std::string image_path;
          const base::DictionaryValue* image_properties;
          if (prefs_images->GetStringWithoutPathExpansion(email, &image_path)) {
            int image_id = User::kInvalidImageIndex;
            if (IsDefaultImagePath(image_path, &image_id)) {
              user->SetImage(UserImage(GetDefaultImage(image_id)), image_id);
            } else {
              int image_index = User::kExternalImageIndex;
              // Until image has been loaded, use the stub image.
              user->SetStubImage(image_index);
              DCHECK(!image_path.empty());
              // Load user image asynchronously.
              image_loader_->Start(
                  image_path, 0  /* no resize */,
                  base::Bind(&UserManagerImpl::SetUserImage,
                             base::Unretained(this),
                             email, image_index, GURL()));
            }
          } else if (prefs_images->GetDictionaryWithoutPathExpansion(
                         email, &image_properties)) {
            int image_index = User::kInvalidImageIndex;
            image_properties->GetString(kImagePathNodeName, &image_path);
            image_properties->GetInteger(kImageIndexNodeName, &image_index);
            if (image_index >= 0 && image_index < kDefaultImagesCount) {
              user->SetImage(UserImage(GetDefaultImage(image_index)),
                             image_index);
            } else if (image_index == User::kExternalImageIndex ||
                       image_index == User::kProfileImageIndex) {
              // Path may be empty for profile images (meaning that the image
              // hasn't been downloaded for the first time yet, in which case a
              // download will be scheduled for |kProfileDataDownloadDelayMs|
              // after user logs in).
              DCHECK(!image_path.empty() ||
                     image_index == User::kProfileImageIndex);
              std::string image_url;
              image_properties->GetString(kImageURLNodeName, &image_url);
              GURL image_gurl(image_url);
              // Until image has been loaded, use the stub image (gray avatar).
              user->SetStubImage(image_index);
              user->SetImageURL(image_gurl);
              if (!image_path.empty()) {
                // Load user image asynchronously.
                image_loader_->Start(
                    image_path, 0  /* no resize */,
                    base::Bind(&UserManagerImpl::SetUserImage,
                               base::Unretained(this),
                               email, image_index, image_gurl));
              }
            } else {
              NOTREACHED();
            }
          }
        }

        string16 display_name;
        if (prefs_display_names &&
            prefs_display_names->GetStringWithoutPathExpansion(
                email, &display_name)) {
          user->set_display_name(display_name);
        }

        std::string display_email;
        if (prefs_display_emails &&
            prefs_display_emails->GetStringWithoutPathExpansion(
                email, &display_email)) {
          user->set_display_email(display_email);
        }
      }
    }
  }
}

void UserManagerImpl::RetrieveTrustedDevicePolicies() {
  ephemeral_users_enabled_ = false;
  owner_email_ = "";

  CrosSettings* cros_settings = CrosSettings::Get();
  // Schedule a callback if device policy has not yet been verified.
  if (CrosSettingsProvider::TRUSTED != cros_settings->PrepareTrustedValues(
      base::Bind(&UserManagerImpl::RetrieveTrustedDevicePolicies,
                 base::Unretained(this)))) {
    return;
  }

  cros_settings->GetBoolean(kAccountsPrefEphemeralUsersEnabled,
                            &ephemeral_users_enabled_);
  cros_settings->GetString(kDeviceOwner, &owner_email_);

  // If ephemeral users are enabled and we are on the login screen, take this
  // opportunity to clean up by removing all users except the owner.
  if (ephemeral_users_enabled_  && !IsUserLoggedIn()) {
    scoped_ptr<base::ListValue> users(
        g_browser_process->local_state()->GetList(kLoggedInUsers)->DeepCopy());

    bool changed = false;
    for (base::ListValue::const_iterator user = users->begin();
        user != users->end(); ++user) {
      std::string user_email;
      (*user)->GetAsString(&user_email);
      if (user_email != owner_email_) {
        RemoveUserFromListInternal(user_email);
        changed = true;
      }
    }

    if (changed) {
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_POLICY_USER_LIST_CHANGED,
          content::Source<UserManagerImpl>(this),
          content::NotificationService::NoDetails());
    }
  }
}

bool UserManagerImpl::AreEphemeralUsersEnabled() const {
  return ephemeral_users_enabled_ &&
      (g_browser_process->browser_policy_connector()->IsEnterpriseManaged() ||
      !owner_email_.empty());
}

const User* UserManagerImpl::FindUserInList(const std::string& email) const {
  const UserList& users = GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->email() == email)
      return *it;
  }
  return NULL;
}

void UserManagerImpl::NotifyOnLogin() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::Source<UserManagerImpl>(this),
      content::Details<const User>(logged_in_user_));

  CrosLibrary::Get()->GetCertLibrary()->LoadKeyStore();

  // Indicate to DeviceSettingsService that the owner key may have become
  // available.
  DeviceSettingsService::Get()->SetUsername(logged_in_user_->email());
}

void UserManagerImpl::SetInitialUserImage(const std::string& username) {
  // Choose a random default image.
  int image_id =
      base::RandInt(kFirstDefaultImageIndex, kDefaultImagesCount - 1);
  SaveUserDefaultImageIndex(username, image_id);
}

void UserManagerImpl::MigrateWallpaperData() {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    const DictionaryValue* user_wallpapers =
          local_state->GetDictionary(kUserWallpapers);
    int index;
    const DictionaryValue* new_user_wallpapers =
        local_state->GetDictionary(kUserWallpapersProperties);
    if (new_user_wallpapers->empty()) {
      const UserList& users = GetUsers();
      for (UserList::const_iterator it = users.begin();
           it != users.end();
           ++it) {
        std::string username = (*it)->email();
        if (user_wallpapers->GetIntegerWithoutPathExpansion((username),
            &index)) {
          DictionaryPrefUpdate prefs_wallpapers_update(local_state,
                                                       kUserWallpapers);
          prefs_wallpapers_update->RemoveWithoutPathExpansion(username, NULL);
          WallpaperManager::Get()->SetUserWallpaperProperties(username,
                                                              User::DEFAULT,
                                                              index,
                                                              true);
        } else {
          // Before M20, wallpaper index is not saved into LocalState unless
          // user specifically sets a wallpaper. After M20, the default
          // wallpaper index is saved to LocalState as soon as a new user login.
          // When migrating wallpaper index from M20 to M21, we only migrate
          // data that is in LocalState. This cause a problem when users login
          // on a M20 device and then update the device to M21. The default
          // wallpaper index failed to migrate because it was not saved into
          // LocalState. Then we assume that all users have index saved in
          // LocalState in M21. This is not true and it results an empty
          // wallpaper for those users as described in cr/130685. So here we use
          // default wallpaper for users that exist in user list but does not
          // have an index saved in LocalState.
          WallpaperManager::Get()->SetUserWallpaperProperties(username,
              User::DEFAULT, ash::GetDefaultWallpaperIndex(), true);
        }
      }
    }
  }
}

void UserManagerImpl::SaveLoggedInUserWallpaperProperties(
    User::WallpaperType type, int index) {
  // Ephemeral users can not save data to local state.
  // We just cache the index in memory for them.
  bool is_persistent = !IsCurrentUserEphemeral();
  WallpaperManager::Get()->SetUserWallpaperProperties(
      GetLoggedInUser().email(), type, index, is_persistent);
}

void UserManagerImpl::SetUserImage(const std::string& username,
                                   int image_index,
                                   const GURL& image_url,
                                   const UserImage& user_image) {
  User* user = const_cast<User*>(FindUser(username));
  // User may have been removed by now.
  if (user) {
    // For existing users, a valid image index should have been set upon loading
    // them from Local State.
    DCHECK(user->image_index() != User::kInvalidImageIndex ||
           is_current_user_new_);
    bool image_changed = user->image_index() != User::kInvalidImageIndex;
    if (!user_image.image().isNull())
      user->SetImage(user_image, image_index);
    else
      user->SetStubImage(image_index);
    user->SetImageURL(image_url);
    // For the logged-in user with a profile picture, initialize
    // |downloaded_profile_picture_|.
    if (user == logged_in_user_ && image_index == User::kProfileImageIndex)
      InitDownloadedProfileImage();
    if (image_changed) {
      // Unless this is first-time setting with |SetInitialUserImage|,
      // send a notification about image change.
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
          content::Source<UserManagerImpl>(this),
          content::Details<const User>(user));
    }
  }
}

void UserManagerImpl::SaveUserImageInternal(const std::string& username,
                                            int image_index,
                                            const GURL& image_url,
                                            const UserImage& user_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetUserImage(username, image_index, image_url, user_image);

  // Ignore for ephemeral users.
  if (IsEphemeralUser(username))
    return;

  FilePath image_path = GetImagePathForUser(username);
  DVLOG(1) << "Saving user image to " << image_path.value();

  last_image_set_async_ = true;

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UserManagerImpl::SaveImageToFile,
                 base::Unretained(this),
                 username, user_image, image_path, image_index, image_url));
}

void UserManagerImpl::SaveImageToFile(const std::string& username,
                                      const UserImage& user_image,
                                      const FilePath& image_path,
                                      int image_index,
                                      const GURL& image_url) {
  if (!SaveBitmapToFile(user_image, image_path))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UserManagerImpl::SaveImageToLocalState,
                 base::Unretained(this),
                 username, image_path.value(), image_index, image_url, true));
}

void UserManagerImpl::SaveImageToLocalState(const std::string& username,
                                            const std::string& image_path,
                                            int image_index,
                                            const GURL& image_url,
                                            bool is_async) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ignore for ephemeral users.
  if (IsEphemeralUser(username))
    return;

  // TODO(ivankr): use unique filenames for user images each time
  // a new image is set so that only the last image update is saved
  // to Local State and notified.
  if (is_async && !last_image_set_async_) {
    DVLOG(1) << "Ignoring saved image because it has changed";
    return;
  } else if (!is_async) {
    // Reset the async image save flag if called directly from the UI thread.
    last_image_set_async_ = false;
  }

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate images_update(local_state, UserManager::kUserImages);
  base::DictionaryValue* image_properties = new base::DictionaryValue();
  image_properties->Set(kImagePathNodeName, new StringValue(image_path));
  image_properties->Set(kImageIndexNodeName,
                        new base::FundamentalValue(image_index));
  if (!image_url.is_empty()) {
    image_properties->Set(kImageURLNodeName,
                          new StringValue(image_url.spec()));
  } else {
    image_properties->Remove(kImageURLNodeName, NULL);
  }
  images_update->SetWithoutPathExpansion(username, image_properties);
  DVLOG(1) << "Saving path to user image in Local State.";

  NotifyLocalStateChanged();
}

void UserManagerImpl::SaveWallpaperToLocalState(const std::string& username,
    const std::string& wallpaper_path,
    ash::WallpaperLayout layout,
    User::WallpaperType type) {
  // TODO(bshe): We probably need to save wallpaper_path instead of index.
  WallpaperManager::Get()->SetUserWallpaperProperties(
      username, type, layout, true);
}

bool UserManagerImpl::SaveBitmapToFile(const UserImage& user_image,
                                       const FilePath& image_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  const UserImage::RawImage* encoded_image = NULL;
  if (user_image.has_animated_image()) {
    encoded_image = &user_image.animated_image();
  } else if (user_image.has_raw_image()) {
    encoded_image = &user_image.raw_image();
  } else {
    LOG(ERROR) << "Failed to encode the image.";
    return false;
  }
  DCHECK(encoded_image != NULL);

  if (file_util::WriteFile(image_path,
                           reinterpret_cast<const char*>(&(*encoded_image)[0]),
                           encoded_image->size()) == -1) {
    LOG(ERROR) << "Failed to save image to file.";
    return false;
  }
  return true;
}

void UserManagerImpl::InitDownloadedProfileImage() {
  DCHECK(logged_in_user_);
  DCHECK_EQ(logged_in_user_->image_index(), User::kProfileImageIndex);
  if (downloaded_profile_image_.isNull() && !logged_in_user_->image_is_stub()) {
    VLOG(1) << "Profile image initialized";
    downloaded_profile_image_ = logged_in_user_->image();
    downloaded_profile_image_data_url_ =
        web_ui_util::GetImageDataUrl(downloaded_profile_image_);
    profile_image_url_ = logged_in_user_->image_url();
  }
}

void UserManagerImpl::DownloadProfileData(const std::string& reason,
                                          bool download_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // For guest login there's no profile image to download.
  if (IsLoggedInAsGuest())
    return;

  // Mark profile picture as needed.
  downloading_profile_image_ |= download_image;

  // Another download is already in progress
  if (profile_image_downloader_.get())
    return;

  profile_image_download_reason_ = reason;
  profile_image_load_start_time_ = base::Time::Now();
  profile_image_downloader_.reset(new ProfileDownloader(this));
  profile_image_downloader_->Start();
}

void UserManagerImpl::DownloadProfileDataScheduled() {
  // If current user image is profile image, it needs to be refreshed.
  bool download_profile_image =
      logged_in_user_->image_index() == User::kProfileImageIndex;
  DownloadProfileData(kProfileDownloadReasonScheduled, download_profile_image);
}

void UserManagerImpl::DeleteUserImage(const FilePath& image_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::Delete(image_path, false)) {
    LOG(ERROR) << "Failed to remove user image.";
    return;
  }
}

void UserManagerImpl::UpdateOwnership(
    DeviceSettingsService::OwnershipStatus status,
    bool is_owner) {
  VLOG(1) << "Current user " << (is_owner ? "is owner" : "is not owner");

  SetCurrentUserIsOwner(is_owner);
}

void UserManagerImpl::CheckOwnership() {
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&UserManagerImpl::UpdateOwnership,
                 base::Unretained(this)));
}

// ProfileDownloaderDelegate override.
bool UserManagerImpl::NeedsProfilePicture() const {
  return downloading_profile_image_;
}

// ProfileDownloaderDelegate override.
int UserManagerImpl::GetDesiredImageSideLength() const {
  return GetCurrentUserImageSize();
}

// ProfileDownloaderDelegate override.
std::string UserManagerImpl::GetCachedPictureURL() const {
  return profile_image_url_.spec();
}

Profile* UserManagerImpl::GetBrowserProfile() {
  return ProfileManager::GetDefaultProfile();
}

void UserManagerImpl::OnProfileDownloadSuccess(ProfileDownloader* downloader) {
  // Make sure that |ProfileDownloader| gets deleted after return.
  scoped_ptr<ProfileDownloader> profile_image_downloader(
      profile_image_downloader_.release());
  DCHECK_EQ(downloader, profile_image_downloader.get());

  if (!downloader->GetProfileFullName().empty()) {
    SaveUserDisplayName(GetLoggedInUser().email(),
                        downloader->GetProfileFullName());
  }

  bool requested_image = downloading_profile_image_;
  downloading_profile_image_ = false;
  if (!requested_image)
    return;

  ProfileDownloadResult result = kDownloadFailure;
  switch (downloader->GetProfilePictureStatus()) {
    case ProfileDownloader::PICTURE_SUCCESS:
      result = kDownloadSuccess;
      break;
    case ProfileDownloader::PICTURE_CACHED:
      result = kDownloadCached;
      break;
    case ProfileDownloader::PICTURE_DEFAULT:
      result = kDownloadDefault;
      break;
    default:
      NOTREACHED();
  }

  UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
      result, kDownloadResultsCount);

  DCHECK(!profile_image_load_start_time_.is_null());
  base::TimeDelta delta = base::Time::Now() - profile_image_load_start_time_;
  AddProfileImageTimeHistogram(result, profile_image_download_reason_, delta);

  if (result == kDownloadDefault) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
        content::Source<UserManagerImpl>(this),
        content::NotificationService::NoDetails());
  }

  // Nothing to do if picture is cached or the default avatar.
  if (result != kDownloadSuccess)
    return;

  // Check if this image is not the same as already downloaded.
  gfx::ImageSkia new_image(downloader->GetProfilePicture());
  std::string new_image_data_url = web_ui_util::GetImageDataUrl(new_image);
  if (!downloaded_profile_image_data_url_.empty() &&
      new_image_data_url == downloaded_profile_image_data_url_)
    return;

  downloaded_profile_image_data_url_ = new_image_data_url;
  downloaded_profile_image_ = gfx::ImageSkia(downloader->GetProfilePicture());
  profile_image_url_ = GURL(downloader->GetProfilePictureURL());

  if (GetLoggedInUser().image_index() == User::kProfileImageIndex) {
    VLOG(1) << "Updating profile image for logged-in user";
    UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
                              kDownloadSuccessChanged,
                              kDownloadResultsCount);
    // This will persist |downloaded_profile_image_| to file.
    SaveUserImageFromProfileImage(GetLoggedInUser().email());
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
      content::Source<UserManagerImpl>(this),
      content::Details<const gfx::ImageSkia>(&downloaded_profile_image_));
}

void UserManagerImpl::OnProfileDownloadFailure(ProfileDownloader* downloader) {
  DCHECK_EQ(downloader, profile_image_downloader_.get());
  profile_image_downloader_.reset();

  downloading_profile_image_ = false;

  UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
      kDownloadFailure, kDownloadResultsCount);

  DCHECK(!profile_image_load_start_time_.is_null());
  base::TimeDelta delta = base::Time::Now() - profile_image_load_start_time_;
  AddProfileImageTimeHistogram(kDownloadFailure, profile_image_download_reason_,
                               delta);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
      content::Source<UserManagerImpl>(this),
      content::NotificationService::NoDetails());
}

User* UserManagerImpl::CreateUser(const std::string& email,
                                  bool is_ephemeral) const {
  User* user = new User(email);
  if (!is_ephemeral)
    user->set_oauth_token_status(LoadUserOAuthStatus(email));
  return user;
}

void UserManagerImpl::RemoveUserFromListInternal(const std::string& email) {
  // Clear the prefs view of the users.
  PrefService* prefs = g_browser_process->local_state();
  ListPrefUpdate prefs_users_update(prefs, kLoggedInUsers);
  prefs_users_update->Clear();

  UserList::iterator user_to_remove = users_.end();
  for (UserList::iterator it = users_.begin(); it != users_.end(); ++it) {
    std::string user_email = (*it)->email();
    // Skip user that we would like to delete.
    if (email != user_email)
      prefs_users_update->Append(Value::CreateStringValue(user_email));
    else
      user_to_remove = it;
  }

  DictionaryPrefUpdate prefs_wallpapers_update(prefs,
                                               kUserWallpapersProperties);
  prefs_wallpapers_update->RemoveWithoutPathExpansion(email, NULL);

  bool new_wallpaper_ui_disabled = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kDisableNewWallpaperUI);
  if (!new_wallpaper_ui_disabled) {
    DictionaryPrefUpdate prefs_wallpapers_info_update(prefs,
        prefs::kUsersWallpaperInfo);
    prefs_wallpapers_info_update->RemoveWithoutPathExpansion(email, NULL);
  }

  // Remove user wallpaper thumbnail
  FilePath wallpaper_thumb_path = WallpaperManager::Get()->
      GetWallpaperPathForUser(email, true);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UserManagerImpl::DeleteUserImage,
                 base::Unretained(this),
                 wallpaper_thumb_path));
  // Remove user wallpaper
  FilePath wallpaper_path = WallpaperManager::Get()->
      GetWallpaperPathForUser(email, false);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UserManagerImpl::DeleteUserImage,
                 base::Unretained(this),
                 wallpaper_path));
  FilePath wallpaper_original_path = WallpaperManager::Get()->
      GetOriginalWallpaperPathForUser(email);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UserManagerImpl::DeleteUserImage,
                 base::Unretained(this),
                 wallpaper_original_path));

  DictionaryPrefUpdate prefs_images_update(prefs, kUserImages);
  std::string image_path_string;
  prefs_images_update->GetStringWithoutPathExpansion(email, &image_path_string);
  prefs_images_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_oauth_update(prefs, kUserOAuthTokenStatus);
  int oauth_status;
  prefs_oauth_update->GetIntegerWithoutPathExpansion(email, &oauth_status);
  prefs_oauth_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_display_name_update(prefs, kUserDisplayName);
  prefs_display_name_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_display_email_update(prefs, kUserDisplayEmail);
  prefs_display_email_update->RemoveWithoutPathExpansion(email, NULL);

  if (user_to_remove != users_.end()) {
    delete *user_to_remove;
    users_.erase(user_to_remove);
  }

  int default_image_id = User::kInvalidImageIndex;
  if (!image_path_string.empty() &&
      !IsDefaultImagePath(image_path_string, &default_image_id)) {
    FilePath image_path(image_path_string);
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&UserManagerImpl::DeleteUserImage,
                   base::Unretained(this),
                   image_path));
  }
}

}  // namespace chromeos
