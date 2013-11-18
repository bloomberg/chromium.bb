// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_manager_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_sync_observer.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

namespace {

// A dictionary that maps user_ids to old user image data with images stored in
// PNG format. Deprecated.
// TODO(ivankr): remove this const char after migration is gone.
const char kUserImages[] = "UserImages";

// A dictionary that maps user_ids to user image data with images stored in
// JPEG format.
const char kUserImageProperties[] = "user_image_info";

// Names of user image properties.
const char kImagePathNodeName[] = "path";
const char kImageIndexNodeName[] = "index";
const char kImageURLNodeName[] = "url";

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

// Add a histogram showing the time it takes to download profile image.
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

bool SaveImage(const UserImage& user_image, const base::FilePath& image_path) {
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

}  // namespace

// static
void UserImageManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kUserImages);
  registry->RegisterDictionaryPref(kUserImageProperties);
}

// Every image load or update is encapsulated by a Job. The Job is allowed to
// perform tasks on background threads or in helper processes but:
// * Changes to User objects and local state as well as any calls to the
//   |parent_| must be performed on the thread that the Job is created on only.
// * File writes and deletions must be performed via the |parent_|'s
//   |background_task_runner_| only.
//
// Only one of the Load*() and Set*() methods may be called per Job.
class UserImageManagerImpl::Job {
 public:
  // The |Job| will update the |user| object for |user_id|.
  Job(UserImageManagerImpl* parent, const std::string& user_id);
  ~Job();

  // Loads the image at |image_path| or one of the default images, depending on
  // |image_index|, and updates the |user| object for |user_id_| with the new
  // image.
  void LoadImage(base::FilePath image_path,
                 const int image_index,
                 const GURL& image_url);

  // Sets the user image for |user_id_| in local state to the default image
  // indicated by |default_image_index|. Also updates the |user| object for
  // |user_id_| with the new image.
  void SetToDefaultImage(int default_image_index);

  // Saves the |user_image| to disk and sets the user image for |user_id_| in
  // local state to that image. Also updates the |user| object for |user_id_|
  // with the new image.
  void SetToImage(int image_index,
                  const UserImage& user_image);

  // Loads the the image at |path|, transcodes it to JPEG format, saves the
  // image to disk and sets the user image for |user_id_| in local state to that
  // image. If  |resize| is true, the image is cropped and resized before
  // transcoding. Also  updates the |user| object for |user_id_| with the new
  // image.
  void SetToPath(const base::FilePath& path,
                 int image_index,
                 const GURL& image_url,
                 bool resize);

 private:
  // Called back after an image has been loaded from disk.
  void OnLoadImageDone(bool save, const UserImage& user_image);

  // Updates the |user| object for |user_id_| with |user_image_|.
  void UpdateUser();

  // Saves |user_image_| to disk in JPEG format. Local state will be updated
  // when a callback indicates that the save has been successful.
  void SaveImageAndUpdateLocalState();

  // Called back after the |user_image_| has been saved to disk. If |success| is
  // true sets the user image for |user_id_| in local state to that image.
  void OnSaveImageDone(bool success);

  // Updates the user image for |user_id_| in local state, setting it to
  // one of the default images or the saved |user_image_|, depending on
  // |image_index_|.
  void UpdateLocalState();

  // Notifies the |parent_| that the Job is done.
  void NotifyJobDone();

  UserImageManagerImpl* parent_;
  const std::string user_id_;

  // Whether one of the Load*() or Set*() methods has been run already.
  bool run_;

  int image_index_;
  GURL image_url_;
  base::FilePath image_path_;

  UserImage user_image_;

