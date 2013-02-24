// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_manager_impl.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_constants.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/webui/web_ui_util.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// A dictionary that maps usernames to old user image data with images stored in
// PNG format. Deprecated.
// TODO(ivankr): remove this const char after migration is gone.
const char kUserImages[] = "UserImages";

// A dictionary that maps usernames to user image data with images stored in
// JPEG format.
const char kUserImageProperties[] = "user_image_info";

// Names of user image properties.
const char kImagePathNodeName[] = "path";
const char kImageIndexNodeName[] = "index";
const char kImageURLNodeName[] = "url";

// Delay betweeen user login and user image migration.
const int kUserImageMigrationDelaySec = 50;

// Delay betweeen user login and attempt to update user's profile data.
const int kProfileDataDownloadDelaySec = 10;

// Interval betweeen retries to update user's profile data.
const int kProfileDataDownloadRetryIntervalSec = 300;

// Delay betweeen subsequent profile refresh attempts (24 hrs).
const int kProfileRefreshIntervalSec = 24 * 3600;

const char kSafeImagePathExtension[] = ".jpg";

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
// Time histogram suffix for a profile image download retry.
const char kProfileDownloadReasonRetry[] = "Retry";

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

  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      histogram_name, min_time, max_time, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->AddTime(time_delta);

  DVLOG(1) << "Profile image download time: " << time_delta.InSecondsF();
}

// Deletes image file.
void DeleteImageFile(const std::string& image_path) {
  if (image_path.empty())
    return;
  base::FilePath fp(image_path);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&file_util::Delete),
                 fp, /* recursive= */  false));
}

// Converts |image_index| to UMA histogram value.
int ImageIndexToHistogramIndex(int image_index) {
  switch (image_index) {
    case User::kExternalImageIndex:
      // TODO(ivankr): Distinguish this from selected from file.
      return kHistogramImageFromCamera;
    case User::kProfileImageIndex:
      return kHistogramImageFromProfile;
    default:
      return image_index;
  }
}

}  // namespace

// static
int UserImageManagerImpl::user_image_migration_delay_sec =
    kUserImageMigrationDelaySec;

// static
void UserImageManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kUserImages);
  registry->RegisterDictionaryPref(kUserImageProperties);
}

UserImageManagerImpl::UserImageManagerImpl()
    : image_loader_(new UserImageLoader(ImageDecoder::ROBUST_JPEG_CODEC)),
      unsafe_image_loader_(new UserImageLoader(ImageDecoder::DEFAULT_CODEC)),
      last_image_set_async_(false),
      downloaded_profile_image_data_url_(chrome::kAboutBlankURL),
      downloading_profile_image_(false),
      migrate_current_user_on_load_(false) {
}

UserImageManagerImpl::~UserImageManagerImpl() {
}

