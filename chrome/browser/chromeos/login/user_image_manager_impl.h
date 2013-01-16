// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_MANAGER_IMPL_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image_loader.h"
#include "chrome/browser/chromeos/login/user_image_manager.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "ui/gfx/image/image_skia.h"

class ProfileDownloader;
class UserImage;

namespace chromeos {

class UserImageManagerImpl : public UserImageManager,
                             public ProfileDownloaderDelegate {
 public:
  UserImageManagerImpl();

  // UserImageManager implemenation:
  virtual ~UserImageManagerImpl();
  virtual void LoadUserImages(const UserList& users) OVERRIDE;
  virtual void UserLoggedIn(const std::string& email,
                            bool user_is_new,
                            bool user_is_local) OVERRIDE;
  virtual void SaveUserDefaultImageIndex(const std::string& username,
                                         int image_index) OVERRIDE;
  virtual void SaveUserImage(const std::string& username,
                             const UserImage& user_image) OVERRIDE;
  virtual void SaveUserImageFromFile(const std::string& username,
                                     const FilePath& path) OVERRIDE;
  virtual void SaveUserImageFromProfileImage(
      const std::string& username) OVERRIDE;
  virtual void DeleteUserImage(const std::string& username) OVERRIDE;
  virtual void DownloadProfileImage(const std::string& reason) OVERRIDE;
  virtual const gfx::ImageSkia& DownloadedProfileImage() const OVERRIDE;

 private:
  friend class UserImageManagerTest;

  // Non-const for testing purposes.
  static int user_image_migration_delay_sec;

  // ProfileDownloaderDelegate implementation:
  virtual bool NeedsProfilePicture() const OVERRIDE;
  virtual int GetDesiredImageSideLength() const OVERRIDE;
  virtual Profile* GetBrowserProfile() OVERRIDE;
  virtual std::string GetCachedPictureURL() const OVERRIDE;
  virtual void OnProfileDownloadSuccess(ProfileDownloader* downloader) OVERRIDE;
  virtual void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) OVERRIDE;

  // Returns image filepath for the given user.
  FilePath GetImagePathForUser(const std::string& username);

  // Sets one of the default images for the specified user and saves this
  // setting in local state.
  // Does not send LOGIN_USER_IMAGE_CHANGED notification.
  void SetInitialUserImage(const std::string& username);

  // Sets image for user |username| and sends LOGIN_USER_IMAGE_CHANGED
  // notification unless this is a new user and image is set for the first time.
  // If |image| is empty, sets a stub image for the user.
  void SetUserImage(const std::string& username,
                    int image_index,
                    const GURL& image_url,
                    const UserImage& user_image);

  // Saves image to file, updates local state preferences to given image index
  // and sends LOGIN_USER_IMAGE_CHANGED notification.
  void SaveUserImageInternal(const std::string& username,
                             int image_index,
                             const GURL& image_url,
                             const UserImage& user_image);

  // Saves image to file with specified path and sends LOGIN_USER_IMAGE_CHANGED
  // notification. Runs on FILE thread. Posts task for saving image info to
  // Local State on UI thread.
  void SaveImageToFile(const std::string& username,
                       const UserImage& user_image,
                       const FilePath& image_path,
                       int image_index,
                       const GURL& image_url);

  // Stores path to the image and its index in local state. Runs on UI thread.
  // If |is_async| is true, it has been posted from the FILE thread after
  // saving the image.
  void SaveImageToLocalState(const std::string& username,
                             const std::string& image_path,
                             int image_index,
                             const GURL& image_url,
                             bool is_async);

  // Saves |image| to the specified |image_path|. Runs on FILE thread.
  bool SaveBitmapToFile(const UserImage& user_image,
                        const FilePath& image_path);

  // Initializes |downloaded_profile_image_| with the picture of the logged-in
  // user.
  void InitDownloadedProfileImage();

  // Download user's profile data, including full name and picture, when
  // |download_image| is true.
  // |reason| is an arbitrary string (used to report UMA histograms with
  // download times).
  void DownloadProfileData(const std::string& reason, bool download_image);

  // Scheduled call for downloading profile data.
  void DownloadProfileDataScheduled();

  // Delayed call to retry downloading profile data.
  void DownloadProfileDataRetry(bool download_image);

  // Migrates image info for the current user and deletes the old image, if any.
  void MigrateUserImage();

  // Deletes old user image and dictionary entry.
  void DeleteOldUserImage(const std::string& username);

  // Loader for JPEG user images.
  scoped_refptr<UserImageLoader> image_loader_;

  // Unsafe loader instance for all user images formats.
  scoped_refptr<UserImageLoader> unsafe_image_loader_;

  // Download user profile image on login to update it if it's changed.
  scoped_ptr<ProfileDownloader> profile_image_downloader_;

  // Arbitrary string passed to the last |DownloadProfileImage| call.
  std::string profile_image_download_reason_;

  // Time when the profile image download has started.
  base::Time profile_image_load_start_time_;

  // True if the last user image required async save operation (which may not
  // have been completed yet). This flag is used to avoid races when user image
  // is first set with |SaveUserImage| and then with |SaveUserImagePath|.
  bool last_image_set_async_;

  // Result of the last successful profile image download, if any.
  gfx::ImageSkia downloaded_profile_image_;

  // Data URL for |downloaded_profile_image_|.
  std::string downloaded_profile_image_data_url_;

  // Original URL of |downloaded_profile_image_|, from which it was downloaded.
  GURL profile_image_url_;

  // True when |profile_image_downloader_| is fetching profile picture (not
  // just full name).
  bool downloading_profile_image_;

  // Timer triggering DownloadProfileDataScheduled for refreshing profile data.
  base::RepeatingTimer<UserImageManagerImpl> profile_download_timer_;

  // Users that need image migration to JPEG.
  std::set<std::string> users_to_migrate_;

  // If |true|, current user image should be migrated right after it is loaded.
  bool migrate_current_user_on_load_;

  DISALLOW_COPY_AND_ASSIGN(UserImageManagerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_MANAGER_IMPL_H_
