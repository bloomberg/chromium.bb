// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_manager.h"

#include "app/resource_bundle.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "gfx/codec/png_codec.h"
#include "grit/theme_resources.h"

namespace chromeos {

namespace {

// A vector pref of the users who have logged into the device.
const wchar_t kLoggedInUsers[] = L"LoggedInUsers";
// A dictionary that maps usernames to file paths to their images.
const wchar_t kUserImages[] = L"UserImages";

// The one true UserManager.
static UserManager* user_manager_ = NULL;

// Stores path to the image in local state. Runs on UI thread.
void save_path_to_local_state(const std::string& username,
                              const std::string& image_path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PrefService* local_state = g_browser_process->local_state();
  DictionaryValue* images =
      local_state->GetMutableDictionary(kUserImages);
  images->SetWithoutPathExpansion(UTF8ToWide(username),
                                  new StringValue(image_path));
  LOG(INFO) << "Saving path to user image in Local State.";
  local_state->SavePersistentPrefs();
}

// Saves image to file with specified path. Runs on FILE thread.
// Posts task for saving image path to local state on UI thread.
void save_image_to_file(const SkBitmap& image,
                        const FilePath& image_path,
                        const std::string& username) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
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

  ChromeThread::PostTask(
      ChromeThread::UI,
      FROM_HERE,
      NewRunnableFunction(&save_path_to_local_state,
                          username, image_path.value()));
}

}  // namespace

UserManager::User::User() {
  image_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_LOGIN_DEFAULT_USER);
}

std::string UserManager::User::GetDisplayName() const {
  size_t i = email_.find('@');
  if (i == 0 || i == std::string::npos) {
    return email_;
  }
  return email_.substr(0, i);
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
      local_state->GetDictionary(kUserImages);

  if (prefs_users) {
    for (ListValue::const_iterator it = prefs_users->begin();
         it < prefs_users->end();
         ++it) {
      std::string email;
      if ((*it)->GetAsString(&email)) {
        User user;
        user.set_email(email);
        UserImages::const_iterator image_it = user_images_.find(email);
        std::string image_path;
        if (image_it == user_images_.end()) {
          if (prefs_images &&
              prefs_images->GetStringWithoutPathExpansion(
                  ASCIIToWide(email), &image_path)) {
            // Insert the default image so we don't send another request if
            // GetUsers is called twice.
            user_images_[email] = user.image();
            image_loader_->Start(email, image_path);
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
  NotifyOnLogin();
}

void UserManager::UserLoggedIn(const std::string& email) {
  // Get a copy of the current users.
  std::vector<User> users = GetUsers();

  // Clear the prefs view of the users.
  PrefService* prefs = g_browser_process->local_state();
  ListValue* prefs_users = prefs->GetMutableList(kLoggedInUsers);
  prefs_users->Clear();

  logged_in_user_.set_email(email);

  // Make sure this user is first.
  prefs_users->Append(Value::CreateStringValue(email));
  for (std::vector<User>::iterator it = users.begin();
       it < users.end();
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
}

void UserManager::RemoveUser(const std::string& email) {
  // Get a copy of the current users.
  std::vector<User> users = GetUsers();

  // Clear the prefs view of the users.
  PrefService* prefs = g_browser_process->local_state();
  ListValue* prefs_users = prefs->GetMutableList(kLoggedInUsers);
  prefs_users->Clear();

  for (std::vector<User>::iterator it = users.begin();
       it < users.end();
       ++it) {
    std::string user_email = it->email();
    // Skip user that we would like to delete.
    if (email != user_email)
      prefs_users->Append(Value::CreateStringValue(user_email));
  }
  prefs->SavePersistentPrefs();
}

void UserManager::SaveUserImage(const std::string& username,
                                const SkBitmap& image) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  std::string filename = username + ".png";
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  FilePath image_path = user_data_dir.AppendASCII(filename);
  LOG(INFO) << "Saving user image to " << image_path.value();

  ChromeThread::PostTask(
      ChromeThread::FILE,
      FROM_HERE,
      NewRunnableFunction(&save_image_to_file,
                          image, image_path, username));
}

void UserManager::OnImageLoaded(const std::string& username,
                                const SkBitmap& image) {
  LOG(INFO) << "Loaded image for " << username;
  user_images_[username] = image;
  User user;
  user.set_email(username);
  user.set_image(image);
  NotificationService::current()->Notify(
      NotificationType::LOGIN_USER_IMAGE_CHANGED,
      Source<UserManager>(this),
      Details<const User>(&user));
}

// Private constructor and destructor. Do nothing.
UserManager::UserManager()
    : ALLOW_THIS_IN_INITIALIZER_LIST(image_loader_(new UserImageLoader(this))) {
}

UserManager::~UserManager() {
  image_loader_->set_delegate(NULL);
}

void UserManager::NotifyOnLogin() {
  NotificationService::current()->Notify(
      NotificationType::LOGIN_USER_CHANGED,
      Source<UserManager>(this),
      Details<const User>(&logged_in_user_));

  // Let the window manager know that we're logged in now.
  WmIpc::instance()->SetLoggedInProperty(true);
}

}  // namespace chromeos
