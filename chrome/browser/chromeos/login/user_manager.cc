// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager.h"

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/nss_util.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "content/browser/browser_thread.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"

namespace chromeos {

namespace {

// A vector pref of the users who have logged into the device.
const char kLoggedInUsers[] = "LoggedInUsers";
// A dictionary that maps usernames to file paths to their images.
const char kUserImages[] = "UserImages";

// Incognito user is represented by an empty string (since some code already
// depends on that and it's hard to figure out what).
const char kGuestUser[] = "";

// Special pathes to default user images.
const char* kDefaultImageNames[] = {
    "default:gray",
    "default:green",
    "default:blue",
    "default:yellow",
    "default:red",
};

// Resource IDs of default user images.
const int kDefaultImageResources[] = {
  IDR_LOGIN_DEFAULT_USER,
  IDR_LOGIN_DEFAULT_USER_1,
  IDR_LOGIN_DEFAULT_USER_2,
  IDR_LOGIN_DEFAULT_USER_3,
  IDR_LOGIN_DEFAULT_USER_4
};

// The one true UserManager.
static UserManager* user_manager_ = NULL;

// Stores path to the image in local state. Runs on UI thread.
void SavePathToLocalState(const std::string& username,
                          const std::string& image_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* local_state = g_browser_process->local_state();
  DictionaryValue* images =
      local_state->GetMutableDictionary(kUserImages);
  images->SetWithoutPathExpansion(username, new StringValue(image_path));
  DVLOG(1) << "Saving path to user image in Local State.";
  local_state->SavePersistentPrefs();
}

// Saves image to file with specified path. Runs on FILE thread.
// Posts task for saving image path to local state on UI thread.
void SaveImageToFile(const SkBitmap& image,
                     const FilePath& image_path,
                     const std::string& username) {
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
      NewRunnableFunction(&SavePathToLocalState,
                          username, image_path.value()));
}

// Deletes user's image file. Runs on FILE thread.
void DeleteUserImage(const FilePath& image_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!file_util::Delete(image_path, false)) {
    LOG(ERROR) << "Failed to remove user image.";
    return;
  }
}

// Checks if given path is one of the default ones. If it is, returns true
// and its index in kDefaultImageNames through |image_id|. If not, returns
// false.
bool IsDefaultImagePath(const std::string& path, size_t* image_id) {
  DCHECK(image_id);
  for (size_t i = 0; i < arraysize(kDefaultImageNames); ++i) {
    if (path == kDefaultImageNames[i]) {
      *image_id = i;
      return true;
    }
  }
  return false;
}

// Updates current user ownership on UI thread.
void UpdateOwnership(bool is_owner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UserManager* user_manager = UserManager::Get();
  user_manager->set_current_user_is_owner(is_owner);

  if (is_owner) {
    // Also update cached value.
    UserCrosSettingsProvider::UpdateCachedOwner(
      user_manager->logged_in_user().email());
  }
}

// Checks current user's ownership on file thread.
void CheckOwnership() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool is_owner = OwnershipService::GetSharedInstance()->CurrentUserIsOwner();
  VLOG(1) << "Current user " << (is_owner ? "is owner" : "is not owner");

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

UserManager::User::User() {
  image_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_LOGIN_DEFAULT_USER);
}

UserManager::User::~User() {}

std::string UserManager::User::GetDisplayName() const {
  size_t i = email_.find('@');
  if (i == 0 || i == std::string::npos) {
    return email_;
  }
  return email_.substr(0, i);
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
  if (!user_manager_)
    user_manager_ = new UserManager();
  return user_manager_;
}

// static
void UserManager::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterListPref(kLoggedInUsers);
  local_state->RegisterDictionaryPref(kUserImages);
}

