// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/task.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_constants.h"
#include "crypto/nss_util.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#endif

using content::BrowserThread;

namespace chromeos {

namespace {

// A vector pref of the users who have logged into the device.
const char kLoggedInUsers[] = "LoggedInUsers";
// A dictionary that maps usernames to file paths to their images.
const char kUserImages[] = "UserImages";
// A dictionary that maps usernames to OAuth token presence flag.
const char kUserOAuthTokenStatus[] = "OAuthTokenStatus";

// Incognito user is represented by an empty string (since some code already
// depends on that and it's hard to figure out what).
const char kGuestUser[] = "";

// Stub user email (for test paths).
const char kStubUser[] = "stub-user@example.com";

// Names of nodes with info about user image.
const char kImagePathNodeName[] = "path";
const char kImageIndexNodeName[] = "index";

// Index of the default image used for the |kStubUser| user.
const int kStubDefaultImageIndex = 0;

// Delay betweeen user login and attempt to update user's profile image.
const long kProfileImageDownloadDelayMs = 10000;

base::LazyInstance<UserManager> g_user_manager = LAZY_INSTANCE_INITIALIZER;

// Enum for reporting histograms about profile picture download.
enum ProfileDownloadResult {
  kDownloadSuccessChanged,
  kDownloadSuccess,
  kDownloadFailure,
  kDownloadDefault,

  // Must be the last, convenient count.
  kDownloadResultsCount
};

// Time histogram name for the default profile image download.
const char kProfileDownloadDefaultTime[] =
    "UserImage.ProfileDownloadTime.Default";
// Time histogram name for a failed profile image download.
const char kProfileDownloadFailureTime[] =
    "UserImage.ProfileDownloadTime.Failure";
// Time histogram name for a successful profile image download.
const char ProfileDownloadSuccessTime[] =
    "UserImage.ProfileDownloadTime.Success";

// Used to handle the asynchronous response of deleting a cryptohome directory.
class RemoveAttempt : public CryptohomeLibrary::Delegate {
 public:
  // Creates new remove attempt for the given user. Note, |delegate| can
  // be NULL.
  RemoveAttempt(const std::string& user_email,
                chromeos::RemoveUserDelegate* delegate)
      : user_email_(user_email),
        delegate_(delegate),
        weak_factory_(this) {
    RemoveUser();
  }

  virtual ~RemoveAttempt() {}

  void RemoveUser() {
    // Owner is not allowed to be removed from the device.
    // Must not proceed without signature verification.
    CrosSettings* cros_settings = CrosSettings::Get();
    bool trusted_owner_available = cros_settings->GetTrusted(
        kDeviceOwner,
        base::Bind(&RemoveAttempt::RemoveUser, weak_factory_.GetWeakPtr()));
    if (!trusted_owner_available) {
      // Value of owner email is still not verified.
      // Another attempt will be invoked after verification completion.
      return;
    }
    std::string owner;
    cros_settings->GetString(kDeviceOwner, &owner);
    if (user_email_ == owner) {
      // Owner is not allowed to be removed from the device. Probably on
      // the stack, so deffer the deletion.
      MessageLoop::current()->DeleteSoon(FROM_HERE, this);
      return;
    }

    if (delegate_)
      delegate_->OnBeforeUserRemoved(user_email_);

    chromeos::UserManager::Get()->RemoveUserFromList(user_email_);
    RemoveUserCryptohome();

    if (delegate_)
      delegate_->OnUserRemoved(user_email_);
  }

  void RemoveUserCryptohome() {
    CrosLibrary::Get()->GetCryptohomeLibrary()->AsyncRemove(user_email_, this);
  }

  void OnComplete(bool success, int return_code) {
    // Log the error, but there's not much we can do.
    if (!success) {
      VLOG(1) << "Removal of cryptohome for " << user_email_
              << " failed, return code: " << return_code;
    }
    delete this;
  }

