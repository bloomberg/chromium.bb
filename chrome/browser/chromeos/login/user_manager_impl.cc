// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager_impl.h"

#include <vector>

#include "ash/shell.h"
#include "ash/desktop_background/desktop_background_controller.h"
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
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cryptohome/async_method_caller.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/remove_user_delegate.h"
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
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_constants.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

using content::BrowserThread;

typedef GoogleServiceAuthError AuthError;

namespace chromeos {

namespace {

// Incognito user is represented by an empty string (since some code already
// depends on that and it's hard to figure out what).
const char kGuestUser[] = "";

// Stub user email (for test paths).
const char kStubUser[] = "stub-user@example.com";

// Names of nodes with info about user image.
const char kImagePathNodeName[] = "path";
const char kImageIndexNodeName[] = "index";

const char kWallpaperTypeNodeName[] = "type";
const char kWallpaperIndexNodeName[] = "index";

const int kThumbnailWidth = 128;
const int kThumbnailHeight = 80;

// Index of the default image used for the |kStubUser| user.
const int kStubDefaultImageIndex = 0;

// Delay betweeen user login and attempt to update user's profile image.
const long kProfileImageDownloadDelayMs = 10000;

// Enum for reporting histograms about profile picture download.
enum ProfileDownloadResult {
  kDownloadSuccessChanged,
  kDownloadSuccess,
  kDownloadFailure,
  kDownloadDefault,

  // Must be the last, convenient count.
  kDownloadResultsCount
};

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
  if (!cros_settings->PrepareTrustedValues(
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
    : ALLOW_THIS_IN_INITIALIZER_LIST(image_loader_(new UserImageLoader)),
      logged_in_user_(NULL),
      session_started_(false),
      is_current_user_owner_(false),
      is_current_user_new_(false),
      is_current_user_ephemeral_(false),
      current_user_wallpaper_type_(User::DEFAULT),
      ALLOW_THIS_IN_INITIALIZER_LIST(current_user_wallpaper_index_(
          ash::GetDefaultWallpaperIndex())),
      ephemeral_users_enabled_(false),
      observed_sync_service_(NULL),
      last_image_set_async_(false),
      downloaded_profile_image_data_url_(chrome::kAboutBlankURL) {
  // If we're not running on ChromeOS, and are not showing the login manager
  // or attempting a command line login? Then login the stub user.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!base::chromeos::IsRunningOnChromeOS() &&
      !command_line->HasSwitch(switches::kLoginManager) &&
      !command_line->HasSwitch(switches::kLoginPassword) &&
      !command_line->HasSwitch(switches::kGuestSession)) {
    StubUserLoggedIn();
  }

  MigrateWallpaperData();

  registrar_.Add(this, chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
      content::NotificationService::AllSources());
  RetrieveTrustedDevicePolicies();
}

UserManagerImpl::~UserManagerImpl() {
  // Can't use STLDeleteElements because of the private destructor of User.
  for (size_t i = 0; i < users_.size();++i)
    delete users_[i];
  users_.clear();
  if (is_current_user_ephemeral_)
    delete logged_in_user_;
}

const UserList& UserManagerImpl::GetUsers() const {
  const_cast<UserManagerImpl*>(this)->EnsureUsersLoaded();
  return users_;
}

void UserManagerImpl::UserLoggedIn(const std::string& email) {
  // Remove the stub user if it is still around.
  if (logged_in_user_) {
    DCHECK(IsLoggedInAsStub());
    delete logged_in_user_;
    logged_in_user_ = NULL;
    is_current_user_ephemeral_ = false;
  }

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
    logged_in_user_ = CreateUser(email);
  } else {
    logged_in_user_ = *logged_in_user;
    users_.erase(logged_in_user);
  }
  // This user must be in the front of the user list.
  users_.insert(users_.begin(), logged_in_user_);

  if (is_current_user_new_) {
    SetInitialUserImage(email);
    SetInitialUserWallpaper(email);
  } else {
    // Download profile image if it's user image and see if it has changed.
    int image_index = logged_in_user_->image_index();
    if (image_index == User::kProfileImageIndex) {
      InitDownloadedProfileImage();
      BrowserThread::PostDelayedTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&UserManagerImpl::DownloadProfileImage,
                     base::Unretained(this),
                     kProfileDownloadReasonLoggedIn),
          base::TimeDelta::FromMilliseconds(kProfileImageDownloadDelayMs));
    }

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

