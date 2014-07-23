// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_IMPL_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "components/user_manager/user.h"
#include "ui/gfx/image/image_skia.h"

class ProfileDownloader;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace user_manager {
class UserImage;
}

namespace chromeos {

class UserImageSyncObserver;
class UserManager;

class UserImageManagerImpl
    : public UserImageManager,
      public ProfileDownloaderDelegate {
 public:
  // UserImageManager:
  UserImageManagerImpl(const std::string& user_id,
                       UserManager* user_manager);
  virtual ~UserImageManagerImpl();

  virtual void LoadUserImage() OVERRIDE;
  virtual void UserLoggedIn(bool user_is_new, bool user_is_local) OVERRIDE;
  virtual void SaveUserDefaultImageIndex(int default_image_index) OVERRIDE;
  virtual void SaveUserImage(
      const user_manager::UserImage& user_image) OVERRIDE;
  virtual void SaveUserImageFromFile(const base::FilePath& path) OVERRIDE;
  virtual void SaveUserImageFromProfileImage() OVERRIDE;
  virtual void DeleteUserImage() OVERRIDE;
  virtual void DownloadProfileImage(const std::string& reason) OVERRIDE;
  virtual const gfx::ImageSkia& DownloadedProfileImage() const OVERRIDE;
  virtual UserImageSyncObserver* GetSyncObserver() const OVERRIDE;
  virtual void Shutdown() OVERRIDE;

  virtual void OnExternalDataSet(const std::string& policy) OVERRIDE;
  virtual void OnExternalDataCleared(const std::string& policy) OVERRIDE;
  virtual void OnExternalDataFetched(const std::string& policy,
                                     scoped_ptr<std::string> data) OVERRIDE;

  static void IgnoreProfileDataDownloadDelayForTesting();

 private:
  friend class UserImageManagerTest;

  // Every image load or update is encapsulated by a Job. Whenever an image load
  // or update is requested for a user, the Job currently running for that user
  // (if any) is canceled. This ensures that at most one Job is running per user
  // at any given time. There are two further guarantees:
  //
  // * Changes to User objects and local state are performed on the thread that
  //   |this| runs on.
  // * File writes and deletions are performed via |background_task_runner_|.
  //
  // With the above, it is guaranteed that any changes made by a canceled Job
  // cannot race against against changes made by the superseding Job.
  class Job;

  // ProfileDownloaderDelegate:
  virtual bool NeedsProfilePicture() const OVERRIDE;
  virtual int GetDesiredImageSideLength() const OVERRIDE;
  virtual Profile* GetBrowserProfile() OVERRIDE;
  virtual std::string GetCachedPictureURL() const OVERRIDE;
  virtual void OnProfileDownloadSuccess(ProfileDownloader* downloader) OVERRIDE;
  virtual void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) OVERRIDE;

  // Returns true if the user image for the user is managed by
  // policy and the user is not allowed to change it.
  bool IsUserImageManaged() const;

  // Randomly chooses one of the default images for the specified user, sends a
  // LOGIN_USER_IMAGE_CHANGED notification and updates local state.
  void SetInitialUserImage();

  // Initializes the |downloaded_profile_image_| for the currently logged-in
  // user to a profile image that had been downloaded and saved before if such
  // a saved image is available and no updated image has been downloaded yet.
  void TryToInitDownloadedProfileImage();

  // Returns true if the profile image needs to be downloaded. This is the case
  // when a GAIA user is logged in and at least one of the following applies:
  // * The profile image has explicitly been requested by a call to
  //   DownloadProfileImage() and has not been successfully downloaded since.
  // * The user's user image is the profile image.
  bool NeedProfileImage() const;

  // Downloads the profile data for the currently logged-in user. The user's
  // full name and, if NeedProfileImage() is true, the profile image are
  // downloaded. |reason| is an arbitrary string (used to report UMA histograms
  // with download times).
  void DownloadProfileData(const std::string& reason);

  // Removes ther user from the dictionary |prefs_dict_root| in
  // local state and deletes the image file that the dictionary
  // referenced for that user.
  void DeleteUserImageAndLocalStateEntry(const char* prefs_dict_root);

  // Called when a Job updates the copy of the user image held in
  // memory.  Allows |this| to update |downloaded_profile_image_| and
  // send a NOTIFICATION_LOGIN_USER_IMAGE_CHANGED notification.
  void OnJobChangedUserImage();

  // Called when a Job for the user finishes. If a migration was
  // required for the user, the migration is now complete and the old
  // image file for that user, if any, is deleted.
  void OnJobDone();

  // Completes migration by removing the user from the old prefs
  // dictionary.
  void UpdateLocalStateAfterMigration();

  // Create a sync observer if a user is logged in, the user's user image is
  // allowed to be synced and no sync observer exists yet.
  void TryToCreateImageSyncObserver();

  // Returns immutable version of user with |user_id_|.
  const user_manager::User* GetUser() const;

  // Returns mutable version of user with |user_id_|.
  user_manager::User* GetUserAndModify() const;

  // Returns true if user with |user_id_| is logged in and a regular user.
  bool IsUserLoggedInAndRegular() const;

  // The user manager.
  UserManager* user_manager_;

  // Loader for JPEG user images.
  scoped_refptr<UserImageLoader> image_loader_;

  // Unsafe loader instance for all user images formats.
  scoped_refptr<UserImageLoader> unsafe_image_loader_;

  // Whether the |profile_downloader_| is downloading the profile image for the
  // currently logged-in user (and not just the full name). Only valid when a
  // download is currently in progress.
  bool downloading_profile_image_;

  // Download reason given to DownloadProfileImage(), used for UMA histograms.
  // Only valid when a download is currently in progress and
  // |downloading_profile_image_| is true.
  std::string profile_image_download_reason_;

  // Time when the profile image download started. Only valid when a download is
  // currently in progress and |downloading_profile_image_| is true.
  base::TimeTicks profile_image_load_start_time_;

  // Downloader for the user's profile data. NULL when no download is
  // currently in progress.
  scoped_ptr<ProfileDownloader> profile_downloader_;

  // The currently logged-in user's downloaded profile image, if successfully
  // downloaded or initialized from a previously downloaded and saved image.
  gfx::ImageSkia downloaded_profile_image_;

  // Data URL corresponding to |downloaded_profile_image_|. Empty if no
  // |downloaded_profile_image_| is currently available.
  std::string downloaded_profile_image_data_url_;

  // URL from which |downloaded_profile_image_| was downloaded. Empty if no
  // |downloaded_profile_image_| is currently available.
  GURL profile_image_url_;

  // Whether a download of the currently logged-in user's profile image has been
  // explicitly requested by a call to DownloadProfileImage() and has not been
  // satisfied by a successful download yet.
  bool profile_image_requested_;

  // Timer used to start a profile data download shortly after login and to
  // restart the download after network errors.
  base::OneShotTimer<UserImageManagerImpl> profile_download_one_shot_timer_;

  // Timer used to periodically start a profile data, ensuring the profile data
  // stays up to date.
  base::RepeatingTimer<UserImageManagerImpl> profile_download_periodic_timer_;

  // Sync observer for the currently logged-in user.
  scoped_ptr<UserImageSyncObserver> user_image_sync_observer_;

  // Background task runner on which Jobs perform file I/O and the image
  // decoders run.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // The currently running job.
  scoped_ptr<Job> job_;

  bool has_managed_image_;
  bool user_needs_migration_;

  base::WeakPtrFactory<UserImageManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserImageManagerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_IMPL_H_