 private:
  std::string user_email_;
  chromeos::RemoveUserDelegate* delegate_;

  // Factory of callbacks.
  base::WeakPtrFactory<RemoveAttempt> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoveAttempt);
};

class RealTPMTokenInfoDelegate : public crypto::TPMTokenInfoDelegate {
 public:
  RealTPMTokenInfoDelegate();
  virtual ~RealTPMTokenInfoDelegate();
  virtual bool IsTokenAvailable() const OVERRIDE;
  virtual bool IsTokenReady() const OVERRIDE;
  virtual void GetTokenInfo(std::string* token_name,
                            std::string* user_pin) const OVERRIDE;
};

RealTPMTokenInfoDelegate::RealTPMTokenInfoDelegate() {}
RealTPMTokenInfoDelegate::~RealTPMTokenInfoDelegate() {}

bool RealTPMTokenInfoDelegate::IsTokenAvailable() const {
  return CrosLibrary::Get()->GetCryptohomeLibrary()->TpmIsEnabled();
}

bool RealTPMTokenInfoDelegate::IsTokenReady() const {
  return CrosLibrary::Get()->GetCryptohomeLibrary()->Pkcs11IsTpmTokenReady();
}

void RealTPMTokenInfoDelegate::GetTokenInfo(std::string* token_name,
                                            std::string* user_pin) const {
  std::string local_token_name;
  std::string local_user_pin;
  CrosLibrary::Get()->GetCryptohomeLibrary()->Pkcs11GetTpmTokenInfo(
      &local_token_name, &local_user_pin);
  if (token_name)
    *token_name = local_token_name;
  if (user_pin)
    *user_pin = local_user_pin;
}

}  // namespace

// static
UserManager* UserManager::Get() {
  // Not thread-safe.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return &g_user_manager.Get();
}

// static
void UserManager::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterListPref(kLoggedInUsers, PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserImages,
                                      PrefService::UNSYNCABLE_PREF);
  local_state->RegisterDictionaryPref(kUserOAuthTokenStatus,
                                      PrefService::UNSYNCABLE_PREF);
}

const UserList& UserManager::GetUsers() const {
  const_cast<UserManager*>(this)->EnsureUsersLoaded();
  return users_;
}

void UserManager::UserLoggedIn(const std::string& email) {
  DCHECK(!user_is_logged_in_);

  user_is_logged_in_ = true;

  if (email == kGuestUser) {
    GuestUserLoggedIn();
    return;
  }

  EnsureUsersLoaded();

  // Clear the prefs view of the users.
  PrefService* prefs = g_browser_process->local_state();
  ListPrefUpdate prefs_users_update(prefs, kLoggedInUsers);
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
  prefs->ScheduleSavePersistentPrefs();

  if (logged_in_user == users_.end()) {
    current_user_is_new_ = true;
    logged_in_user_ = CreateUser(email);
  } else {
    logged_in_user_ = *logged_in_user;
    users_.erase(logged_in_user);
  }
  // This user must be in the front of the user list.
  users_.insert(users_.begin(), logged_in_user_);

  NotifyOnLogin();

  if (current_user_is_new_) {
    SetInitialUserImage(email);
  } else {
    // Download profile image if it's user image and see if it has changed.
    int image_index = logged_in_user_->image_index();
    if (image_index == User::kProfileImageIndex) {
      BrowserThread::PostDelayedTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&UserManager::DownloadProfileImage,
                     base::Unretained(this)),
          kProfileImageDownloadDelayMs);
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
}

void UserManager::GuestUserLoggedIn() {
  logged_in_user_ = &guest_user_;
  NotifyOnLogin();
}

void UserManager::RemoveUser(const std::string& email,
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

  // |RemoveAttempt| deletes itself when done.
  new RemoveAttempt(email, delegate);
}