  base::WeakPtrFactory<Job> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

UserImageManagerImpl::Job::Job(UserImageManagerImpl* parent,
                               const std::string& user_id)
    : parent_(parent),
      user_id_(user_id),
      run_(false),
      weak_factory_(this) {
}

UserImageManagerImpl::Job::~Job() {
}

void UserImageManagerImpl::Job::LoadImage(base::FilePath image_path,
                                          const int image_index,
                                          const GURL& image_url) {
  DCHECK(!run_);
  run_ = true;

  image_index_ = image_index;
  image_url_ = image_url;
  image_path_ = image_path;

  if (image_index_ >= 0 && image_index_ < kDefaultImagesCount) {
    // Load one of the default images. This happens synchronously.
    user_image_ = UserImage(GetDefaultImage(image_index_));
    UpdateUser();
    NotifyJobDone();
  } else if (image_index_ == User::kExternalImageIndex ||
             image_index_ == User::kProfileImageIndex) {
    // Load the user image from a file referenced by |image_path|. This happens
    // asynchronously. The JPEG image loader can be used here because
    // LoadImage() is called only for users whose user image has previously
    // been set by one of the Set*() methods, which transcode to JPEG format.
    DCHECK(!image_path_.empty());
    parent_->image_loader_->Start(image_path_.value(),
                                  0,
                                  base::Bind(&Job::OnLoadImageDone,
                                             weak_factory_.GetWeakPtr(),
                                             false));
  } else {
    NOTREACHED();
    NotifyJobDone();
  }
}

void UserImageManagerImpl::Job::SetToDefaultImage(int default_image_index) {
  DCHECK(!run_);
  run_ = true;

  DCHECK_LE(0, default_image_index);
  DCHECK_GT(kDefaultImagesCount, default_image_index);

  image_index_ = default_image_index;
  user_image_ = UserImage(GetDefaultImage(image_index_));

  UpdateUser();
  UpdateLocalState();
  NotifyJobDone();
}

void UserImageManagerImpl::Job::SetToImage(int image_index,
                                           const UserImage& user_image) {
  DCHECK(!run_);
  run_ = true;

  DCHECK(image_index == User::kExternalImageIndex ||
         image_index == User::kProfileImageIndex);

  image_index_ = image_index;
  user_image_ = user_image;

  UpdateUser();
  SaveImageAndUpdateLocalState();
}

void UserImageManagerImpl::Job::SetToPath(const base::FilePath& path,
                                          int image_index,
                                          const GURL& image_url,
                                          bool resize) {
  DCHECK(!run_);
  run_ = true;

  image_index_ = image_index;
  image_url_ = image_url;

  DCHECK(!path.empty());
  parent_->unsafe_image_loader_->Start(path.value(),
                                       resize ? login::kMaxUserImageSize : 0,
                                       base::Bind(&Job::OnLoadImageDone,
                                                  weak_factory_.GetWeakPtr(),
                                                  true));
}

void UserImageManagerImpl::Job::OnLoadImageDone(bool save,
                                                const UserImage& user_image) {
  user_image_ = user_image;
  UpdateUser();
  if (save)
    SaveImageAndUpdateLocalState();
  else
    NotifyJobDone();
}

void UserImageManagerImpl::Job::UpdateUser() {
  User* user = UserManager::Get()->FindUserAndModify(user_id_);
  if (!user)
    return;

  if (!user_image_.image().isNull())
    user->SetImage(user_image_, image_index_);
  else
    user->SetStubImage(image_index_, false);
  user->SetImageURL(image_url_);

  parent_->OnJobChangedUserImage(user);
}

void UserImageManagerImpl::Job::SaveImageAndUpdateLocalState() {
  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  image_path_ = user_data_dir.Append(user_id_ + kSafeImagePathExtension);

  base::PostTaskAndReplyWithResult(
      parent_->background_task_runner_,
      FROM_HERE,
      base::Bind(&SaveImage, user_image_, image_path_),
      base::Bind(&Job::OnSaveImageDone, weak_factory_.GetWeakPtr()));
}

void UserImageManagerImpl::Job::OnSaveImageDone(bool success) {
  if (success)
    UpdateLocalState();
  NotifyJobDone();
}

void UserImageManagerImpl::Job::UpdateLocalState() {
  // Ignore if data stored or cached outside the user's cryptohome is to be
  // treated as ephemeral.
  if (UserManager::Get()->IsUserNonCryptohomeDataEphemeral(user_id_))
    return;

  scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
  entry->Set(kImagePathNodeName, new base::StringValue(image_path_.value()));
  entry->Set(kImageIndexNodeName, new base::FundamentalValue(image_index_));
  if (!image_url_.is_empty())
    entry->Set(kImageURLNodeName, new StringValue(image_url_.spec()));
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              kUserImageProperties);
  update->SetWithoutPathExpansion(user_id_, entry.release());

  UserManager::Get()->NotifyLocalStateChanged();
}

