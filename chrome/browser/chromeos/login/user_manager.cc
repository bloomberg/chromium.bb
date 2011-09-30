// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "crypto/nss_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"

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

// Names of nodes with info about user image.
const char kImagePathNodeName[] = "path";
const char kImageIndexNodeName[] = "index";

// Delay betweeen user login and attempt to update user's profile image.
const long kProfileImageDownloadDelayMs = 10000;

base::LazyInstance<UserManager> g_user_manager(base::LINKER_INITIALIZED);

// Stores user's OAuthTokenStatus in local state. Runs on UI thread.
void SaveOAuthTokenStatusToLocalState(const std::string& username,
    UserManager::OAuthTokenStatus oauth_token_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate oauth_status_update(local_state, kUserOAuthTokenStatus);
  oauth_status_update->SetWithoutPathExpansion(username,
      new base::FundamentalValue(static_cast<int>(oauth_token_status)));
  DVLOG(1) << "Saving user OAuth token status in Local State.";
  local_state->SavePersistentPrefs();
}

// Gets user's OAuthTokenStatus from local state.
UserManager::OAuthTokenStatus GetOAuthTokenStatusFromLocalState(
    const std::string& username) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSkipOAuthLogin)) {
    // Use OAUTH_TOKEN_STATUS_VALID flag if kSkipOAuthLogin is present.
    return UserManager::OAUTH_TOKEN_STATUS_VALID;
  } else {
    PrefService* local_state = g_browser_process->local_state();
    const DictionaryValue* prefs_oauth_status =
        local_state->GetDictionary(kUserOAuthTokenStatus);

    int oauth_token_status = UserManager::OAUTH_TOKEN_STATUS_UNKNOWN;
    if (prefs_oauth_status &&
        prefs_oauth_status->GetIntegerWithoutPathExpansion(username,
            &oauth_token_status)) {
      return static_cast<UserManager::OAuthTokenStatus>(oauth_token_status);
    }
  }

  return UserManager::OAUTH_TOKEN_STATUS_UNKNOWN;
}

// Stores path to the image and its index in local state. Runs on UI thread.
void SaveImageToLocalState(const std::string& username,
                           const std::string& image_path,
                           int image_index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate images_update(local_state, kUserImages);
  base::DictionaryValue* image_properties = new base::DictionaryValue();
  image_properties->Set(kImagePathNodeName, new StringValue(image_path));
  image_properties->Set(kImageIndexNodeName,
                        new base::FundamentalValue(image_index));
  images_update->SetWithoutPathExpansion(username, image_properties);
  DVLOG(1) << "Saving path to user image in Local State.";
  local_state->SavePersistentPrefs();
  UserManager::Get()->NotifyLocalStateChanged();
  UserManager* user_manager = UserManager::Get();
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
      Source<UserManager>(user_manager),
      Details<const UserManager::User>(&(user_manager->logged_in_user())));
}

// Saves image to file with specified path. Runs on FILE thread.
// Posts task for saving image info to local state on UI thread.
void SaveImageToFile(const SkBitmap& image,
                     const FilePath& image_path,
                     const std::string& username,
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
      NewRunnableFunction(&SaveImageToLocalState,
                          username, image_path.value(), image_index));
}

// Deletes user's image file. Runs on FILE thread.
void DeleteUserImage(const FilePath& image_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::Delete(image_path, false)) {
    LOG(ERROR) << "Failed to remove user image.";
    return;
  }
}

// Updates current user ownership on UI thread.
void UpdateOwnership(bool is_owner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  g_user_manager.Get().set_current_user_is_owner(is_owner);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_OWNERSHIP_CHECKED,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
  if (is_owner) {
    // Also update cached value.
    UserCrosSettingsProvider::UpdateCachedOwner(
      g_user_manager.Get().logged_in_user().email());
  }
}

// Checks current user's ownership on file thread.
void CheckOwnership() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool is_owner = OwnershipService::GetSharedInstance()->CurrentUserIsOwner();
  VLOG(1) << "Current user " << (is_owner ? "is owner" : "is not owner");

  g_user_manager.Get().set_current_user_is_owner(is_owner);

  // UserManager should be accessed only on UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableFunction(&UpdateOwnership, is_owner));
}