void UserManager::RemoveUserFromList(const std::string& email) {
  EnsureUsersLoaded();

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

  DictionaryPrefUpdate prefs_images_update(prefs, kUserImages);
  std::string image_path_string;
  prefs_images_update->GetStringWithoutPathExpansion(email, &image_path_string);
  prefs_images_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_oauth_update(prefs, kUserOAuthTokenStatus);
  int oauth_status;
  prefs_oauth_update->GetIntegerWithoutPathExpansion(email, &oauth_status);
  prefs_oauth_update->RemoveWithoutPathExpansion(email, NULL);

  prefs->ScheduleSavePersistentPrefs();

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
        base::Bind(&UserManager::DeleteUserImage,
                   base::Unretained(this),
                   image_path));
  }
}

bool UserManager::IsKnownUser(const std::string& email) const {
  return FindUser(email) != NULL;
}

const User* UserManager::FindUser(const std::string& email) const {
  // Speed up search by checking the logged-in user first.
  if (logged_in_user_ && logged_in_user_->email() == email)
    return logged_in_user_;
  const UserList& users = GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->email() == email)
      return *it;
  }
  return NULL;
}

bool UserManager::IsDisplayNameUnique(const std::string& display_name) const {
  return display_name_count_[display_name] < 2;
}

void UserManager::SaveUserOAuthStatus(
    const std::string& username,
    User::OAuthTokenStatus oauth_token_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate oauth_status_update(local_state, kUserOAuthTokenStatus);
  oauth_status_update->SetWithoutPathExpansion(username,
      new base::FundamentalValue(static_cast<int>(oauth_token_status)));
  DVLOG(1) << "Saving user OAuth token status in Local State.";
  local_state->ScheduleSavePersistentPrefs();
  User* user = const_cast<User*>(FindUser(username));
  if (user)
    user->set_oauth_token_status(oauth_token_status);
}

User::OAuthTokenStatus UserManager::GetUserOAuthStatus(
    const std::string& username) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSkipOAuthLogin)) {
    // Use OAUTH_TOKEN_STATUS_VALID flag if kSkipOAuthLogin is present.
    return User::OAUTH_TOKEN_STATUS_VALID;
  } else {
    PrefService* local_state = g_browser_process->local_state();
    const DictionaryValue* prefs_oauth_status =
        local_state->GetDictionary(kUserOAuthTokenStatus);

    int oauth_token_status = User::OAUTH_TOKEN_STATUS_UNKNOWN;
    if (prefs_oauth_status &&
        prefs_oauth_status->GetIntegerWithoutPathExpansion(username,
            &oauth_token_status)) {
      return static_cast<User::OAuthTokenStatus>(oauth_token_status);
    }
  }

  return User::OAUTH_TOKEN_STATUS_UNKNOWN;
}

void UserManager::SaveUserDefaultImageIndex(const std::string& username,
                                            int image_index) {
  DCHECK(image_index >= 0 && image_index < kDefaultImagesCount);
  SetUserImage(username, image_index, GetDefaultImage(image_index));
  SaveImageToLocalState(username, "", image_index, false);
}

void UserManager::SaveUserImage(const std::string& username,
                                const SkBitmap& image) {
  SaveUserImageInternal(username, User::kExternalImageIndex, image);
}

void UserManager::SaveUserImageFromFile(const std::string& username,
                                        const FilePath& path) {
  image_loader_->Start(
      path.value(), login::kUserImageSize,
      base::Bind(&UserManager::SaveUserImage,
                 base::Unretained(this), username));
}

void UserManager::SaveUserImageFromProfileImage(const std::string& username) {
  if (!downloaded_profile_image_.empty()) {
    // Profile image has already been downloaded, so save it to file right now.
    SaveUserImageInternal(username, User::kProfileImageIndex,
                          downloaded_profile_image_);
  } else {
    // No profile image - use the default gray avatar.
    SetUserImage(username, User::kProfileImageIndex,
                 *ResourceBundle::GetSharedInstance().
                     GetBitmapNamed(IDR_PROFILE_PICTURE_LOADING));
    SaveImageToLocalState(username, "", User::kProfileImageIndex, false);
  }
}