  NotifyOnLogin();
}

void UserManagerImpl::DemoUserLoggedIn() {
  is_current_user_new_ = true;
  is_current_user_ephemeral_ = true;
  logged_in_user_ = new User(kDemoUser, false);
  SetInitialUserImage(kDemoUser);
  SetInitialUserWallpaper(kDemoUser);
  NotifyOnLogin();
}

void UserManagerImpl::GuestUserLoggedIn() {
  is_current_user_ephemeral_ = true;
  SetInitialUserWallpaper(kGuestUser);
  logged_in_user_ = new User(kGuestUser, true);
  NotifyOnLogin();
}

void UserManagerImpl::EphemeralUserLoggedIn(const std::string& email) {
  is_current_user_new_ = true;
  is_current_user_ephemeral_ = true;
  logged_in_user_ = CreateUser(email);
  SetInitialUserImage(email);
  SetInitialUserWallpaper(email);
  NotifyOnLogin();
}

void UserManagerImpl::StubUserLoggedIn() {
  is_current_user_ephemeral_ = true;
  logged_in_user_ = new User(kStubUser, false);
  logged_in_user_->SetImage(GetDefaultImage(kStubDefaultImageIndex),
                            kStubDefaultImageIndex);
}

void UserManagerImpl::UserSelected(const std::string& email) {
  if (IsKnownUser(email)) {
    User::WallpaperType type;
    int index;
    GetUserWallpaperProperties(email, &type, &index);
    if (type == User::RANDOM) {
      // Generate a new random wallpaper index if the selected user chose
      // RANDOM wallpaper.
      index = ash::GetRandomWallpaperIndex();
      SaveUserWallpaperProperties(email, User::RANDOM, index);
    } else if (type == User::CUSTOMIZED) {
      std::string wallpaper_path =
          GetWallpaperPathForUser(email, false).value();
      // In customized mode, we use index pref to save the user selected
      // wallpaper layout. See function SaveWallpaperToLocalState().
      ash::WallpaperLayout layout = static_cast<ash::WallpaperLayout>(index);
      // Load user image asynchronously.
      image_loader_->Start(
          wallpaper_path, 0,
          base::Bind(&UserManagerImpl::LoadCustomWallpaperThumbnail,
              base::Unretained(this), email, layout));
      return;
    }
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetDefaultWallpaper(index);
  }
}

void UserManagerImpl::SessionStarted() {
  session_started_ = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void UserManagerImpl::RemoveUser(const std::string& email,
                                 RemoveUserDelegate* delegate) {
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
  EnsureUsersLoaded();
  RemoveUserFromListInternal(email);
}

bool UserManagerImpl::IsKnownUser(const std::string& email) const {
  return FindUser(email) != NULL;
}

const User* UserManagerImpl::FindUser(const std::string& email) const {
  if (logged_in_user_ && logged_in_user_->email() == email)
    return logged_in_user_;
  return FindUserInList(email);
}

const User& UserManagerImpl::GetLoggedInUser() const {
  return *logged_in_user_;
}

User& UserManagerImpl::GetLoggedInUser() {
  return *logged_in_user_;
}

bool UserManagerImpl::IsDisplayNameUnique(
    const std::string& display_name) const {
  return display_name_count_[display_name] < 2;
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
  SetUserImage(username, image_index, GetDefaultImage(image_index));
  SaveImageToLocalState(username, "", image_index, false);
}

void UserManagerImpl::SaveUserImage(const std::string& username,
                                    const SkBitmap& image) {
  SaveUserImageInternal(username, User::kExternalImageIndex, image);
}

void UserManagerImpl::SetLoggedInUserCustomWallpaperLayout(
    ash::WallpaperLayout layout) {
  // TODO(bshe): We current disabled the customized wallpaper feature for
  // Ephemeral user. As we dont want to keep a copy of customized wallpaper in
  // memory. Need a smarter way to solve this.
  if (IsCurrentUserEphemeral())
    return;
  const chromeos::User& user = GetLoggedInUser();
  std::string username = user.email();
  DCHECK(!username.empty());

  std::string file_path = GetWallpaperPathForUser(username, false).value();
  SaveWallpaperToLocalState(username, file_path, layout, User::CUSTOMIZED);
  // Load wallpaper from file.
  UserSelected(username);
}

void UserManagerImpl::SaveUserImageFromFile(const std::string& username,
                                            const FilePath& path) {
  image_loader_->Start(
      path.value(), login::kUserImageSize,
      base::Bind(&UserManagerImpl::SaveUserImage,
                 base::Unretained(this), username));
}

void UserManagerImpl::SaveUserWallpaperFromFile(const std::string& username,
                                                const FilePath& path,
                                                ash::WallpaperLayout layout,
                                                WallpaperDelegate* delegate) {
  // For wallpapers, save the image without resizing.
  image_loader_->Start(
      path.value(), 0 /* Original size */,
      base::Bind(&UserManagerImpl::SaveUserWallpaperInternal,
                 base::Unretained(this), username, layout, User::CUSTOMIZED,
                 delegate));
}

void UserManagerImpl::SaveUserImageFromProfileImage(
    const std::string& username) {
  if (!downloaded_profile_image_.empty()) {
    // Profile image has already been downloaded, so save it to file right now.
    SaveUserImageInternal(username, User::kProfileImageIndex,
                          downloaded_profile_image_);
  } else {
    // No profile image - use the stub image (gray avatar).
    SetUserImage(username, User::kProfileImageIndex, SkBitmap());
    SaveImageToLocalState(username, "", User::kProfileImageIndex, false);
  }
}

void UserManagerImpl::DownloadProfileImage(const std::string& reason) {
  if (profile_image_downloader_.get()) {
    // Another download is already in progress
    return;
  }

  if (IsLoggedInAsGuest()) {
    // This is a guest login so there's no profile image to download.
    return;
  }

  profile_image_download_reason_ = reason;
  profile_image_load_start_time_ = base::Time::Now();
  profile_image_downloader_.reset(new ProfileDownloader(this));
  profile_image_downloader_->Start();
}

void UserManagerImpl::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED:
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                              base::Bind(&UserManagerImpl::CheckOwnership,
                                         base::Unretained(this)));
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&UserManagerImpl::RetrieveTrustedDevicePolicies,
          base::Unretained(this)));
      break;
    case chrome::NOTIFICATION_PROFILE_ADDED:
      if (IsUserLoggedIn() && !IsLoggedInAsGuest() && !IsLoggedInAsStub()) {
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
  DCHECK(IsUserLoggedIn() && !IsLoggedInAsGuest() && !IsLoggedInAsStub());
  if (observed_sync_service_->GetAuthError().state() != AuthError::NONE) {
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
  base::AutoLock lk(is_current_user_owner_lock_);
  return is_current_user_owner_;
}

void UserManagerImpl::SetCurrentUserIsOwner(bool is_current_user_owner) {
  base::AutoLock lk(is_current_user_owner_lock_);
  is_current_user_owner_ = is_current_user_owner;
}

bool UserManagerImpl::IsCurrentUserNew() const {
  return is_current_user_new_;
}

bool UserManagerImpl::IsCurrentUserEphemeral() const {
  return is_current_user_ephemeral_;
}

bool UserManagerImpl::IsUserLoggedIn() const {
  return logged_in_user_;
}

bool UserManagerImpl::IsLoggedInAsDemoUser() const {
  return IsUserLoggedIn() && logged_in_user_->email() == kDemoUser;
}

bool UserManagerImpl::IsLoggedInAsGuest() const {
  return IsUserLoggedIn() && logged_in_user_->email() == kGuestUser;
}

bool UserManagerImpl::IsLoggedInAsStub() const {
  return IsUserLoggedIn() && logged_in_user_->email() == kStubUser;
}

bool UserManagerImpl::IsSessionStarted() const {
  return session_started_;
}

void UserManagerImpl::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void UserManagerImpl::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

const SkBitmap& UserManagerImpl::DownloadedProfileImage() const {
  return downloaded_profile_image_;
}

void UserManagerImpl::NotifyLocalStateChanged() {
  FOR_EACH_OBSERVER(
    Observer,
    observer_list_,
    LocalStateChanged(this));
}

FilePath UserManagerImpl::GetImagePathForUser(const std::string& username) {
  std::string filename = username + ".png";
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.AppendASCII(filename);
}

FilePath UserManagerImpl::GetWallpaperPathForUser(const std::string& username,
                                                  bool is_thumbnail) {
  std::string filename = username +
      (is_thumbnail ? "_wallpaper_thumb.png" : "_wallpaper.png");
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
  const DictionaryValue* prefs_display_emails =
      local_state->GetDictionary(UserManager::kUserDisplayEmail);

  if (prefs_users) {
    for (ListValue::const_iterator it = prefs_users->begin();
         it != prefs_users->end(); ++it) {
      std::string email;
      if ((*it)->GetAsString(&email)) {
        User* user = CreateUser(email);
        users_.push_back(user);

        if (prefs_images) {
          // Get account image path.
          // TODO(avayvod): Reading image path as a string is here for
          // backward compatibility.
          std::string image_path;
          base::DictionaryValue* image_properties;
          if (prefs_images->GetStringWithoutPathExpansion(email, &image_path)) {
            int image_id = User::kInvalidImageIndex;
            if (IsDefaultImagePath(image_path, &image_id)) {
              user->SetImage(GetDefaultImage(image_id), image_id);
            } else {
              int image_index = User::kExternalImageIndex;
              // Until image has been loaded, use the stub image.
              user->SetStubImage(image_index);
              DCHECK(!image_path.empty());
              // Load user image asynchronously.
              image_loader_->Start(
                  image_path, 0,
                  base::Bind(&UserManagerImpl::SetUserImage,
                             base::Unretained(this), email, image_index));
            }
          } else if (prefs_images->GetDictionaryWithoutPathExpansion(
                         email, &image_properties)) {
            int image_index = User::kInvalidImageIndex;
            image_properties->GetString(kImagePathNodeName, &image_path);
            image_properties->GetInteger(kImageIndexNodeName, &image_index);
            if (image_index >= 0 && image_index < kDefaultImagesCount) {
              user->SetImage(GetDefaultImage(image_index), image_index);
            } else if (image_index == User::kExternalImageIndex ||
                       image_index == User::kProfileImageIndex) {
              // Path may be empty for profile images (meaning that the image
              // hasn't been downloaded for the first time yet, in which case a
              // download will be scheduled for |kProfileImageDownloadDelayMs|
              // after user logs in).
              DCHECK(!image_path.empty() ||
                     image_index == User::kProfileImageIndex);
              // Until image has been loaded, use the stub image (gray avatar).
              user->SetStubImage(image_index);
              if (!image_path.empty()) {
                // Load user image asynchronously.
                image_loader_->Start(
                    image_path, 0,
                    base::Bind(&UserManagerImpl::SetUserImage,
                               base::Unretained(this), email, image_index));
              }
            } else {
              NOTREACHED();
            }
          }
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
  if (!cros_settings->PrepareTrustedValues(
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

bool UserManagerImpl::IsEphemeralUser(const std::string& email) const {
  // The guest user always is ephemeral.
  if (email == kGuestUser)
    return true;

  // The currently logged-in user is ephemeral iff logged in as ephemeral.
  if (logged_in_user_ && (email == logged_in_user_->email()))
    return is_current_user_ephemeral_;

  // Any other user is ephemeral iff ephemeral users are enabled, the user is
  // not the owner and is not in the persistent list.
  return AreEphemeralUsersEnabled() &&
      (email != owner_email_) &&
      !FindUserInList(email);
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

  // Schedules current user ownership check on file thread.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&UserManagerImpl::CheckOwnership,
                                     base::Unretained(this)));
}

void UserManagerImpl::SetInitialUserImage(const std::string& username) {
  // Choose a random default image.
  int image_id = base::RandInt(0, kDefaultImagesCount - 1);
  SaveUserDefaultImageIndex(username, image_id);
}

void UserManagerImpl::SetInitialUserWallpaper(const std::string& username) {
  current_user_wallpaper_type_ = User::DEFAULT;
  // TODO(bshe): Ideally, wallpaper should start to load as soon as user click
  // "Add user".
  if (username == kGuestUser)
    current_user_wallpaper_index_ = ash::GetGuestWallpaperIndex();
  else
    current_user_wallpaper_index_ = ash::GetDefaultWallpaperIndex();
  SaveUserWallpaperProperties(username,
                              current_user_wallpaper_type_,
                              current_user_wallpaper_index_);

  // Some browser tests do not have shell instance. And it is not necessary to
  // create a wallpaper for these tests. Add HasInstance check to prevent tests
  // crash and speed up the tests by avoid loading wallpaper.
  if (ash::Shell::HasInstance()) {
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetDefaultWallpaper(current_user_wallpaper_index_);
  }
}

void UserManagerImpl::MigrateWallpaperData() {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    const DictionaryValue* user_wallpapers =
          local_state->GetDictionary(kUserWallpapers);
    int index;
    if (user_wallpapers) {
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
          SaveUserWallpaperProperties(username, User::DEFAULT, index);
        }
      }
    }
  }
}

int UserManagerImpl::GetLoggedInUserWallpaperIndex() {
  User::WallpaperType type;
  int index;
  GetLoggedInUserWallpaperProperties(&type, &index);
  return index;
}

void UserManagerImpl::GetLoggedInUserWallpaperProperties(
    User::WallpaperType* type,
    int* index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!IsUserLoggedIn() || IsLoggedInAsStub()) {
    *type = current_user_wallpaper_type_ = User::DEFAULT;
    *index = current_user_wallpaper_index_ = ash::GetInvalidWallpaperIndex();
    return;
  }

  GetUserWallpaperProperties(GetLoggedInUser().email(), type, index);
}

void UserManagerImpl::SaveLoggedInUserWallpaperProperties(
    User::WallpaperType type,
    int index) {
  SaveUserWallpaperProperties(GetLoggedInUser().email(), type, index);
}

void UserManagerImpl::SetUserImage(const std::string& username,
                                   int image_index,
                                   const SkBitmap& image) {
  User* user = const_cast<User*>(FindUser(username));
  // User may have been removed by now.
  if (user) {
    // For existing users, a valid image index should have been set upon loading
    // them from Local State.
    DCHECK(user->image_index() != User::kInvalidImageIndex ||
           is_current_user_new_);
    bool image_changed = user->image_index() != User::kInvalidImageIndex;
    if (!image.empty())
      user->SetImage(image, image_index);
    else
      user->SetStubImage(image_index);
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

void UserManagerImpl::GetUserWallpaperProperties(const std::string& username,
                                                 User::WallpaperType* type,
                                                 int* index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Default to the values cached in memory.
  *type = current_user_wallpaper_type_;
  *index = current_user_wallpaper_index_;

  // Override with values found in local store, if any.
  if (!username.empty()) {
    const DictionaryValue* user_wallpapers = g_browser_process->local_state()->
        GetDictionary(UserManager::kUserWallpapersProperties);
    if (user_wallpapers) {
      base::DictionaryValue* wallpaper_properties;
      if (user_wallpapers->GetDictionaryWithoutPathExpansion(
          username,
          &wallpaper_properties)) {
        *type = User::UNKNOWN;
        *index = ash::GetDefaultWallpaperIndex();
        wallpaper_properties->GetInteger(kWallpaperTypeNodeName,
                                         reinterpret_cast<int*>(type));
        wallpaper_properties->GetInteger(kWallpaperIndexNodeName, index);
      }
    }
  }
}

void UserManagerImpl::SaveUserWallpaperProperties(const std::string& username,
                                                  User::WallpaperType type,
                                                  int index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  current_user_wallpaper_type_ = type;
  current_user_wallpaper_index_ = index;
  // Ephemeral users can not save data to local state. We just cache the index
  // in memory for them.
  if (IsCurrentUserEphemeral()) {
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate wallpaper_update(local_state,
      UserManager::kUserWallpapersProperties);

  base::DictionaryValue* wallpaper_properties = new base::DictionaryValue();
  wallpaper_properties->Set(kWallpaperTypeNodeName,
                            new base::FundamentalValue(type));
  wallpaper_properties->Set(kWallpaperIndexNodeName,
                            new base::FundamentalValue(index));
  wallpaper_update->SetWithoutPathExpansion(username, wallpaper_properties);
}

void UserManagerImpl::SaveUserImageInternal(const std::string& username,
                                            int image_index,
                                            const SkBitmap& image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetUserImage(username, image_index, image);

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
                 username, image, image_path, image_index));
}

void UserManagerImpl::SaveUserWallpaperInternal(const std::string& username,
                                                ash::WallpaperLayout layout,
                                                User::WallpaperType type,
                                                WallpaperDelegate* delegate,
                                                const SkBitmap& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UserManagerImpl::GenerateUserWallpaperThumbnail,
                 base::Unretained(this), username, type, delegate, wallpaper));

  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper, layout);

  // Ignore for ephemeral users.
  if (IsEphemeralUser(username))
    return;

  FilePath wallpaper_path = GetWallpaperPathForUser(username, false);
  DVLOG(1) << "Saving user image to " << wallpaper_path.value();

  last_image_set_async_ = true;

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UserManagerImpl::SaveWallpaperToFile,
                 base::Unretained(this), username, wallpaper, wallpaper_path,
                 layout, User::CUSTOMIZED));
}