// Used to handle the asynchronous response of deleting a cryptohome directory.
class RemoveAttempt : public CryptohomeLibrary::Delegate {
 public:
  // Creates new remove attempt for the given user. Note, |delegate| can
  // be NULL.
  RemoveAttempt(const std::string& user_email,
                chromeos::RemoveUserDelegate* delegate)
      : user_email_(user_email),
        delegate_(delegate),
        method_factory_(this) {
    RemoveUser();
  }

  virtual ~RemoveAttempt() {}

  void RemoveUser() {
    // Owner is not allowed to be removed from the device.
    // Must not proceed without signature verification.
    UserCrosSettingsProvider user_settings;
    bool trusted_owner_available = user_settings.RequestTrustedOwner(
        method_factory_.NewRunnableMethod(&RemoveAttempt::RemoveUser));
    if (!trusted_owner_available) {
      // Value of owner email is still not verified.
      // Another attempt will be invoked after verification completion.
      return;
    }
    if (user_email_ == UserCrosSettingsProvider::cached_owner()) {
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
    if (CrosLibrary::Get()->EnsureLoaded()) {
      CrosLibrary::Get()->GetCryptohomeLibrary()->AsyncRemove(user_email_,
                                                              this);
    }
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
  ScopedRunnableMethodFactory<RemoveAttempt> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoveAttempt);
};

}  // namespace

UserManager::User::User() : oauth_token_status_(OAUTH_TOKEN_STATUS_UNKNOWN),
                            is_displayname_unique_(false) {
  image_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      kDefaultImageResources[0]);
}

UserManager::User::~User() {}

void UserManager::User::SetImage(const SkBitmap& image,
                                 int default_image_index) {
  image_ = image;
  default_image_index_ = default_image_index;
}

std::string UserManager::User::GetDisplayName() const {
  size_t i = email_.find('@');
  if (i == 0 || i == std::string::npos) {
    return email_;
  }
  return email_.substr(0, i);
}

bool UserManager::User::NeedsNameTooltip() const {
  return !is_displayname_unique_;
}

std::string UserManager::User::GetNameTooltip() const {
  const std::string& user_email = email();
  size_t at_pos = user_email.rfind('@');
  if (at_pos == std::string::npos) {
    NOTREACHED();
    return std::string();
  }
  size_t domain_start = at_pos + 1;
  std::string domain = user_email.substr(domain_start,
                                         user_email.length() - domain_start);
  return base::StringPrintf("%s (%s)",
                            GetDisplayName().c_str(),
                            domain.c_str());
}

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