void UserImageManagerImpl::LoadUserImages(const UserList& users) {
  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* prefs_images_unsafe =
      local_state->GetDictionary(kUserImages);
  const DictionaryValue* prefs_images =
      local_state->GetDictionary(kUserImageProperties);
  if (!prefs_images && !prefs_images_unsafe)
    return;

  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    User* user = *it;
    const base::DictionaryValue* image_properties = NULL;
    bool needs_migration = false;  // |true| if user has image in old format.
    bool safe_source = false;      // |true| if image loaded from safe source.

    if (prefs_images_unsafe) {
      needs_migration = prefs_images_unsafe->GetDictionaryWithoutPathExpansion(
          user->email(), &image_properties);
    }
    if (prefs_images) {
      safe_source = prefs_images->GetDictionaryWithoutPathExpansion(
          user->email(), &image_properties);
    }

    if (needs_migration)
      users_to_migrate_.insert(user->email());

    if (!image_properties) {
      SetInitialUserImage(user->email());
    } else {
      int image_index = User::kInvalidImageIndex;
      image_properties->GetInteger(kImageIndexNodeName, &image_index);

      if (image_index >= 0 && image_index < kDefaultImagesCount) {
        user->SetImage(UserImage(GetDefaultImage(image_index)),
                       image_index);
      } else if (image_index == User::kExternalImageIndex ||
                 image_index == User::kProfileImageIndex) {
        std::string image_path;
        image_properties->GetString(kImagePathNodeName, &image_path);
        // Path may be empty for profile images (meaning that the image
        // hasn't been downloaded for the first time yet, in which case a
        // download will be scheduled for |kProfileDataDownloadDelayMs|
        // after user logs in).
        DCHECK(!image_path.empty() || image_index == User::kProfileImageIndex);
        std::string image_url;
        image_properties->GetString(kImageURLNodeName, &image_url);
        GURL image_gurl(image_url);
        // Until image has been loaded, use the stub image (gray avatar).
        user->SetStubImage(image_index, true);
        user->SetImageURL(image_gurl);
        if (!image_path.empty()) {
          // Load user image asynchronously.
          UserImageLoader* loader =
              safe_source ? image_loader_.get() : unsafe_image_loader_.get();
          loader->Start(
              image_path, 0  /* no resize */,
              base::Bind(&UserImageManagerImpl::SetUserImage,
                         base::Unretained(this),
                         user->email(), image_index, image_gurl));
        }
      } else {
        NOTREACHED();
      }
    }
  }
}

void UserImageManagerImpl::UserLoggedIn(const std::string& email,
                                        bool user_is_new,
                                        bool user_is_local) {
  if (user_is_new) {
    SetInitialUserImage(email);
  } else {
    int image_index = UserManager::Get()->GetLoggedInUser()->image_index();

    if (!user_is_local) {
      // If current user image is profile image, it needs to be refreshed.
      bool download_profile_image = image_index == User::kProfileImageIndex;
      if (download_profile_image)
        InitDownloadedProfileImage();

      // Download user's profile data (full name and optionally image) to see if
      // it has changed.
      BrowserThread::PostDelayedTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&UserImageManagerImpl::DownloadProfileData,
                     base::Unretained(this),
                     kProfileDownloadReasonLoggedIn,
                     download_profile_image),
          base::TimeDelta::FromSeconds(kProfileDataDownloadDelaySec));
    }

    UMA_HISTOGRAM_ENUMERATION("UserImage.LoggedIn",
                              ImageIndexToHistogramIndex(image_index),
                              kHistogramImagesCount);

    if (users_to_migrate_.count(email)) {
      // User needs image format migration.
      BrowserThread::PostDelayedTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&UserImageManagerImpl::MigrateUserImage,
                     base::Unretained(this)),
          base::TimeDelta::FromSeconds(user_image_migration_delay_sec));
    }
  }

  if (!user_is_local) {
    // Set up a repeating timer for refreshing the profile data.
    profile_download_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kProfileRefreshIntervalSec),
        this, &UserImageManagerImpl::DownloadProfileDataScheduled);
  }
}

void UserImageManagerImpl::SaveUserDefaultImageIndex(
    const std::string& username,
    int image_index) {
  DCHECK(image_index >= 0 && image_index < kDefaultImagesCount);
  SetUserImage(username, image_index, GURL(),
               UserImage(GetDefaultImage(image_index)));
  SaveImageToLocalState(username, "", image_index, GURL(), false);
}

void UserImageManagerImpl::SaveUserImage(const std::string& username,
                                         const UserImage& user_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SaveUserImageInternal(username, User::kExternalImageIndex,
                        GURL(), user_image);
}

void UserImageManagerImpl::SaveUserImageFromFile(const std::string& username,
                                                 const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Always use unsafe image loader because we resize the image when saving
  // anyway.
  unsafe_image_loader_->Start(
      path.value(), login::kMaxUserImageSize,
      base::Bind(&UserImageManagerImpl::SaveUserImage,
                 base::Unretained(this), username));
}