void UserManagerImpl::LoadCustomWallpaperThumbnail(const std::string& email,
                                                   ash::WallpaperLayout layout,
                                                   const SkBitmap& wallpaper) {
  ash::Shell::GetInstance()->desktop_background_controller()->
      SetCustomWallpaper(wallpaper, layout);
  // Load wallpaper thumbnail
  std::string wallpaper_path = GetWallpaperPathForUser(email, true).value();
  image_loader_->Start(
      wallpaper_path, 0,
      base::Bind(&UserManagerImpl::OnCustomWallpaperThumbnailLoaded,
      base::Unretained(this), email));
}

void UserManagerImpl::OnCustomWallpaperThumbnailLoaded(
    const std::string& email,
    const SkBitmap& wallpaper) {
  User* user = const_cast<User*>(FindUser(email));
  // User may have been removed by now.
  if (user && !wallpaper.empty())
    user->SetWallpaperThumbnail(wallpaper);
}

void UserManagerImpl::OnThumbnailUpdated(WallpaperDelegate* delegate) {
  if (delegate)
    delegate->SetCustomWallpaperThumbnail();
}

void UserManagerImpl::GenerateUserWallpaperThumbnail(
    const std::string& username,
    User::WallpaperType type,
    WallpaperDelegate* delegate,
    const SkBitmap& wallpaper) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SkBitmap thumbnail =
      skia::ImageOperations::Resize(wallpaper,
                                    skia::ImageOperations::RESIZE_LANCZOS3,
                                    kThumbnailWidth, kThumbnailHeight);
  logged_in_user_->SetWallpaperThumbnail(thumbnail);

  // Notify thumbnail is ready.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UserManagerImpl::OnThumbnailUpdated,
                 base::Unretained(this), delegate));

  // Ignore for ephemeral users.
  if (IsEphemeralUser(username))
    return;

  FilePath thumbnail_path = GetWallpaperPathForUser(username, true);
  SaveBitmapToFile(thumbnail, thumbnail_path);
}