void UserImageManagerImpl::Job::NotifyJobDone() {
  parent_->OnJobDone(user_id_);
}

UserImageManagerImpl::UserImageManagerImpl()
    : downloading_profile_image_(false),
      profile_image_requested_(false),
      weak_factory_(this) {
  base::SequencedWorkerPool* blocking_pool =
      content::BrowserThread::GetBlockingPool();
  background_task_runner_ =
      blocking_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          blocking_pool->GetSequenceToken(),
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  image_loader_ = new UserImageLoader(ImageDecoder::ROBUST_JPEG_CODEC,
                                      background_task_runner_);
  unsafe_image_loader_ = new UserImageLoader(ImageDecoder::DEFAULT_CODEC,
                                             background_task_runner_);
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
    const std::string& user_id = user->email();
    bool needs_migration = false;

    // If entries are found in both |prefs_images_unsafe| and |prefs_images|,
    // |prefs_images| is honored for now but |prefs_images_unsafe| will be
    // migrated, overwriting the |prefs_images| entry, when the user logs in.
    const base::DictionaryValue* image_properties = NULL;
    if (prefs_images_unsafe) {
      needs_migration = prefs_images_unsafe->GetDictionaryWithoutPathExpansion(
          user_id, &image_properties);
      if (needs_migration)
        users_to_migrate_.insert(user_id);
    }
    if (prefs_images) {
      prefs_images->GetDictionaryWithoutPathExpansion(user_id,
                                                      &image_properties);
    }

    if (!image_properties) {
      SetInitialUserImage(user_id);
      continue;
    }

    int image_index = User::kInvalidImageIndex;
    image_properties->GetInteger(kImageIndexNodeName, &image_index);
    if (image_index >= 0 && image_index < kDefaultImagesCount) {
      user->SetImage(UserImage(GetDefaultImage(image_index)),
                     image_index);
      continue;
    }

    if (image_index != User::kExternalImageIndex &&
        image_index != User::kProfileImageIndex) {
      NOTREACHED();
      continue;
    }

    std::string image_url_string;
    image_properties->GetString(kImageURLNodeName, &image_url_string);
    GURL image_url(image_url_string);
    std::string image_path;
    image_properties->GetString(kImagePathNodeName, &image_path);

    user->SetImageURL(image_url);
    DCHECK(!image_path.empty() || image_index == User::kProfileImageIndex);
    if (image_path.empty() || needs_migration) {
      // Use a stub image (gray avatar) if either of the following is true:
      // * The profile image is to be used but has not been downloaded yet. The
      //   profile image will be downloaded after login.
      // * The image needs migration. Migration will be performed after login.
      user->SetStubImage(image_index, true);
      continue;
    }

    linked_ptr<Job>& job = jobs_[user_id];
    job.reset(new Job(this, user_id));
    job->LoadImage(base::FilePath(image_path), image_index, image_url);
  }
}