std::vector<UserManager::User> UserManager::GetUsers() const {
  std::vector<User> users;
  if (!g_browser_process)
    return users;

  PrefService* local_state = g_browser_process->local_state();
  const ListValue* prefs_users = local_state->GetList(kLoggedInUsers);
  const DictionaryValue* prefs_images = local_state->GetDictionary(kUserImages);

  if (prefs_users) {
    std::map<std::string, size_t> display_name_count;

    for (ListValue::const_iterator it = prefs_users->begin();
         it != prefs_users->end();
         ++it) {
      std::string email;
      if ((*it)->GetAsString(&email)) {
        User user;
        user.set_email(email);
        user.set_oauth_token_status(GetOAuthTokenStatusFromLocalState(email));

        if (prefs_images) {
          // Get account image path.
          // TODO(avayvod): Reading image path as a string is here for
          // backward compatibility.
          std::string image_path;
          base::DictionaryValue* image_properties;
          if (prefs_images->GetStringWithoutPathExpansion(email, &image_path)) {
            int default_image_id = kDefaultImagesCount;
            if (IsDefaultImagePath(image_path, &default_image_id)) {
              DCHECK(default_image_id >= 0);
              DCHECK(default_image_id < kDefaultImagesCount);
              int resource_id = kDefaultImageResources[default_image_id];
              user.SetImage(
                  *ResourceBundle::GetSharedInstance().GetBitmapNamed(
                      resource_id),
                  default_image_id);
            } else {
              UserImages::const_iterator image_it = user_images_.find(email);
              if (image_it == user_images_.end()) {
                // Insert the default image so we don't send another request if
                // GetUsers is called twice.
                user_images_[email] = user.image();
                image_loader_->Start(
                    email, image_path, User::kExternalImageIndex, false);
              } else {
                user.SetImage(image_it->second, User::kExternalImageIndex);
              }
            }
          } else if (prefs_images->GetDictionaryWithoutPathExpansion(
                         email, &image_properties)) {
            int image_index = User::kInvalidImageIndex;
            image_properties->GetString(kImagePathNodeName, &image_path);
            image_properties->GetInteger(kImageIndexNodeName, &image_index);
            if (image_index >= 0 && image_index < kDefaultImagesCount) {
              int resource_id = kDefaultImageResources[image_index];
              user.SetImage(
                  *ResourceBundle::GetSharedInstance().GetBitmapNamed(
                      resource_id),
                  image_index);
            } else if (image_index == User::kExternalImageIndex ||
                       image_index == User::kProfileImageIndex) {
              UserImages::const_iterator image_it = user_images_.find(email);
              if (image_it == user_images_.end()) {
                // Insert the default image so we don't send another request if
                // GetUsers is called twice.
                user_images_[email] = user.image();
                image_loader_->Start(email, image_path, image_index, false);
              } else {
                user.SetImage(image_it->second, image_index);
              }
            }
          }
        }

        // Makes table to determine whether displayname is unique.
        const std::string& display_name = user.GetDisplayName();
        ++display_name_count[display_name];

        users.push_back(user);
      }
    }

    for (UserVector::iterator it = users.begin(); it != users.end(); ++it) {
      const std::string& display_name = it->GetDisplayName();
      it->is_displayname_unique_ = display_name_count[display_name] <= 1;
    }
  }
  return users;
}

void UserManager::OffTheRecordUserLoggedIn() {
  user_is_logged_in_ = true;
  logged_in_user_ = User();
  logged_in_user_.set_email(kGuestUser);
  NotifyOnLogin();
}