void UserManagerImpl::SaveImageToFile(const std::string& username,
                                      const SkBitmap& image,
                                      const FilePath& image_path,
                                      int image_index) {
  if (!SaveBitmapToFile(image, image_path))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UserManagerImpl::SaveImageToLocalState,
                 base::Unretained(this),
                 username, image_path.value(), image_index, true));
}

void UserManagerImpl::SaveWallpaperToFile(const std::string& username,
                                          const SkBitmap& wallpaper,
                                          const FilePath& wallpaper_path,
                                          ash::WallpaperLayout layout,
                                          User::WallpaperType type) {
  // TODO(bshe): We should save the original file unchanged instead of
  // re-encoding it and saving it.
  if (!SaveBitmapToFile(wallpaper, wallpaper_path))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UserManagerImpl::SaveWallpaperToLocalState,
                 base::Unretained(this),
                 username, wallpaper_path.value(), layout, type));
}

void UserManagerImpl::SaveImageToLocalState(const std::string& username,
                                            const std::string& image_path,
                                            int image_index,
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
  images_update->SetWithoutPathExpansion(username, image_properties);
  DVLOG(1) << "Saving path to user image in Local State.";

  NotifyLocalStateChanged();
}

void UserManagerImpl::SaveWallpaperToLocalState(const std::string& username,
    const std::string& wallpaper_path,
    ash::WallpaperLayout layout,
    User::WallpaperType type) {
  // TODO(bshe): We probably need to save wallpaper_path instead of index.
  SaveUserWallpaperProperties(username, type, layout);
}