void UserImageManagerImpl::UserLoggedIn(const std::string& user_id,
                                        bool user_is_new,
                                        bool user_is_local) {
  User* user = UserManager::Get()->GetLoggedInUser();
  if (user_is_new) {
    if (!user_is_local)
      SetInitialUserImage(user_id);
  } else {
    UMA_HISTOGRAM_ENUMERATION("UserImage.LoggedIn",
                              ImageIndexToHistogramIndex(user->image_index()),
                              kHistogramImagesCount);

    if (users_to_migrate_.find(user_id) != users_to_migrate_.end()) {
      const DictionaryValue* prefs_images_unsafe =
          g_browser_process->local_state()->GetDictionary(kUserImages);
      const base::DictionaryValue* image_properties = NULL;
      if (prefs_images_unsafe->GetDictionaryWithoutPathExpansion(
              user_id, &image_properties)) {
        std::string image_path;
        image_properties->GetString(kImagePathNodeName, &image_path);
        linked_ptr<Job>& job = jobs_[user_id];
        job.reset(new Job(this, user_id));
        if (!image_path.empty()) {
          LOG(INFO) << "Loading old user image, then migrating it.";
          job->SetToPath(base::FilePath(image_path),
                         user->image_index(),
                         user->image_url(),
                         false);
        } else {
          job->SetToDefaultImage(user->image_index());
        }
      }
    }
  }

  // Reset the downloaded profile image as a new user logged in.
  downloaded_profile_image_ = gfx::ImageSkia();
  downloaded_profile_image_data_url_.clear();
  profile_image_url_ = GURL();
  profile_image_requested_ = false;

  if (UserManager::Get()->IsLoggedInAsRegularUser()) {
    TryToInitDownloadedProfileImage();

    // Schedule an initial download of the profile data (full name and
    // optionally image).
    profile_download_one_shot_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kProfileDataDownloadDelaySec),
        base::Bind(&UserImageManagerImpl::DownloadProfileData,
                   base::Unretained(this),
                   kProfileDownloadReasonLoggedIn));
    // Schedule periodic refreshes of the profile data.
    profile_download_periodic_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kProfileRefreshIntervalSec),
        base::Bind(&UserImageManagerImpl::DownloadProfileData,
                   base::Unretained(this),
                   kProfileDownloadReasonScheduled));
  } else {
    profile_download_one_shot_timer_.Stop();
    profile_download_periodic_timer_.Stop();
  }

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (user_image_sync_observer_.get() &&
      !command_line->HasSwitch(::switches::kMultiProfiles)) {
    NOTREACHED() << "User logged in more than once.";
  }

  if (user->CanSyncImage() &&
      !command_line->HasSwitch(chromeos::switches::kDisableUserImageSync)) {
    user_image_sync_observer_.reset(new UserImageSyncObserver(user));
  } else {
    user_image_sync_observer_.reset();
  }
}

void UserImageManagerImpl::SaveUserDefaultImageIndex(const std::string& user_id,
                                                     int default_image_index) {
  linked_ptr<Job>& job = jobs_[user_id];
  job.reset(new Job(this, user_id));
  job->SetToDefaultImage(default_image_index);
}

void UserImageManagerImpl::SaveUserImage(const std::string& user_id,
                                         const UserImage& user_image) {
  linked_ptr<Job>& job = jobs_[user_id];
  job.reset(new Job(this, user_id));
  job->SetToImage(User::kExternalImageIndex, user_image);
}

void UserImageManagerImpl::SaveUserImageFromFile(const std::string& user_id,
                                                 const base::FilePath& path) {
  linked_ptr<Job>& job = jobs_[user_id];
  job.reset(new Job(this, user_id));
  job->SetToPath(path, User::kExternalImageIndex, GURL(), true);
}

void UserImageManagerImpl::SaveUserImageFromProfileImage(
    const std::string& user_id) {
  // Use the profile image if it has been downloaded already. Otherwise, use a
  // stub image (gray avatar).
  linked_ptr<Job>& job = jobs_[user_id];
  job.reset(new Job(this, user_id));
  job->SetToImage(User::kProfileImageIndex,
                  downloaded_profile_image_.isNull() ?
                      UserImage() :
                      UserImage::CreateAndEncode(downloaded_profile_image_));
}

void UserImageManagerImpl::DeleteUserImage(const std::string& user_id) {
  jobs_.erase(user_id);
  DeleteUserImageAndLocalStateEntry(user_id, kUserImages);
  DeleteUserImageAndLocalStateEntry(user_id, kUserImageProperties);
}

void UserImageManagerImpl::DownloadProfileImage(const std::string& reason) {
  profile_image_requested_ = true;
  DownloadProfileData(reason);
}

UserImageSyncObserver* UserImageManagerImpl::GetSyncObserver() const {
  return user_image_sync_observer_.get();
}

void UserImageManagerImpl::Shutdown() {
  profile_downloader_.reset();
  user_image_sync_observer_.reset();
}

const gfx::ImageSkia& UserImageManagerImpl::DownloadedProfileImage() const {
  return downloaded_profile_image_;
}

void UserImageManagerImpl::SetInitialUserImage(const std::string& user_id) {
  // Choose a random default image.
  SaveUserDefaultImageIndex(user_id,
                            base::RandInt(kFirstDefaultImageIndex,
                                          kDefaultImagesCount - 1));
}