void UserImageManagerImpl::SaveUserImageFromProfileImage(
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

void UserImageManagerImpl::DeleteUserImage(const std::string& username) {
  // Delete from the old dictionary, if present.
  DeleteOldUserImage(username);

  PrefService* prefs = g_browser_process->local_state();
  DictionaryPrefUpdate prefs_images_update(prefs, kUserImageProperties);
  const base::DictionaryValue* image_properties;
  if (prefs_images_update->GetDictionaryWithoutPathExpansion(
          username, &image_properties)) {
    std::string image_path;
    image_properties->GetString(kImageURLNodeName, &image_path);
    prefs_images_update->RemoveWithoutPathExpansion(username, NULL);
    DeleteImageFile(image_path);
  }
}

void UserImageManagerImpl::DownloadProfileImage(const std::string& reason) {
  DownloadProfileData(reason, true);
}

const gfx::ImageSkia& UserImageManagerImpl::DownloadedProfileImage() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return downloaded_profile_image_;
}

base::FilePath UserImageManagerImpl::GetImagePathForUser(
    const std::string& username) {
  std::string filename = username + kSafeImagePathExtension;
  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  return user_data_dir.AppendASCII(filename);
}

void UserImageManagerImpl::SetInitialUserImage(const std::string& username) {
  // Choose a random default image.
  int image_id =
      base::RandInt(kFirstDefaultImageIndex, kDefaultImagesCount - 1);
  SaveUserDefaultImageIndex(username, image_id);
}

void UserImageManagerImpl::SetUserImage(const std::string& username,
                                        int image_index,
                                        const GURL& image_url,
                                        const UserImage& user_image) {
  User* user = const_cast<User*>(UserManager::Get()->FindUser(username));
  // User may have been removed by now.
  if (user) {
    bool image_changed = user->image_index() != User::kInvalidImageIndex;
    bool is_current_user = user == UserManager::Get()->GetLoggedInUser();
    if (!user_image.image().isNull())
      user->SetImage(user_image, image_index);
    else
      user->SetStubImage(image_index, false);
    user->SetImageURL(image_url);
    // For the logged-in user with a profile picture, initialize
    // |downloaded_profile_picture_|.
    if (is_current_user && image_index == User::kProfileImageIndex) {
      InitDownloadedProfileImage();
    }
    if (image_changed) {
      // Unless this is first-time setting with |SetInitialUserImage|,
      // send a notification about image change.
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
          content::Source<UserImageManager>(this),
          content::Details<const User>(user));
    }
    if (is_current_user && migrate_current_user_on_load_)
      MigrateUserImage();
  }
}

void UserImageManagerImpl::SaveUserImageInternal(const std::string& username,
                                                 int image_index,
                                                 const GURL& image_url,
                                                 const UserImage& user_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetUserImage(username, image_index, image_url, user_image);

  // Ignore if data stored or cached outside the user's cryptohome is to be
  // treated as ephemeral.
  if (UserManager::Get()->IsUserNonCryptohomeDataEphemeral(username))
    return;

  base::FilePath image_path = GetImagePathForUser(username);
  DVLOG(1) << "Saving user image to " << image_path.value();

  last_image_set_async_ = true;

  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&UserImageManagerImpl::SaveImageToFile,
                 base::Unretained(this),
                 username, user_image, image_path, image_index, image_url),
      /* is_slow= */ false);
}

void UserImageManagerImpl::SaveImageToFile(const std::string& username,
                                           const UserImage& user_image,
                                           const base::FilePath& image_path,
                                           int image_index,
                                           const GURL& image_url) {
  if (!SaveBitmapToFile(user_image, image_path))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UserImageManagerImpl::SaveImageToLocalState,
                 base::Unretained(this),
                 username, image_path.value(), image_index, image_url, true));
}