void UserManager::UserLoggedIn(const std::string& email) {
  if (email == kGuestUser) {
    OffTheRecordUserLoggedIn();
    return;
  }

  if (!IsKnownUser(email)) {
    current_user_is_new_ = true;
    browser_defaults::skip_restore = true;
  }

  // Get a copy of the current users.
  std::vector<User> users = GetUsers();

  // Clear the prefs view of the users.
  PrefService* prefs = g_browser_process->local_state();
  ListPrefUpdate prefs_users_update(prefs, kLoggedInUsers);
  prefs_users_update->Clear();

  user_is_logged_in_ = true;
  logged_in_user_ = User();
  logged_in_user_.set_email(email);

  // Make sure this user is first.
  prefs_users_update->Append(Value::CreateStringValue(email));
  for (std::vector<User>::iterator it = users.begin();
       it != users.end();
       ++it) {
    std::string user_email = it->email();
    // Skip the most recent user.
    if (email != user_email) {
      prefs_users_update->Append(Value::CreateStringValue(user_email));
    } else {
      logged_in_user_ = *it;
    }
  }
  prefs->SavePersistentPrefs();
  NotifyOnLogin();
  if (current_user_is_new_) {
    SetDefaultUserImage(email);
  } else {
    // Download profile image if it's user image and see if it has changed.
    int image_index = logged_in_user_.default_image_index();
    if (image_index == User::kProfileImageIndex) {
      BrowserThread::PostDelayedTask(
          BrowserThread::UI,
          FROM_HERE,
          method_factory_.NewRunnableMethod(
              &UserManager::DownloadProfileImage),
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

void UserManager::RemoveUser(const std::string& email,
                             RemoveUserDelegate* delegate) {
  // Get a copy of the current users.
  std::vector<User> users = GetUsers();

  // Sanity check: we must not remove single user. This check may seem
  // redundant at a first sight because this single user must be an owner and
  // we perform special check later in order not to remove an owner.  However
  // due to non-instant nature of ownership assignment this later check may
  // sometimes fail. See http://crosbug.com/12723
  if (users.size() < 2)
    return;

  bool user_found = false;
  for (size_t i = 0; !user_found && i < users.size(); ++i)
    user_found = (email == users[i].email());
  if (!user_found)
    return;

  // |RemoveAttempt| deletes itself when done.
  new RemoveAttempt(email, delegate);
}

void UserManager::RemoveUserFromList(const std::string& email) {
  // Get a copy of the current users.
  std::vector<User> users = GetUsers();

  // Clear the prefs view of the users.
  PrefService* prefs = g_browser_process->local_state();
  ListPrefUpdate prefs_users_update(prefs, kLoggedInUsers);
  prefs_users_update->Clear();

  for (std::vector<User>::iterator it = users.begin();
       it != users.end();
       ++it) {
    std::string user_email = it->email();
    // Skip user that we would like to delete.
    if (email != user_email)
      prefs_users_update->Append(Value::CreateStringValue(user_email));
  }

  DictionaryPrefUpdate prefs_images_update(prefs, kUserImages);
  std::string image_path_string;
  prefs_images_update->GetStringWithoutPathExpansion(email, &image_path_string);
  prefs_images_update->RemoveWithoutPathExpansion(email, NULL);

  DictionaryPrefUpdate prefs_oauth_update(prefs, kUserOAuthTokenStatus);
  int oauth_status;
  prefs_oauth_update->GetIntegerWithoutPathExpansion(email, &oauth_status);
  prefs_oauth_update->RemoveWithoutPathExpansion(email, NULL);

  prefs->SavePersistentPrefs();

  int default_image_id = kDefaultImagesCount;
  if (!IsDefaultImagePath(image_path_string, &default_image_id)) {
    FilePath image_path(image_path_string);
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        NewRunnableFunction(&DeleteUserImage,
                            image_path));
  }
}

bool UserManager::IsKnownUser(const std::string& email) {
  std::vector<User> users = GetUsers();
  for (std::vector<User>::iterator it = users.begin();
       it < users.end();
       ++it) {
    if (it->email() == email)
      return true;
  }

  return false;
}

const UserManager::User& UserManager::logged_in_user() const {
  return logged_in_user_;
}

void UserManager::SetLoggedInUserImage(const SkBitmap& image,
                                       int default_image_index) {
  profile_image_downloader_.reset();
  if (logged_in_user_.email().empty())
    return;
  SetUserImage(logged_in_user_.email(), image, default_image_index, false);
}

void UserManager::SetUserImage(const std::string& username,
                               const SkBitmap& image,
                               int default_image_index,
                               bool should_save_image) {
  user_images_[username] = image;
  if (logged_in_user_.email() == username)
    logged_in_user_.SetImage(image, default_image_index);
  if (should_save_image)
    SaveUserImage(username, image, default_image_index);
}

void UserManager::LoadLoggedInUserImage(const FilePath& path) {
  profile_image_downloader_.reset();
  if (logged_in_user_.email().empty())
    return;
  image_loader_->Start(
      logged_in_user_.email(), path.value(), User::kExternalImageIndex, true);
}

void UserManager::SaveUserImage(const std::string& username,
                                const SkBitmap& image,
                                int image_index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FilePath image_path = GetImagePathForUser(username);
  DVLOG(1) << "Saving user image to " << image_path.value();

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      NewRunnableFunction(&SaveImageToFile,
                          image, image_path, username, image_index));
}

void UserManager::SaveUserOAuthStatus(const std::string& username,
                                      OAuthTokenStatus oauth_token_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SaveOAuthTokenStatusToLocalState(username, oauth_token_status);
}

UserManager::OAuthTokenStatus UserManager::GetUserOAuthStatus(
    const std::string& username) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return GetOAuthTokenStatusFromLocalState(username);
}

void UserManager::SaveUserImagePath(const std::string& username,
                                    const std::string& image_path,
                                    int image_index) {
  SaveImageToLocalState(username, image_path, image_index);
}

void UserManager::SetDefaultUserImage(const std::string& username) {
  if (!g_browser_process)
    return;

  // Choose a random default image.
  int image_id = base::RandInt(0, kDefaultImagesCount - 1);
  std::string user_image_path = GetDefaultImagePath(image_id);
  int resource_id = kDefaultImageResources[image_id];
  SkBitmap user_image = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      resource_id);

  SaveImageToLocalState(username, user_image_path, image_id);
  SetLoggedInUserImage(user_image, image_id);
}