std::vector<UserManager::User> UserManager::GetUsers() const {
  std::vector<User> users;
  if (!g_browser_process)
    return users;

  PrefService* local_state = g_browser_process->local_state();
  const ListValue* prefs_users = local_state->GetList(kLoggedInUsers);
  const DictionaryValue* prefs_images =
      local_state->GetMutableDictionary(kUserImages);

  if (prefs_users) {
    for (ListValue::const_iterator it = prefs_users->begin();
         it != prefs_users->end();
         ++it) {
      std::string email;
      if ((*it)->GetAsString(&email)) {
        User user;
        user.set_email(email);
        UserImages::const_iterator image_it = user_images_.find(email);
        std::string image_path;
        if (image_it == user_images_.end()) {
          if (prefs_images &&
              prefs_images->GetStringWithoutPathExpansion(email, &image_path)) {
            size_t default_image_id = arraysize(kDefaultImageNames);
            if (IsDefaultImagePath(image_path, &default_image_id)) {
              DCHECK(default_image_id < arraysize(kDefaultImageNames));
              int resource_id = kDefaultImageResources[default_image_id];
              user.set_image(
                  *ResourceBundle::GetSharedInstance().GetBitmapNamed(
                      resource_id));
              user_images_[email] = user.image();
            } else {
              // Insert the default image so we don't send another request if
              // GetUsers is called twice.
              user_images_[email] = user.image();
              image_loader_->Start(email, image_path);
            }
          }
        } else {
          user.set_image(image_it->second);
        }
        users.push_back(user);
      }
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
  ListValue* prefs_users = prefs->GetMutableList(kLoggedInUsers);
  prefs_users->Clear();

  user_is_logged_in_ = true;
  logged_in_user_ = User();
  logged_in_user_.set_email(email);

  // Make sure this user is first.
  prefs_users->Append(Value::CreateStringValue(email));
  for (std::vector<User>::iterator it = users.begin();
       it != users.end();
       ++it) {
    std::string user_email = it->email();
    // Skip the most recent user.
    if (email != user_email) {
      prefs_users->Append(Value::CreateStringValue(user_email));
    } else {
      logged_in_user_ = *it;
    }
  }
  prefs->SavePersistentPrefs();
  NotifyOnLogin();
  if (current_user_is_new_)
    SetDefaultUserImage(email);
}

void UserManager::RemoveUser(const std::string& email,
                             RemoveUserDelegate* delegate) {
  // Get a copy of the current users.
  std::vector<User> users = GetUsers();

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
  ListValue* prefs_users = prefs->GetMutableList(kLoggedInUsers);
  DCHECK(prefs_users);
  prefs_users->Clear();

  for (std::vector<User>::iterator it = users.begin();
       it != users.end();
       ++it) {
    std::string user_email = it->email();
    // Skip user that we would like to delete.
    if (email != user_email)
      prefs_users->Append(Value::CreateStringValue(user_email));
  }

  DictionaryValue* prefs_images = prefs->GetMutableDictionary(kUserImages);
  DCHECK(prefs_images);
  std::string image_path_string;
  prefs_images->GetStringWithoutPathExpansion(email, &image_path_string);
  prefs_images->RemoveWithoutPathExpansion(email, NULL);

  prefs->SavePersistentPrefs();

  size_t default_image_id;
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

void UserManager::SetLoggedInUserImage(const SkBitmap& image) {
  if (logged_in_user_.email().empty())
    return;
  logged_in_user_.set_image(image);
  OnImageLoaded(logged_in_user_.email(), image);
}

void UserManager::SaveUserImage(const std::string& username,
                                const SkBitmap& image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FilePath image_path = GetImagePathForUser(username);
  DVLOG(1) << "Saving user image to " << image_path.value();

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      NewRunnableFunction(&SaveImageToFile,
                          image, image_path, username));
}

void UserManager::SetDefaultUserImage(const std::string& username) {
  if (!g_browser_process)
    return;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const ListValue* prefs_users = local_state->GetList(kLoggedInUsers);
  DCHECK(prefs_users);
  const DictionaryValue* prefs_images =
      local_state->GetDictionary(kUserImages);
  DCHECK(prefs_images);

  // We want to distribute default images between users uniformly so that if
  // there're more users with red image, we won't add red one for sure.
  // Thus we count how many default images of each color are used and choose
  // the first color with minimal usage.
  std::vector<int> colors_count(arraysize(kDefaultImageNames), 0);
  for (ListValue::const_iterator it = prefs_users->begin();
       it != prefs_users->end();
       ++it) {
    std::string email;
    if ((*it)->GetAsString(&email)) {
      std::string image_path;
      size_t default_image_id = arraysize(kDefaultImageNames);
      if (prefs_images->GetStringWithoutPathExpansion(email, &image_path) &&
          IsDefaultImagePath(image_path, &default_image_id)) {
        DCHECK(default_image_id < arraysize(kDefaultImageNames));
        ++colors_count[default_image_id];
      }
    }
  }
  std::vector<int>::const_iterator min_it =
      std::min_element(colors_count.begin(), colors_count.end());
  int selected_id = min_it - colors_count.begin();
  std::string user_image_path = kDefaultImageNames[selected_id];
  int resource_id = kDefaultImageResources[selected_id];
  SkBitmap user_image = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      resource_id);

  SavePathToLocalState(username, user_image_path);
  SetLoggedInUserImage(user_image);
}

void UserManager::OnImageLoaded(const std::string& username,
                                const SkBitmap& image) {
  DVLOG(1) << "Loaded image for " << username;
  user_images_[username] = image;
  User user;
  user.set_email(username);
  user.set_image(image);
  NotificationService::current()->Notify(
      NotificationType::LOGIN_USER_IMAGE_CHANGED,
      Source<UserManager>(this),
      Details<const User>(&user));
}

bool UserManager::IsLoggedInAsGuest() const {
  return logged_in_user().email() == kGuestUser;
}

// Private constructor and destructor. Do nothing.
UserManager::UserManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(image_loader_(new UserImageLoader(this))),
      current_user_is_owner_(false),
      current_user_is_new_(false),
      user_is_logged_in_(false) {
  registrar_.Add(this, NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED,
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

void UserManager::NotifyOnLogin() {
  NotificationService::current()->Notify(
      NotificationType::LOGIN_USER_CHANGED,
      Source<UserManager>(this),
      Details<const User>(&logged_in_user_));

  chromeos::CrosLibrary::Get()->GetInputMethodLibrary()->
      SetDeferImeStartup(false);
  // Shut down the IME so that it will reload the user's settings.
  chromeos::CrosLibrary::Get()->GetInputMethodLibrary()->
      StopInputMethodDaemon();
  // Let the window manager know that we're logged in now.
  WmIpc::instance()->SetLoggedInProperty(true);
  // Ensure we've opened the real user's key/certificate database.
  base::OpenPersistentNSSDB();

  // Schedules current user ownership check on file thread.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&CheckOwnership));
}

void UserManager::Observe(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (type == NotificationType::OWNER_KEY_FETCH_ATTEMPT_SUCCEEDED) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(&CheckOwnership));
  }
}

bool UserManager::current_user_is_owner() const {
  return current_user_is_owner_;
}

void UserManager::set_current_user_is_owner(bool current_user_is_owner) {
  current_user_is_owner_ = current_user_is_owner;
}

}  // namespace chromeos