bool UserManagerImpl::SaveBitmapToFile(const SkBitmap& image,
                                       const FilePath& image_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::vector<unsigned char> encoded_image;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(image, false, &encoded_image)) {
    LOG(ERROR) << "Failed to PNG encode the image.";
    return false;
  }

  if (file_util::WriteFile(image_path,
                           reinterpret_cast<char*>(&encoded_image[0]),
                           encoded_image.size()) == -1) {
    LOG(ERROR) << "Failed to save image to file.";
    return false;
  }
  return true;
}

void UserManagerImpl::InitDownloadedProfileImage() {
  DCHECK(logged_in_user_);
  if (downloaded_profile_image_.empty() && !logged_in_user_->image_is_stub()) {
    VLOG(1) << "Profile image initialized";
    downloaded_profile_image_ = logged_in_user_->image();
    downloaded_profile_image_data_url_ =
        web_ui_util::GetImageDataUrl(downloaded_profile_image_);
  }
}

void UserManagerImpl::DeleteUserImage(const FilePath& image_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::Delete(image_path, false)) {
    LOG(ERROR) << "Failed to remove user image.";
    return;
  }
}

void UserManagerImpl::UpdateOwnership(bool is_owner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetCurrentUserIsOwner(is_owner);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_OWNERSHIP_CHECKED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
  if (is_owner) {
    // Also update cached value.
    CrosSettings::Get()->SetString(kDeviceOwner, GetLoggedInUser().email());
  }
}