void UserManager::DownloadProfileImage() {
  if (profile_image_downloader_.get()) {
    // Another download is already in progress
    return;
  }

  if (logged_in_user().email().empty()) {
    // This is a guest login so there's no profile image to download.
    return;
  }

  profile_image_load_start_time_ = base::Time::Now();
  profile_image_downloader_.reset(new ProfileDownloader(this));
  profile_image_downloader_->Start();
}

void UserManager::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&UserManager::CheckOwnership,
                                       base::Unretained(this)));
  }
}

bool UserManager::current_user_is_owner() const {
  base::AutoLock lk(current_user_is_owner_lock_);
  return current_user_is_owner_;
}

void UserManager::set_current_user_is_owner(bool current_user_is_owner) {
  base::AutoLock lk(current_user_is_owner_lock_);
  current_user_is_owner_ = current_user_is_owner;
}

bool UserManager::IsLoggedInAsGuest() const {
  return logged_in_user_ == &guest_user_;
}

void UserManager::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void UserManager::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

void UserManager::NotifyLocalStateChanged() {
  FOR_EACH_OBSERVER(
    Observer,
    observer_list_,
    LocalStateChanged(this));
}

// Protected constructor and destructor.
UserManager::UserManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(image_loader_(new UserImageLoader)),
      guest_user_(kGuestUser),
      stub_user_(kStubUser),
      logged_in_user_(NULL),
      current_user_is_owner_(false),
      current_user_is_new_(false),
      user_is_logged_in_(false),
      last_image_set_async_(false),
      downloaded_profile_image_data_url_(chrome::kAboutBlankURL) {
  // Use stub as the logged-in user for test paths without login.
  if (!system::runtime_environment::IsRunningOnChromeOS())
    StubUserLoggedIn();
  registrar_.Add(this, chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,
      content::NotificationService::AllSources());
}

UserManager::~UserManager() {
}

FilePath UserManager::GetImagePathForUser(const std::string& username) {
  std::string filename = username + ".png";
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.AppendASCII(filename);
}

void UserManager::EnsureUsersLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!users_.empty())
    return;
  if (!g_browser_process)
    return;

  PrefService* local_state = g_browser_process->local_state();
  const ListValue* prefs_users = local_state->GetList(kLoggedInUsers);
  const DictionaryValue* prefs_images = local_state->GetDictionary(kUserImages);

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
              // Until image has been loaded, use a stub.
              user->SetImage(*ResourceBundle::GetSharedInstance().
                                 GetBitmapNamed(IDR_PROFILE_PICTURE_LOADING),
                             image_index);
              DCHECK(!image_path.empty());
              // Load user image asynchronously.
              image_loader_->Start(
                  image_path, 0,
                  base::Bind(&UserManager::SetUserImage,
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
              // Until image has been loaded, use a gray avatar.
              user->SetImage(*ResourceBundle::GetSharedInstance().
                                 GetBitmapNamed(IDR_PROFILE_PICTURE_LOADING),
                             image_index);
              if (!image_path.empty()) {
                // Load user image asynchronously.
                image_loader_->Start(
                    image_path, 0,
                    base::Bind(&UserManager::SetUserImage,
                               base::Unretained(this), email, image_index));
              }
            } else {
              NOTREACHED();
            }
          }
        }
      }
    }
  }
}

void UserManager::StubUserLoggedIn() {
  logged_in_user_ = &stub_user_;
  stub_user_.SetImage(GetDefaultImage(kStubDefaultImageIndex),
                      kStubDefaultImageIndex);
}

void UserManager::NotifyOnLogin() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::Source<UserManager>(this),
      content::Details<const User>(logged_in_user_));

  chromeos::input_method::InputMethodManager::GetInstance()->
      SetDeferImeStartup(false);
  // Shut down the IME so that it will reload the user's settings.
  chromeos::input_method::InputMethodManager::GetInstance()->
      StopInputMethodDaemon();

