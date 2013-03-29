// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

const char kTestUser1[] = "test-user@example.com";
const char kTestUser2[] = "test-user2@example.com";

class UserImageManagerTest : public CrosInProcessBrowserTest,
                             public content::NotificationObserver,
                             public UserManager::Observer {
 protected:
  UserImageManagerTest() {
  }

  // CrosInProcessBrowserTest overrides:
  virtual void SetUpOnMainThread() OVERRIDE {
    UserManager::Get()->AddObserver(this);
    user_image_manager_ = UserManager::Get()->GetUserImageManager();
    local_state_ = g_browser_process->local_state();
    // No migration delay for testing.
    UserImageManagerImpl::user_image_migration_delay_sec = 0;
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
  }

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK(type == chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED);
    registrar_.Remove(this, chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                      content::NotificationService::AllSources());
    MessageLoopForUI::current()->Quit();
  }

  // UserManager::Observer overrides:
  virtual void LocalStateChanged(UserManager* user_manager) OVERRIDE {
    MessageLoopForUI::current()->Quit();
  }

  // Adds given user to Local State, if not there.
  void AddUser(const std::string& username) {
    ListPrefUpdate users_pref(local_state_, "LoggedInUsers");
    users_pref->AppendIfNotPresent(new base::StringValue(username));
  }

  // Logs in |username|.
  void LogIn(const std::string& username) {
    UserManager::Get()->UserLoggedIn(username, username, false);
  }

  // Subscribes for image change notification.
  void ExpectImageChange() {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                   content::NotificationService::AllSources());
  }

  // Stores old (pre-migration) user image info.
  void SetOldUserImageInfo(const std::string& username,
                           int image_index,
                           const base::FilePath& image_path) {
    AddUser(username);
    DictionaryPrefUpdate images_pref(local_state_, "UserImages");
    base::DictionaryValue* image_properties = new base::DictionaryValue();
    image_properties->Set(
        "index", base::Value::CreateIntegerValue(image_index));
    image_properties->Set(
        "path" , new base::StringValue(image_path.value()));
    images_pref->SetWithoutPathExpansion(username, image_properties);
  }

  // Verifies user image info in |images_pref| dictionary.
  void ExpectUserImageInfo(const base::DictionaryValue* images_pref,
                           const std::string& username,
                           int image_index,
                           const base::FilePath& image_path) {
    ASSERT_TRUE(images_pref);
    const base::DictionaryValue* image_properties = NULL;
    images_pref->GetDictionaryWithoutPathExpansion(username, &image_properties);
    ASSERT_TRUE(image_properties);
    int actual_image_index;
    std::string actual_image_path;
    ASSERT_TRUE(image_properties->GetInteger("index", &actual_image_index) &&
                image_properties->GetString("path", &actual_image_path));
    EXPECT_EQ(image_index, actual_image_index);
    EXPECT_EQ(image_path.value(), actual_image_path);
  }

  // Verifies that there is no image info for |username| in dictionary
  // |images_pref|.
  void ExpectNoUserImageInfo(const base::DictionaryValue* images_pref,
                             const std::string& username) {
    ASSERT_TRUE(images_pref);
    const base::DictionaryValue* image_properties = NULL;
    images_pref->GetDictionaryWithoutPathExpansion(username, &image_properties);
    ASSERT_FALSE(image_properties);
  }

  // Verifies that old user image info matches |image_index| and |image_path|
  // and that new user image info does not exist.
  void ExpectOldUserImageInfo(const std::string& username,
                              int image_index,
                              const base::FilePath& image_path) {
    ExpectUserImageInfo(local_state_->GetDictionary("UserImages"),
                        username, image_index, image_path);
    ExpectNoUserImageInfo(local_state_->GetDictionary("user_image_info"),
                          username);
  }

  // Verifies that new user image info matches |image_index| and |image_path|
  // and that old user image info does not exist.
  void ExpectNewUserImageInfo(const std::string& username,
                              int image_index,
                              const base::FilePath& image_path) {
    ExpectUserImageInfo(local_state_->GetDictionary("user_image_info"),
                        username, image_index, image_path);
    ExpectNoUserImageInfo(local_state_->GetDictionary("UserImages"),
                          username);
  }

  // Sets bitmap |resource_id| as image for |username| and saves it to disk.
  void SaveUserImagePNG(const std::string& username,
                        int resource_id) {
    base::FilePath image_path = GetUserImagePath(username, "png");
    scoped_refptr<base::RefCountedStaticMemory> image_data(
        ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
            resource_id, ui::SCALE_FACTOR_100P));
    int written = file_util::WriteFile(
        image_path,
        reinterpret_cast<const char*>(image_data->front()),
        image_data->size());
    EXPECT_EQ(static_cast<int>(image_data->size()), written);
    SetOldUserImageInfo(username, User::kExternalImageIndex, image_path);
  }

  // Returns the image path for user |username| with specified |extension|.
  base::FilePath GetUserImagePath(const std::string& username,
                            const std::string& extension) {
    base::FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    return user_data_dir.Append(username).AddExtension(extension);
  }

  UserImageManager* user_image_manager_;
  PrefService* local_state_;
  content::NotificationRegistrar registrar_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserImageManagerTest);
};

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_DefaultUserImagePreserved) {
  // Setup an old default (stock) user image.
  ScopedMockUserManagerEnabler mock_user_manager;
  SetOldUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, DefaultUserImagePreserved) {
  UserManager::Get()->GetUsers();  // Load users.
  // Old info preserved.
  ExpectOldUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
  LogIn(kTestUser1);
  // Wait for migration.
  content::RunMessageLoop();
  // Image info is migrated now.
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_OtherUsersUnaffected) {
  // Setup two users with stock images.
  ScopedMockUserManagerEnabler mock_user_manager;
  SetOldUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
  SetOldUserImageInfo(kTestUser2, kFirstDefaultImageIndex + 1,
                      base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, OtherUsersUnaffected) {
  UserManager::Get()->GetUsers();  // Load users.
  // Old info preserved.
  ExpectOldUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
  ExpectOldUserImageInfo(kTestUser2, kFirstDefaultImageIndex + 1,
                         base::FilePath());
  LogIn(kTestUser1);
  // Wait for migration.
  content::RunMessageLoop();
  // Image info is migrated for the first user and unaffected for the rest.
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
  ExpectOldUserImageInfo(kTestUser2, kFirstDefaultImageIndex + 1,
                         base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_PRE_NonJPEGImageFromFile) {
  // Setup a user with non-JPEG image.
  ScopedMockUserManagerEnabler mock_user_manager;
  SaveUserImagePNG(
      kTestUser1, kDefaultImageResourceIDs[kFirstDefaultImageIndex]);
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_NonJPEGImageFromFile) {
  UserManager::Get()->GetUsers();  // Load users.
  // Old info preserved.
  ExpectOldUserImageInfo(kTestUser1, User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "png"));
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  EXPECT_TRUE(user->image_is_stub());
  LogIn(kTestUser1);
  // Wait for migration.
  content::RunMessageLoop();
  // Image info is migrated and the image is converted to JPG.
  ExpectNewUserImageInfo(kTestUser1, User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));
  user = UserManager::Get()->GetLoggedInUser();
  ASSERT_TRUE(user);
  EXPECT_FALSE(user->image_is_safe_format());
  // Check image dimensions.
  const gfx::ImageSkia& saved_image = GetDefaultImage(kFirstDefaultImageIndex);
  EXPECT_EQ(saved_image.width(), user->image().width());
  EXPECT_EQ(saved_image.height(), user->image().height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, NonJPEGImageFromFile) {
  ExpectImageChange();
  UserManager::Get()->GetUsers();  // Load users.
  // Wait for image load.
  content::RunMessageLoop();
  // Now the migrated image is used.
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);
  EXPECT_TRUE(user->image_is_safe_format());
  // Check image dimensions. Images can't be compared since JPEG is lossy.
  const gfx::ImageSkia& saved_image = GetDefaultImage(kFirstDefaultImageIndex);
  EXPECT_EQ(saved_image.width(), user->image().width());
  EXPECT_EQ(saved_image.height(), user->image().height());
}

}  // namespace chromeos