void UserManagerImpl::CheckOwnership() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool is_owner = OwnershipService::GetSharedInstance()->IsCurrentUserOwner();
  VLOG(1) << "Current user " << (is_owner ? "is owner" : "is not owner");

  SetCurrentUserIsOwner(is_owner);

  // UserManagerImpl should be accessed only on UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UserManagerImpl::UpdateOwnership,
                 base::Unretained(this),
                 is_owner));
}

int UserManagerImpl::GetDesiredImageSideLength() const {
  return login::kUserImageSize;
}

Profile* UserManagerImpl::GetBrowserProfile() {
  return ProfileManager::GetDefaultProfile();
}

std::string UserManagerImpl::GetCachedPictureURL() const {
  // Currently the profile picture URL is not cached on ChromeOS.
  return std::string();
}

void UserManagerImpl::OnDownloadComplete(ProfileDownloader* downloader,
                                         bool success) {
  // Make sure that |ProfileDownloader| gets deleted after return.
  scoped_ptr<ProfileDownloader> profile_image_downloader(
      profile_image_downloader_.release());
  DCHECK(profile_image_downloader.get() == downloader);

  ProfileDownloadResult result;
  if (!success) {
    result = kDownloadFailure;
  } else if (downloader->GetProfilePicture().isNull()) {
    result = kDownloadDefault;
  } else {
    result = kDownloadSuccess;
  }
  UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
      result, kDownloadResultsCount);

  DCHECK(!profile_image_load_start_time_.is_null());
  base::TimeDelta delta = base::Time::Now() - profile_image_load_start_time_;
  AddProfileImageTimeHistogram(result, profile_image_download_reason_, delta);

  if (result == kDownloadSuccess) {
    // Check if this image is not the same as already downloaded.
    std::string new_image_data_url =
        web_ui_util::GetImageDataUrl(downloader->GetProfilePicture());
    if (!downloaded_profile_image_data_url_.empty() &&
        new_image_data_url == downloaded_profile_image_data_url_)
      return;

    downloaded_profile_image_data_url_ = new_image_data_url;
    downloaded_profile_image_ = downloader->GetProfilePicture();

    if (GetLoggedInUser().image_index() == User::kProfileImageIndex) {
      VLOG(1) << "Updating profile image for logged-in user";
      UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
                                kDownloadSuccessChanged,
                                kDownloadResultsCount);

      // This will persist |downloaded_profile_image_| to file.
      SaveUserImageFromProfileImage(GetLoggedInUser().email());
    }
  }

  if (result == kDownloadSuccess) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
        content::Source<UserManagerImpl>(this),
        content::Details<const SkBitmap>(&downloaded_profile_image_));
  } else {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
        content::Source<UserManagerImpl>(this),
        content::NotificationService::NoDetails());
  }
}