void UserImageManagerImpl::SaveImageToLocalState(const std::string& username,
                                                 const std::string& image_path,
                                                 int image_index,
                                                 const GURL& image_url,
                                                 bool is_async) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ignore if data stored or cached outside the user's cryptohome is to be
  // treated as ephemeral.
  if (UserManager::Get()->IsUserNonCryptohomeDataEphemeral(username))
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
  DictionaryPrefUpdate images_update(local_state, kUserImageProperties);
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

  if (users_to_migrate_.count(username)) {
    DeleteOldUserImage(username);
    users_to_migrate_.erase(username);
  }

  UserManager::Get()->NotifyLocalStateChanged();
}

bool UserImageManagerImpl::SaveBitmapToFile(const UserImage& user_image,
                                            const base::FilePath& image_path) {
  UserImage safe_image;
  const UserImage::RawImage* encoded_image = NULL;
  if (!user_image.is_safe_format()) {
    safe_image = UserImage::CreateAndEncode(user_image.image());
    encoded_image = &safe_image.raw_image();
    UMA_HISTOGRAM_MEMORY_KB("UserImage.RecodedJpegSize", encoded_image->size());
  } else if (user_image.has_raw_image()) {
    encoded_image = &user_image.raw_image();
  } else {
    NOTREACHED() << "Raw image missing.";
    return false;
  }

  if (file_util::WriteFile(image_path,
                           reinterpret_cast<const char*>(&(*encoded_image)[0]),
                           encoded_image->size()) == -1) {
    LOG(ERROR) << "Failed to save image to file.";
    return false;
  }
  return true;
}

void UserImageManagerImpl::InitDownloadedProfileImage() {
  const User* logged_in_user = UserManager::Get()->GetLoggedInUser();
  DCHECK_EQ(logged_in_user->image_index(), User::kProfileImageIndex);
  if (downloaded_profile_image_.isNull() && !logged_in_user->image_is_stub()) {
    VLOG(1) << "Profile image initialized";
    downloaded_profile_image_ = logged_in_user->image();
    downloaded_profile_image_data_url_ =
        webui::GetBitmapDataUrl(*downloaded_profile_image_.bitmap());
    profile_image_url_ = logged_in_user->image_url();
  }
}

void UserImageManagerImpl::DownloadProfileData(const std::string& reason,
                                               bool download_image) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // GAIA profiles exist for regular users only.
  if (!UserManager::Get()->IsLoggedInAsRegularUser())
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

void UserImageManagerImpl::DownloadProfileDataScheduled() {
  const User* logged_in_user = UserManager::Get()->GetLoggedInUser();
  // If current user image is profile image, it needs to be refreshed.
  bool download_profile_image =
      logged_in_user->image_index() == User::kProfileImageIndex;
  DownloadProfileData(kProfileDownloadReasonScheduled, download_profile_image);
}

void UserImageManagerImpl::DownloadProfileDataRetry(bool download_image) {
  DownloadProfileData(kProfileDownloadReasonRetry, download_image);
}

// ProfileDownloaderDelegate override.
bool UserImageManagerImpl::NeedsProfilePicture() const {
  return downloading_profile_image_;
}

// ProfileDownloaderDelegate override.
int UserImageManagerImpl::GetDesiredImageSideLength() const {
  return GetCurrentUserImageSize();
}

// ProfileDownloaderDelegate override.
std::string UserImageManagerImpl::GetCachedPictureURL() const {
  return profile_image_url_.spec();
}

Profile* UserImageManagerImpl::GetBrowserProfile() {
  return ProfileManager::GetDefaultProfile();
}