int UserManager::GetUserDefaultImageIndex(const std::string& username) {
  if (!g_browser_process)
    return User::kInvalidImageIndex;

  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* prefs_images = local_state->GetDictionary(kUserImages);

  if (!prefs_images)
    return User::kInvalidImageIndex;

  std::string image_path;
  if (!prefs_images->GetStringWithoutPathExpansion(username, &image_path))
    return User::kInvalidImageIndex;

  int image_id = kDefaultImagesCount;
  if (!IsDefaultImagePath(image_path, &image_id))
    return User::kInvalidImageIndex;
  return image_id;
}

void UserManager::OnImageLoaded(const std::string& username,
                                const SkBitmap& image,
                                int image_index,
                                bool should_save_image) {
  DVLOG(1) << "Loaded image for " << username;
  SetUserImage(username, image, image_index, should_save_image);
}

void UserManager::OnDownloadSuccess(const SkBitmap& image) {
  VLOG(1) << "Downloaded profile image for logged-in user.";

  std::string current_image_data_url =
      web_ui_util::GetImageDataUrl(logged_in_user_.image());
  std::string new_image_data_url =
      web_ui_util::GetImageDataUrl(image);
  if (current_image_data_url != new_image_data_url) {
    VLOG(1) << "Updating profile image for logged-in user";
    SetLoggedInUserImage(image, User::kProfileImageIndex);
    SaveUserImage(logged_in_user_.email(), image, User::kProfileImageIndex);
  }
}

bool UserManager::IsLoggedInAsGuest() const {
  return logged_in_user().email() == kGuestUser;
}

// Private constructor and destructor. Do nothing.
UserManager::UserManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(image_loader_(new UserImageLoader(this))),
      offline_login_(false),
      current_user_is_owner_(false),
      current_user_is_new_(false),
      user_is_logged_in_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  registrar_.Add(this, chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,
      NotificationService::AllSources());
}

UserManager::~UserManager() {
  image_loader_->set_delegate(NULL);
}

FilePath UserManager::GetImagePathForUser(const std::string& username) {
  std::string filename = username + ".png";
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.AppendASCII(filename);
}

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

void UserManager::NotifyOnLogin() {
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      Source<UserManager>(this),
      Details<const User>(&logged_in_user_));

  chromeos::input_method::InputMethodManager::GetInstance()->
      SetDeferImeStartup(false);
  // Shut down the IME so that it will reload the user's settings.
  chromeos::input_method::InputMethodManager::GetInstance()->
      StopInputMethodDaemon();
  // Let the window manager know that we're logged in now.
  WmIpc::instance()->SetLoggedInProperty(true);
  // Ensure we've opened the real user's key/certificate database.
  crypto::OpenPersistentNSSDB();

  // Only load the Opencryptoki library into NSS if we have this switch.
  // TODO(gspencer): Remove this switch once cryptohomed work is finished:
  // http://crosbug.com/12295 and http://crosbug.com/12304
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLoadOpencryptoki)) {
    crypto::EnableTPMTokenForNSS(new RealTPMTokenInfoDelegate());
  }

  // Schedules current user ownership check on file thread.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&CheckOwnership));
}

void UserManager::Observe(int type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(&CheckOwnership));
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

void UserManager::DownloadProfileImage() {
  profile_image_downloader_.reset(new ProfileImageDownloader(this));
  profile_image_downloader_->Start();
}

}  // namespace chromeos