User* UserManagerImpl::CreateUser(const std::string& email) const {
  User* user = new User(email, email == kGuestUser);
  user->set_oauth_token_status(LoadUserOAuthStatus(email));
  // Used to determine whether user's display name is unique.
  ++display_name_count_[user->GetDisplayName()];
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

  // Remove user wallpaper thumbnail
  FilePath wallpaper_thumb_path = GetWallpaperPathForUser(email, true);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UserManagerImpl::DeleteUserImage,
                 base::Unretained(this),
                 wallpaper_thumb_path));
  // Remove user wallpaper
  FilePath wallpaper_path = GetWallpaperPathForUser(email, false);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UserManagerImpl::DeleteUserImage,
                 base::Unretained(this),
                 wallpaper_path));

  DictionaryPrefUpdate prefs_images_update(prefs, kUserImages);
  std::string image_path_string;
  prefs_images_update->GetStringWithoutPathExpansion(email, &image_path_string);
  prefs_images_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_oauth_update(prefs, kUserOAuthTokenStatus);
  int oauth_status;
  prefs_oauth_update->GetIntegerWithoutPathExpansion(email, &oauth_status);
  prefs_oauth_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_display_email_update(prefs, kUserDisplayEmail);
  prefs_display_email_update->RemoveWithoutPathExpansion(email, NULL);

  if (user_to_remove != users_.end()) {
    --display_name_count_[(*user_to_remove)->GetDisplayName()];
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