void UserImageManagerImpl::OnProfileDownloadSuccess(
    ProfileDownloader* downloader) {
  // Make sure that |ProfileDownloader| gets deleted after return.
  scoped_ptr<ProfileDownloader> profile_image_downloader(
      profile_image_downloader_.release());
  DCHECK_EQ(downloader, profile_image_downloader.get());

  UserManager* user_manager = UserManager::Get();
  const User* user = user_manager->GetLoggedInUser();

  if (!downloader->GetProfileFullName().empty()) {
    user_manager->SaveUserDisplayName(
        user->email(), downloader->GetProfileFullName());
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
        content::Source<UserImageManager>(this),
        content::NotificationService::NoDetails());
  }

  // Nothing to do if picture is cached or the default avatar.
  if (result != kDownloadSuccess)
    return;

  // Check if this image is not the same as already downloaded.
  SkBitmap new_bitmap(downloader->GetProfilePicture());
  std::string new_image_data_url = webui::GetBitmapDataUrl(new_bitmap);
  if (!downloaded_profile_image_data_url_.empty() &&
      new_image_data_url == downloaded_profile_image_data_url_)
    return;

  downloaded_profile_image_data_url_ = new_image_data_url;
  downloaded_profile_image_ = gfx::ImageSkia::CreateFrom1xBitmap(
      downloader->GetProfilePicture());
  profile_image_url_ = GURL(downloader->GetProfilePictureURL());

  if (user->image_index() == User::kProfileImageIndex) {
    VLOG(1) << "Updating profile image for logged-in user";
    UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
                              kDownloadSuccessChanged,
                              kDownloadResultsCount);
    // This will persist |downloaded_profile_image_| to file.
    SaveUserImageFromProfileImage(user->email());
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
      content::Source<UserImageManager>(this),
      content::Details<const gfx::ImageSkia>(&downloaded_profile_image_));
}

void UserImageManagerImpl::OnProfileDownloadFailure(
    ProfileDownloader* downloader,
    ProfileDownloaderDelegate::FailureReason reason) {
  DCHECK_EQ(downloader, profile_image_downloader_.get());
  profile_image_downloader_.reset();

  UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
      kDownloadFailure, kDownloadResultsCount);

  DCHECK(!profile_image_load_start_time_.is_null());
  base::TimeDelta delta = base::Time::Now() - profile_image_load_start_time_;
  AddProfileImageTimeHistogram(kDownloadFailure, profile_image_download_reason_,
                               delta);

  // Retry download after some time if a network error has occured.
  if (reason == ProfileDownloaderDelegate::NETWORK_ERROR) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&UserImageManagerImpl::DownloadProfileDataRetry,
                   base::Unretained(this),
                   downloading_profile_image_),
        base::TimeDelta::FromSeconds(kProfileDataDownloadRetryIntervalSec));
  }

  downloading_profile_image_ = false;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
      content::Source<UserImageManager>(this),
      content::NotificationService::NoDetails());
}

void UserImageManagerImpl::MigrateUserImage() {
  User* user = UserManager::Get()->GetLoggedInUser();
  if (user->image_is_loading()) {
    LOG(INFO) << "Waiting for user image to load before migration";
    migrate_current_user_on_load_ = true;
    return;
  }
  migrate_current_user_on_load_ = false;
  if (user->has_raw_image() && user->image_is_safe_format()) {
    // Nothing to migrate already, make sure we delete old image.
    DeleteOldUserImage(user->email());
    users_to_migrate_.erase(user->email());
    return;
  }
  if (user->HasDefaultImage()) {
    SaveUserDefaultImageIndex(user->email(), user->image_index());
  } else {
    SaveUserImageInternal(user->email(), user->image_index(),
                          user->image_url(), user->user_image());
  }
  UMA_HISTOGRAM_ENUMERATION("UserImage.Migration",
                            ImageIndexToHistogramIndex(user->image_index()),
                            kHistogramImagesCount);
}

void UserImageManagerImpl::DeleteOldUserImage(const std::string& username) {
  PrefService* prefs = g_browser_process->local_state();
  DictionaryPrefUpdate prefs_images_update(prefs, kUserImages);
  const base::DictionaryValue* image_properties;
  if (prefs_images_update->GetDictionaryWithoutPathExpansion(
          username, &image_properties)) {
    std::string image_path;
    image_properties->GetString(kImagePathNodeName, &image_path);
    prefs_images_update->RemoveWithoutPathExpansion(username, NULL);
    DeleteImageFile(image_path);
  }
}

}  // namespace chromeos