void UserImageManagerImpl::TryToInitDownloadedProfileImage() {
  const User* user = UserManager::Get()->GetLoggedInUser();
  if (user->image_index() == User::kProfileImageIndex &&
      downloaded_profile_image_.isNull() &&
      !user->image_is_stub()) {
    // Initialize the |downloaded_profile_image_| for the currently logged-in
    // user if it has not been initialized already, the user image is the
    // profile image and the user image has been loaded successfully.
    VLOG(1) << "Profile image initialized from disk.";
    downloaded_profile_image_ = user->image();
    downloaded_profile_image_data_url_ =
        webui::GetBitmapDataUrl(*downloaded_profile_image_.bitmap());
    profile_image_url_ = user->image_url();
  }
}

bool UserImageManagerImpl::NeedProfileImage() const {
  return UserManager::Get()->IsLoggedInAsRegularUser() &&
         (UserManager::Get()->GetLoggedInUser()->image_index() ==
              User::kProfileImageIndex ||
          profile_image_requested_);
}

void UserImageManagerImpl::DownloadProfileData(const std::string& reason) {
  // GAIA profiles exist for regular users only.
  if (!UserManager::Get()->IsLoggedInAsRegularUser())
    return;

  // If a download is already in progress, allow it to continue, with one
  // exception: If the current download does not include the profile image but
  // the image has since become necessary, start a new download that includes
  // the profile image.
  if (profile_downloader_ &&
      (downloading_profile_image_ || !NeedProfileImage())) {
    return;
  }

  downloading_profile_image_ = NeedProfileImage();
  profile_image_download_reason_ = reason;
  profile_image_load_start_time_ = base::TimeTicks::Now();
  profile_downloader_.reset(new ProfileDownloader(this));
  profile_downloader_->Start();
}

bool UserImageManagerImpl::NeedsProfilePicture() const {
  return downloading_profile_image_;
}

int UserImageManagerImpl::GetDesiredImageSideLength() const {
  return GetCurrentUserImageSize();
}

std::string UserImageManagerImpl::GetCachedPictureURL() const {
  return profile_image_url_.spec();
}

Profile* UserImageManagerImpl::GetBrowserProfile() {
  return ProfileManager::GetDefaultProfile();
}

void UserImageManagerImpl::OnProfileDownloadSuccess(
    ProfileDownloader* downloader) {
  // Ensure that the |profile_downloader_| is deleted when this method returns.
  scoped_ptr<ProfileDownloader> profile_downloader(
      profile_downloader_.release());
  DCHECK_EQ(downloader, profile_downloader.get());

  const User* user = UserManager::Get()->GetLoggedInUser();
  const std::string& user_id = user->email();

  UserManager::Get()->UpdateUserAccountData(user_id,
                                            downloader->GetProfileFullName(),
                                            downloader->GetProfileLocale());

  if (!downloading_profile_image_)
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
                            result,
                            kDownloadResultsCount);
  DCHECK(!profile_image_load_start_time_.is_null());
  AddProfileImageTimeHistogram(
      result,
      profile_image_download_reason_,
      base::TimeTicks::Now() - profile_image_load_start_time_);

  // Ignore the image if it is no longer needed.
  if (!NeedProfileImage())
    return;

  if (result == kDownloadDefault) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
        content::Source<UserImageManager>(this),
        content::NotificationService::NoDetails());
  }

  // Nothing to do if the picture is cached or is the default avatar.
  if (result != kDownloadSuccess)
    return;

  profile_image_requested_ = false;

  // Check if this image is not the same as already downloaded.
  const std::string new_image_data_url =
      webui::GetBitmapDataUrl(SkBitmap(downloader->GetProfilePicture()));
  if (!downloaded_profile_image_data_url_.empty() &&
      new_image_data_url == downloaded_profile_image_data_url_) {
    return;
  }

  downloaded_profile_image_data_url_ = new_image_data_url;
  downloaded_profile_image_ = gfx::ImageSkia::CreateFrom1xBitmap(
      downloader->GetProfilePicture());
  profile_image_url_ = GURL(downloader->GetProfilePictureURL());

  if (user->image_index() == User::kProfileImageIndex) {
    VLOG(1) << "Updating profile image for logged-in user.";
    UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
                              kDownloadSuccessChanged,
                              kDownloadResultsCount);
    // This will persist |downloaded_profile_image_| to disk.
    SaveUserImageFromProfileImage(user_id);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
      content::Source<UserImageManager>(this),
      content::Details<const gfx::ImageSkia>(&downloaded_profile_image_));
}