#if defined(TOOLKIT_USES_GTK)
  // Let the window manager know that we're logged in now.
  WmIpc::instance()->SetLoggedInProperty(true);
#endif

  // Ensure we've opened the real user's key/certificate database.
  crypto::OpenPersistentNSSDB();

  // Only load the Opencryptoki library into NSS if we have this switch.
  // TODO(gspencer): Remove this switch once cryptohomed work is finished:
  // http://crosbug.com/12295 and http://crosbug.com/12304
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLoadOpencryptoki)) {
    crypto::EnableTPMTokenForNSS(new RealTPMTokenInfoDelegate());
    CertLibrary* cert_library;
    cert_library = chromeos::CrosLibrary::Get()->GetCertLibrary();
    cert_library->RequestCertificates();
  }

  // Schedules current user ownership check on file thread.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&UserManager::CheckOwnership,
                                     base::Unretained(this)));
}

void UserManager::SetInitialUserImage(const std::string& username) {
  // Choose a random default image.
  int image_id = base::RandInt(0, kDefaultImagesCount - 1);
  SaveUserDefaultImageIndex(username, image_id);
}

void UserManager::SetUserImage(const std::string& username,
                               int image_index,
                               const SkBitmap& image) {
  User* user = const_cast<User*>(FindUser(username));
  // User may have been removed by now.
  if (user) {
    // For existing users, a valid image index should have been set upon loading
    // them from Local State.
    DCHECK(user->image_index() != User::kInvalidImageIndex ||
           current_user_is_new_);
    bool image_changed = user->image_index() != User::kInvalidImageIndex;
    user->SetImage(image, image_index);
    // If it is the profile image of the current user, save it to
    // |downloaded_profile_image_| so that it can be reused if the started
    // download attempt fails.
    if (image_index == User::kProfileImageIndex && user == logged_in_user_) {
      downloaded_profile_image_ = image;
      downloaded_profile_image_data_url_ = web_ui_util::GetImageDataUrl(image);
    }
    if (image_changed) {
      // Unless this is first-time setting with |SetInitialUserImage|,
      // send a notification about image change.
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
          content::Source<UserManager>(this),
          content::Details<const User>(user));
    }
  }
}

void UserManager::SaveUserImageInternal(const std::string& username,
                                        int image_index,
                                        const SkBitmap& image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetUserImage(username, image_index, image);

  FilePath image_path = GetImagePathForUser(username);
  DVLOG(1) << "Saving user image to " << image_path.value();

  last_image_set_async_ = true;

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UserManager::SaveImageToFile,
                 base::Unretained(this),
                 username, image, image_path, image_index));
}

void UserManager::SaveImageToFile(const std::string& username,
                                  const SkBitmap& image,
                                  const FilePath& image_path,
                                  int image_index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::vector<unsigned char> encoded_image;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(image, true, &encoded_image)) {
    LOG(ERROR) << "Failed to PNG encode the image.";
    return;
  }

  if (file_util::WriteFile(image_path,
                           reinterpret_cast<char*>(&encoded_image[0]),
                           encoded_image.size()) == -1) {
    LOG(ERROR) << "Failed to save image to file.";
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UserManager::SaveImageToLocalState,
                 base::Unretained(this),
                 username, image_path.value(), image_index, true));
}

void UserManager::SaveImageToLocalState(const std::string& username,
                                        const std::string& image_path,
                                        int image_index,
                                        bool is_async) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
  DictionaryPrefUpdate images_update(local_state, kUserImages);
  base::DictionaryValue* image_properties = new base::DictionaryValue();
  image_properties->Set(kImagePathNodeName, new StringValue(image_path));
  image_properties->Set(kImageIndexNodeName,
                        new base::FundamentalValue(image_index));
  images_update->SetWithoutPathExpansion(username, image_properties);
  DVLOG(1) << "Saving path to user image in Local State.";
  local_state->ScheduleSavePersistentPrefs();

  NotifyLocalStateChanged();
}

