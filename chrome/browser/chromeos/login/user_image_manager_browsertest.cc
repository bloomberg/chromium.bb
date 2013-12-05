// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "chrome/browser/chromeos/login/user_image_manager_impl.h"
#include "chrome/browser/chromeos/login/user_image_manager_test_util.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

namespace {

const char kTestUser1[] = "test-user@example.com";
const char kTestUser2[] = "test-user2@example.com";

}  // namespace

class UserImageManagerTest : public LoginManagerTest,
                             public UserManager::Observer {
 protected:
  UserImageManagerTest() : LoginManagerTest(true) {
  }

  // LoginManagerTest overrides:
  virtual void SetUpOnMainThread() OVERRIDE {
    LoginManagerTest::SetUpOnMainThread();
    local_state_ = g_browser_process->local_state();
    UserManager::Get()->AddObserver(this);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    UserManager::Get()->RemoveObserver(this);
    LoginManagerTest::TearDownOnMainThread();
  }

  // UserManager::Observer overrides:
  virtual void LocalStateChanged(UserManager* user_manager) OVERRIDE {
    if (run_loop_)
      run_loop_->Quit();
  }

  // Logs in |username|.
  void LogIn(const std::string& username) {
    UserManager::Get()->UserLoggedIn(username, username, false);
  }

  // Stores old (pre-migration) user image info.
  void SetOldUserImageInfo(const std::string& username,
                           int image_index,
                           const base::FilePath& image_path) {
    RegisterUser(username);
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

  // Returns the path to a test image that can be set as the user image.
  base::FilePath GetTestImagePath() {
    base::FilePath test_data_dir;
    if (!PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir)) {
      ADD_FAILURE();
      return base::FilePath();
    }
    return test_data_dir.Append("chromeos").Append("avatar1.jpg");
  }

  // Completes the download of all non-image profile data for the currently
  // logged-in user. This method must only be called after a profile data
  // download has been started.
  // |url_fetcher_factory| will capture the net::TestURLFetcher created by the
  // ProfileDownloader to download the profile image.
  void CompleteProfileMetadataDownload(
      net::TestURLFetcherFactory* url_fetcher_factory) {
    ProfileDownloader* profile_downloader =
        reinterpret_cast<UserImageManagerImpl*>(
            UserManager::Get()->GetUserImageManager())->
                profile_downloader_.get();
    ASSERT_TRUE(profile_downloader);

    static_cast<OAuth2TokenService::Consumer*>(profile_downloader)->
        OnGetTokenSuccess(NULL,
                          std::string(),
                          base::Time::Now() + base::TimeDelta::FromDays(1));

    net::TestURLFetcher* fetcher = url_fetcher_factory->GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    fetcher->SetResponseString(
        "{ \"picture\": \"http://localhost/avatar.jpg\" }");
    fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                              net::OK));
    fetcher->set_response_code(200);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    base::RunLoop().RunUntilIdle();
  }

  // Completes the download of the currently logged-in user's profile image.
  // This method must only be called after a profile data download including
  // the profile image has been started, the download of all non-image data has
  // been completed by calling CompleteProfileMetadataDownload() and the
  // net::TestURLFetcher created by the ProfileDownloader to download the
  // profile image has been captured by |url_fetcher_factory|.
  void CompleteProfileImageDownload(
      net::TestURLFetcherFactory* url_fetcher_factory) {
    std::string profile_image_data;
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    EXPECT_TRUE(ReadFileToString(
        test_data_dir.Append("chromeos").Append("avatar1.jpg"),
        &profile_image_data));

    base::RunLoop run_loop;
    PrefChangeRegistrar pref_change_registrar;
    pref_change_registrar.Init(local_state_);
    pref_change_registrar.Add("UserDisplayName", run_loop.QuitClosure());
    net::TestURLFetcher* fetcher = url_fetcher_factory->GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    fetcher->SetResponseString(profile_image_data);
    fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS,
                                              net::OK));
    fetcher->set_response_code(200);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    run_loop.Run();

    const User* user = UserManager::Get()->GetLoggedInUser();
    ASSERT_TRUE(user);
    const std::map<std::string, linked_ptr<UserImageManagerImpl::Job> >&
        job_map = reinterpret_cast<UserImageManagerImpl*>(
            UserManager::Get()->GetUserImageManager())->jobs_;
    if (job_map.find(user->email()) != job_map.end()) {
      run_loop_.reset(new base::RunLoop);
      run_loop_->Run();
    }
  }

  PrefService* local_state_;

  scoped_ptr<gfx::ImageSkia> decoded_image_;

  scoped_ptr<base::RunLoop> run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserImageManagerTest);
};

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_DefaultUserImagePreserved) {
  // Setup an old default (stock) user image.
  ScopedUserManagerEnabler(new MockUserManager);
  SetOldUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, DefaultUserImagePreserved) {
  UserManager::Get()->GetUsers();  // Load users.
  // Old info preserved.
  ExpectOldUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
  LogIn(kTestUser1);
  // Image info is migrated now.
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_OtherUsersUnaffected) {
  // Setup two users with stock images.
  ScopedUserManagerEnabler(new MockUserManager);
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
  // Image info is migrated for the first user and unaffected for the rest.
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
  ExpectOldUserImageInfo(kTestUser2, kFirstDefaultImageIndex + 1,
                         base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_PRE_NonJPEGImageFromFile) {
  // Setup a user with non-JPEG image.
  ScopedUserManagerEnabler(new MockUserManager);
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

  base::RunLoop run_loop;
  PrefChangeRegistrar pref_change_registrar_;
  pref_change_registrar_.Init(local_state_);
  pref_change_registrar_.Add("UserImages", run_loop.QuitClosure());
  LogIn(kTestUser1);

  // Wait for migration.
  run_loop.Run();

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
  UserManager::Get()->GetUsers();  // Load users.
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);
  // Wait for image load.
  if (user->image_index() == User::kInvalidImageIndex) {
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
        content::NotificationService::AllSources()).Wait();
  }
  // Now the migrated image is used.
  EXPECT_TRUE(user->image_is_safe_format());
  // Check image dimensions. Images can't be compared since JPEG is lossy.
  const gfx::ImageSkia& saved_image = GetDefaultImage(kFirstDefaultImageIndex);
  EXPECT_EQ(saved_image.width(), user->image().width());
  EXPECT_EQ(saved_image.height(), user->image().height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveUserDefaultImageIndex) {
  RegisterUser(kTestUser1);
}