void UserImageManagerImpl::OnProfileDownloadFailure(
    ProfileDownloader* downloader,
    ProfileDownloaderDelegate::FailureReason reason) {
  DCHECK_EQ(downloader, profile_downloader_.get());
  profile_downloader_.reset();

  if (downloading_profile_image_) {
    UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
                              kDownloadFailure,
                              kDownloadResultsCount);
    DCHECK(!profile_image_load_start_time_.is_null());
    AddProfileImageTimeHistogram(
        kDownloadFailure,
        profile_image_download_reason_,
        base::TimeTicks::Now() - profile_image_load_start_time_);
  }

  UserManager* user_manager = UserManager::Get();
  const User* user = user_manager->GetLoggedInUser();

  user_manager->UpdateUserAccountData(user->email(), string16(), "");

  if (reason == ProfileDownloaderDelegate::NETWORK_ERROR) {
    // Retry download after a delay if a network error occurred.
    profile_download_one_shot_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kProfileDataDownloadRetryIntervalSec),
        base::Bind(&UserImageManagerImpl::DownloadProfileData,
                   base::Unretained(this),
                   kProfileDownloadReasonRetry));
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
      content::Source<UserImageManager>(this),
      content::NotificationService::NoDetails());
}

void UserImageManagerImpl::DeleteUserImageAndLocalStateEntry(
    const std::string& user_id,
    const char* prefs_dict_root) {
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              prefs_dict_root);
  const base::DictionaryValue* image_properties;
  if (!update->GetDictionaryWithoutPathExpansion(user_id, &image_properties))
    return;

  std::string image_path;
  image_properties->GetString(kImagePathNodeName, &image_path);
  if (!image_path.empty()) {
    background_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::DeleteFile),
                   base::FilePath(image_path),
                   false));
  }
  update->RemoveWithoutPathExpansion(user_id, NULL);
}

void UserImageManagerImpl::OnJobChangedUserImage(const User* user) {
  if (user == UserManager::Get()->GetLoggedInUser())
    TryToInitDownloadedProfileImage();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
      content::Source<UserImageManagerImpl>(this),
      content::Details<const User>(user));
}

void UserImageManagerImpl::OnJobDone(const std::string& user_id) {
  std::map<std::string, linked_ptr<Job> >::iterator it =
      jobs_.find(user_id);
  if (it != jobs_.end()) {
    base::MessageLoopProxy::current()->DeleteSoon(FROM_HERE,
                                                  it->second.release());
    jobs_.erase(it);
  } else {
    NOTREACHED();
  }

  if (users_to_migrate_.find(user_id) == users_to_migrate_.end())
    return;
  // Migration completed for |user_id|.
  users_to_migrate_.erase(user_id);

  const DictionaryValue* prefs_images_unsafe =
      g_browser_process->local_state()->GetDictionary(kUserImages);
  const base::DictionaryValue* image_properties = NULL;
  if (!prefs_images_unsafe->GetDictionaryWithoutPathExpansion(
          user_id, &image_properties)) {
    NOTREACHED();
    return;
  }

  int image_index = User::kInvalidImageIndex;
  image_properties->GetInteger(kImageIndexNodeName, &image_index);
  UMA_HISTOGRAM_ENUMERATION("UserImage.Migration",
                            ImageIndexToHistogramIndex(image_index),
                            kHistogramImagesCount);

  std::string image_path;
  image_properties->GetString(kImagePathNodeName, &image_path);
  if (!image_path.empty()) {
    // If an old image exists, delete it and remove |user_id| from the old prefs
    // dictionary only after the deletion has completed. This ensures that no
    // orphaned image is left behind if the browser crashes before the deletion
    // has been performed: In that case, local state will be unchanged and the
    // migration will be run again on the user's next login.
    background_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::DeleteFile),
                   base::FilePath(image_path),
                   false),
        base::Bind(&UserImageManagerImpl::UpdateLocalStateAfterMigration,
                   weak_factory_.GetWeakPtr(),
                   user_id));
  } else {
    // If no old image exists, remove |user_id| from the old prefs dictionary.
    UpdateLocalStateAfterMigration(user_id);
  }
}

void UserImageManagerImpl::UpdateLocalStateAfterMigration(
    const std::string& user_id) {
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              kUserImages);
  update->RemoveWithoutPathExpansion(user_id, NULL);
}

}  // namespace chromeos