void UserManager::DeleteUserImage(const FilePath& image_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::Delete(image_path, false)) {
    LOG(ERROR) << "Failed to remove user image.";
    return;
  }
}

void UserManager::UpdateOwnership(bool is_owner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  set_current_user_is_owner(is_owner);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_OWNERSHIP_CHECKED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
  if (is_owner) {
    // Also update cached value.
    CrosSettings::Get()->SetString(
        kDeviceOwner,
        g_user_manager.Get().logged_in_user().email());
  }
}

void UserManager::CheckOwnership() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool is_owner = OwnershipService::GetSharedInstance()->CurrentUserIsOwner();
  VLOG(1) << "Current user " << (is_owner ? "is owner" : "is not owner");

  set_current_user_is_owner(is_owner);

  // UserManager should be accessed only on UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UserManager::UpdateOwnership,
                 base::Unretained(this),
                 is_owner));
}

int UserManager::GetDesiredImageSideLength() const {
  return login::kUserImageSize;
}

Profile* UserManager::GetBrowserProfile() {
  return ProfileManager::GetDefaultProfile();
}

bool UserManager::ShouldUseOAuthRefreshToken() const {
  return false;
}

std::string UserManager::GetCachedPictureURL() const {
  // Currently the profile picture URL is not cached on ChromeOS.
  return std::string();
}

void UserManager::OnDownloadComplete(ProfileDownloader* downloader,
                                     bool success) {
  ProfileDownloadResult result;
  std::string time_histogram_name;
  if (!success) {
    result = kDownloadFailure;
    time_histogram_name = kProfileDownloadFailureTime;
  } else if (downloader->GetProfilePicture().isNull()) {
    result = kDownloadDefault;
    time_histogram_name = kProfileDownloadDefaultTime;
  } else {
    result = kDownloadSuccess;
    time_histogram_name = ProfileDownloadSuccessTime;
  }

  UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
      result, kDownloadResultsCount);

  DCHECK(!profile_image_load_start_time_.is_null());
  base::TimeDelta delta = base::Time::Now() - profile_image_load_start_time_;
  VLOG(1) << "Profile image download time: " << delta.InSecondsF();
  UMA_HISTOGRAM_TIMES(time_histogram_name, delta);

  if (result == kDownloadSuccess) {
    // Check if this image is not the same as already downloaded.
    std::string new_image_data_url =
        web_ui_util::GetImageDataUrl(downloader->GetProfilePicture());
    if (!downloaded_profile_image_data_url_.empty() &&
        new_image_data_url == downloaded_profile_image_data_url_)
      return;

    downloaded_profile_image_data_url_ = new_image_data_url;
    downloaded_profile_image_ = downloader->GetProfilePicture();

    if (logged_in_user().image_index() == User::kProfileImageIndex) {
      VLOG(1) << "Updating profile image for logged-in user";
      UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
                                kDownloadSuccessChanged,
                                kDownloadResultsCount);

      // This will persist |downloaded_profile_image_| to file.
      SaveUserImageFromProfileImage(logged_in_user().email());
    }
  }

  if (result == kDownloadSuccess) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
        content::Source<UserManager>(this),
        content::Details<const SkBitmap>(&downloaded_profile_image_));
  } else {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
        content::Source<UserManager>(this),
        content::NotificationService::NoDetails());
  }

  profile_image_downloader_.reset();
}

User* UserManager::CreateUser(const std::string& email) const {
  User* user = new User(email);
  user->set_oauth_token_status(GetUserOAuthStatus(email));
  // Used to determine whether user's display name is unique.
  ++display_name_count_[user->GetDisplayName()];
  return user;
}

}  // namespace chromeos