// Verifies that SaveUserDefaultImageIndex() correctly sets and persists the
// chosen user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserDefaultImageIndex) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  const gfx::ImageSkia& default_image =
      GetDefaultImage(kFirstDefaultImageIndex);

  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserDefaultImageIndex(kTestUser1,
                                                kFirstDefaultImageIndex);

  EXPECT_TRUE(user->HasDefaultImage());
  EXPECT_EQ(kFirstDefaultImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(default_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveUserImage) {
  RegisterUser(kTestUser1);
}

// Verifies that SaveUserImage() correctly sets and persists the chosen user
// image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImage) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  SkBitmap custom_image_bitmap;
  custom_image_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
  custom_image_bitmap.allocPixels();
  custom_image_bitmap.setImmutable();
  const gfx::ImageSkia custom_image =
      gfx::ImageSkia::CreateFrom1xBitmap(custom_image_bitmap);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserImage(kTestUser1,
                                    UserImage::CreateAndEncode(custom_image));
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kExternalImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(custom_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  const scoped_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(custom_image.width(), saved_image->width());
  EXPECT_EQ(custom_image.height(), saved_image->height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest, PRE_SaveUserImageFromFile) {
  RegisterUser(kTestUser1);
}

// Verifies that SaveUserImageFromFile() correctly sets and persists the chosen
// user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImageFromFile) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  const base::FilePath custom_image_path = GetTestImagePath();
  const scoped_ptr<gfx::ImageSkia> custom_image =
      test::ImageLoader(custom_image_path).Load();
  ASSERT_TRUE(custom_image);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserImageFromFile(kTestUser1, custom_image_path);
  run_loop_->Run();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kExternalImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(*custom_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kExternalImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  const scoped_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(custom_image->width(), saved_image->width());
  EXPECT_EQ(custom_image->height(), saved_image->height());
}

IN_PROC_BROWSER_TEST_F(UserImageManagerTest,
                       PRE_SaveUserImageFromProfileImage) {
  RegisterUser(kTestUser1);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Verifies that SaveUserImageFromProfileImage() correctly downloads, sets and
// persists the chosen user image.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest, SaveUserImageFromProfileImage) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  UserImageManagerImpl::IgnoreProfileDataDownloadDelayForTesting();
  LoginUser(kTestUser1);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserImageFromProfileImage(kTestUser1);
  run_loop_->Run();

  net::TestURLFetcherFactory url_fetcher_factory;
  CompleteProfileMetadataDownload(&url_fetcher_factory);
  CompleteProfileImageDownload(&url_fetcher_factory);

  const gfx::ImageSkia& profile_image =
      user_image_manager->DownloadedProfileImage();

  EXPECT_FALSE(user->HasDefaultImage());
  EXPECT_EQ(User::kProfileImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(profile_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1,
                         User::kProfileImageIndex,
                         GetUserImagePath(kTestUser1, "jpg"));

  const scoped_ptr<gfx::ImageSkia> saved_image =
      test::ImageLoader(GetUserImagePath(kTestUser1, "jpg")).Load();
  ASSERT_TRUE(saved_image);

  // Check image dimensions. Images can't be compared since JPEG is lossy.
  EXPECT_EQ(profile_image.width(), saved_image->width());
  EXPECT_EQ(profile_image.height(), saved_image->height());
}


IN_PROC_BROWSER_TEST_F(UserImageManagerTest,
                       PRE_ProfileImageDownloadDoesNotClobber) {
  RegisterUser(kTestUser1);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Sets the user image to the profile image, then sets it to one of the default
// images while the profile image download is still in progress. Verifies that
// when the download completes, the profile image is ignored and does not
// clobber the default image chosen in the meantime.
IN_PROC_BROWSER_TEST_F(UserImageManagerTest,
                       ProfileImageDownloadDoesNotClobber) {
  const User* user = UserManager::Get()->FindUser(kTestUser1);
  ASSERT_TRUE(user);

  const gfx::ImageSkia& default_image =
      GetDefaultImage(kFirstDefaultImageIndex);

  UserImageManagerImpl::IgnoreProfileDataDownloadDelayForTesting();
  LoginUser(kTestUser1);

  run_loop_.reset(new base::RunLoop);
  UserImageManager* user_image_manager =
      UserManager::Get()->GetUserImageManager();
  user_image_manager->SaveUserImageFromProfileImage(kTestUser1);
  run_loop_->Run();

  net::TestURLFetcherFactory url_fetcher_factory;
  CompleteProfileMetadataDownload(&url_fetcher_factory);

  user_image_manager->SaveUserDefaultImageIndex(kTestUser1,
                                                kFirstDefaultImageIndex);

  CompleteProfileImageDownload(&url_fetcher_factory);

  EXPECT_TRUE(user->HasDefaultImage());
  EXPECT_EQ(kFirstDefaultImageIndex, user->image_index());
  EXPECT_TRUE(test::AreImagesEqual(default_image, user->image()));
  ExpectNewUserImageInfo(kTestUser1, kFirstDefaultImageIndex, base::FilePath());
}

}  // namespace chromeos
