// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/avatar/user_image_manager_impl.h"

#include "base/bind.h"
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
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/theme_resources.h"
#include "components/user_manager/user_image/default_user_images.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "policy/policy_constants.h"
#include "ui/base/resource/resource_bundle.h"
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
// Time histogram suffix for a profile image download when the user chooses the
// profile image but it has not been downloaded yet.
const char kProfileDownloadReasonProfileImageChosen[] = "ProfileImageChosen";
// Time histogram suffix for a scheduled profile image download.
const char kProfileDownloadReasonScheduled[] = "Scheduled";
// Time histogram suffix for a profile image download retry.
const char kProfileDownloadReasonRetry[] = "Retry";

static bool g_ignore_profile_data_download_delay_ = false;

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
    case user_manager::User::USER_IMAGE_EXTERNAL:
      // TODO(ivankr): Distinguish this from selected from file.
      return user_manager::kHistogramImageFromCamera;
    case user_manager::User::USER_IMAGE_PROFILE:
      return user_manager::kHistogramImageFromProfile;
    default:
      return image_index;
  }
}

bool SaveImage(const user_manager::UserImage& user_image,
               const base::FilePath& image_path) {
  user_manager::UserImage safe_image;
  const user_manager::UserImage::RawImage* encoded_image = NULL;
  if (!user_image.is_safe_format()) {
    safe_image = user_manager::UserImage::CreateAndEncode(user_image.image());
    encoded_image = &safe_image.raw_image();
    UMA_HISTOGRAM_MEMORY_KB("UserImage.RecodedJpegSize", encoded_image->size());
  } else if (user_image.has_raw_image()) {
    encoded_image = &user_image.raw_image();
  } else {
    NOTREACHED() << "Raw image missing.";
    return false;
  }

  if (!encoded_image->size() ||
      base::WriteFile(image_path,
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
  // The |Job| will update the user object corresponding to |parent|.
  explicit Job(UserImageManagerImpl* parent);
  ~Job();

  // Loads the image at |image_path| or one of the default images,
  // depending on |image_index|, and updates the user object with the
  // new image.
  void LoadImage(base::FilePath image_path,
                 const int image_index,
                 const GURL& image_url);

  // Sets the user image in local state to the default image indicated
  // by |default_image_index|. Also updates the user object with the
  // new image.
  void SetToDefaultImage(int default_image_index);

  // Saves the |user_image| to disk and sets the user image in local
  // state to that image. Also updates the user with the new image.
  void SetToImage(int image_index, const user_manager::UserImage& user_image);

  // Decodes the JPEG image |data|, crops and resizes the image, saves
  // it to disk and sets the user image in local state to that image.
  // Also updates the user object with the new image.
  void SetToImageData(scoped_ptr<std::string> data);

  // Loads the image at |path|, transcodes it to JPEG format, saves
  // the image to disk and sets the user image in local state to that
  // image.  If |resize| is true, the image is cropped and resized
  // before transcoding.  Also updates the user object with the new
  // image.
  void SetToPath(const base::FilePath& path,
                 int image_index,
                 const GURL& image_url,
                 bool resize);

 private:
  // Called back after an image has been loaded from disk.
  void OnLoadImageDone(bool save, const user_manager::UserImage& user_image);

  // Updates the user object with |user_image_|.
  void UpdateUser();

  // Saves |user_image_| to disk in JPEG format. Local state will be updated
  // when a callback indicates that the image has been saved.
  void SaveImageAndUpdateLocalState();

  // Called back after the |user_image_| has been saved to
  // disk. Updates the user image information in local state. The
  // information is only updated if |success| is true (indicating that
  // the image was saved successfully) or the user image is the
  // profile image (indicating that even if the image could not be
  // saved because it is not available right now, it will be
  // downloaded eventually).
  void OnSaveImageDone(bool success);

  // Updates the user image in local state, setting it to one of the
  // default images or the saved |user_image_|, depending on
  // |image_index_|.
  void UpdateLocalState();

  // Notifies the |parent_| that the Job is done.
  void NotifyJobDone();

  const std::string& user_id() const { return parent_->user_id(); }

  UserImageManagerImpl* parent_;

  // Whether one of the Load*() or Set*() methods has been run already.
  bool run_;

  int image_index_;
  GURL image_url_;
  base::FilePath image_path_;

  user_manager::UserImage user_image_;

  base::WeakPtrFactory<Job> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

UserImageManagerImpl::Job::Job(UserImageManagerImpl* parent)
    : parent_(parent),
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

  if (image_index_ >= 0 && image_index_ < user_manager::kDefaultImagesCount) {
    // Load one of the default images. This happens synchronously.
    user_image_ =
        user_manager::UserImage(user_manager::GetDefaultImage(image_index_));
    UpdateUser();
    NotifyJobDone();
  } else if (image_index_ == user_manager::User::USER_IMAGE_EXTERNAL ||
             image_index_ == user_manager::User::USER_IMAGE_PROFILE) {
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
  DCHECK_GT(user_manager::kDefaultImagesCount, default_image_index);

  image_index_ = default_image_index;
  user_image_ =
      user_manager::UserImage(user_manager::GetDefaultImage(image_index_));

  UpdateUser();
  UpdateLocalState();
  NotifyJobDone();
}

void UserImageManagerImpl::Job::SetToImage(
    int image_index,
    const user_manager::UserImage& user_image) {
  DCHECK(!run_);
  run_ = true;

  DCHECK(image_index == user_manager::User::USER_IMAGE_EXTERNAL ||
         image_index == user_manager::User::USER_IMAGE_PROFILE);

  image_index_ = image_index;
  user_image_ = user_image;

  UpdateUser();
  SaveImageAndUpdateLocalState();
}

void UserImageManagerImpl::Job::SetToImageData(scoped_ptr<std::string> data) {
  DCHECK(!run_);
  run_ = true;

  image_index_ = user_manager::User::USER_IMAGE_EXTERNAL;

  // This method uses the image_loader_, not the unsafe_image_loader_:
  // * This is necessary because the method is used to update the user image
  //   whenever the policy for a user is set. In the case of device-local
  //   accounts, policy may change at any time, even if the user is not
  //   currently logged in (and thus, the unsafe_image_loader_ may not be used).
  // * This is possible because only JPEG |data| is accepted. No support for
  //   other image file formats is needed.
  // * This is safe because the image_loader_ employs a hardened JPEG decoder
  //   that protects against malicious invalid image data being used to attack
  //   the login screen or another user session currently in progress.
  parent_->image_loader_->Start(data.Pass(),
                                login::kMaxUserImageSize,
                                base::Bind(&Job::OnLoadImageDone,
                                           weak_factory_.GetWeakPtr(),
                                           true));
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

void UserImageManagerImpl::Job::OnLoadImageDone(
    bool save,
    const user_manager::UserImage& user_image) {
  user_image_ = user_image;
  UpdateUser();
  if (save)
    SaveImageAndUpdateLocalState();
  else
    NotifyJobDone();
}

void UserImageManagerImpl::Job::UpdateUser() {
  user_manager::User* user =
      parent_->user_manager_->FindUserAndModify(user_id());
  if (!user)
    return;

  if (!user_image_.image().isNull()) {
    user->SetImage(user_image_, image_index_);
  } else {
    user->SetStubImage(
        user_manager::UserImage(
            *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                IDR_PROFILE_PICTURE_LOADING)),
        image_index_,
        false);
  }
  user->SetImageURL(image_url_);

  parent_->OnJobChangedUserImage();
}

void UserImageManagerImpl::Job::SaveImageAndUpdateLocalState() {
  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  image_path_ = user_data_dir.Append(user_id() + kSafeImagePathExtension);

  base::PostTaskAndReplyWithResult(
      parent_->background_task_runner_,
      FROM_HERE,
      base::Bind(&SaveImage, user_image_, image_path_),
      base::Bind(&Job::OnSaveImageDone, weak_factory_.GetWeakPtr()));
}

void UserImageManagerImpl::Job::OnSaveImageDone(bool success) {
  if (success || image_index_ == user_manager::User::USER_IMAGE_PROFILE)
    UpdateLocalState();
  NotifyJobDone();
}

void UserImageManagerImpl::Job::UpdateLocalState() {
  // Ignore if data stored or cached outside the user's cryptohome is to be
  // treated as ephemeral.
  if (parent_->user_manager_->IsUserNonCryptohomeDataEphemeral(user_id()))
    return;

  scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
  entry->Set(kImagePathNodeName, new base::StringValue(image_path_.value()));
  entry->Set(kImageIndexNodeName, new base::FundamentalValue(image_index_));
  if (!image_url_.is_empty())
    entry->Set(kImageURLNodeName, new base::StringValue(image_url_.spec()));
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              kUserImageProperties);
  update->SetWithoutPathExpansion(user_id(), entry.release());

  parent_->user_manager_->NotifyLocalStateChanged();
}

void UserImageManagerImpl::Job::NotifyJobDone() {
  parent_->OnJobDone();
}

UserImageManagerImpl::UserImageManagerImpl(const std::string& user_id,
                                           UserManager* user_manager)
    : UserImageManager(user_id),
      user_manager_(user_manager),
      downloading_profile_image_(false),
      profile_image_requested_(false),
      has_managed_image_(false),
      user_needs_migration_(false),
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

UserImageManagerImpl::~UserImageManagerImpl() {}

void UserImageManagerImpl::LoadUserImage() {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* prefs_images_unsafe =
      local_state->GetDictionary(kUserImages);
  const base::DictionaryValue* prefs_images =
      local_state->GetDictionary(kUserImageProperties);
  if (!prefs_images && !prefs_images_unsafe)
    return;
  user_manager::User* user = GetUserAndModify();
  bool needs_migration = false;

  // If entries are found in both |prefs_images_unsafe| and |prefs_images|,
  // |prefs_images| is honored for now but |prefs_images_unsafe| will be
  // migrated, overwriting the |prefs_images| entry, when the user logs in.
  const base::DictionaryValue* image_properties = NULL;
  if (prefs_images_unsafe) {
    needs_migration = prefs_images_unsafe->GetDictionaryWithoutPathExpansion(
        user_id(), &image_properties);
    if (needs_migration)
      user_needs_migration_ = true;
  }
  if (prefs_images) {
    prefs_images->GetDictionaryWithoutPathExpansion(user_id(),
                                                    &image_properties);
  }

  // If the user image for |user_id| is managed by policy and the policy-set
  // image is being loaded and persisted right now, let that job continue. It
  // will update the user image when done.
  if (IsUserImageManaged() && job_.get())
    return;

  if (!image_properties) {
    SetInitialUserImage();
    return;
  }

  int image_index = user_manager::User::USER_IMAGE_INVALID;
  image_properties->GetInteger(kImageIndexNodeName, &image_index);
  if (image_index >= 0 && image_index < user_manager::kDefaultImagesCount) {
    user->SetImage(
        user_manager::UserImage(user_manager::GetDefaultImage(image_index)),
        image_index);
    return;
  }

  if (image_index != user_manager::User::USER_IMAGE_EXTERNAL &&
      image_index != user_manager::User::USER_IMAGE_PROFILE) {
    NOTREACHED();
    return;
  }

  std::string image_url_string;
  image_properties->GetString(kImageURLNodeName, &image_url_string);
  GURL image_url(image_url_string);
  std::string image_path;
  image_properties->GetString(kImagePathNodeName, &image_path);

  user->SetImageURL(image_url);
  user->SetStubImage(user_manager::UserImage(
                         *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                             IDR_PROFILE_PICTURE_LOADING)),
                     image_index,
                     true);
  DCHECK(!image_path.empty() ||
         image_index == user_manager::User::USER_IMAGE_PROFILE);
  if (image_path.empty() || needs_migration) {
    // Return if either of the following is true:
    // * The profile image is to be used but has not been downloaded yet. The
    //   profile image will be downloaded after login.
    // * The image needs migration. Migration will be performed after login.
    return;
  }

  job_.reset(new Job(this));
  job_->LoadImage(base::FilePath(image_path), image_index, image_url);
}

void UserImageManagerImpl::UserLoggedIn(bool user_is_new,
                                        bool user_is_local) {
  const user_manager::User* user = GetUser();
  if (user_is_new) {
    if (!user_is_local)
      SetInitialUserImage();
  } else {
    UMA_HISTOGRAM_ENUMERATION("UserImage.LoggedIn",
                              ImageIndexToHistogramIndex(user->image_index()),
                              user_manager::kHistogramImagesCount);

    if (!IsUserImageManaged() && user_needs_migration_) {
      const base::DictionaryValue* prefs_images_unsafe =
          g_browser_process->local_state()->GetDictionary(kUserImages);
      const base::DictionaryValue* image_properties = NULL;
      if (prefs_images_unsafe->GetDictionaryWithoutPathExpansion(
              user_id(), &image_properties)) {
        std::string image_path;
        image_properties->GetString(kImagePathNodeName, &image_path);
        job_.reset(new Job(this));
        if (!image_path.empty()) {
          VLOG(0) << "Loading old user image, then migrating it.";
          job_->SetToPath(base::FilePath(image_path),
                          user->image_index(),
                          user->image_url(),
                          false);
        } else {
          job_->SetToDefaultImage(user->image_index());
        }
      }
    }
  }

  // Reset the downloaded profile image as a new user logged in.
  downloaded_profile_image_ = gfx::ImageSkia();
  profile_image_url_ = GURL();
  profile_image_requested_ = false;

  if (IsUserLoggedInAndRegular()) {
    TryToInitDownloadedProfileImage();

    // Schedule an initial download of the profile data (full name and
    // optionally image).
    profile_download_one_shot_timer_.Start(
        FROM_HERE,
        g_ignore_profile_data_download_delay_ ?
            base::TimeDelta() :
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

  user_image_sync_observer_.reset();
  TryToCreateImageSyncObserver();
}

void UserImageManagerImpl::SaveUserDefaultImageIndex(int default_image_index) {
  if (IsUserImageManaged())
    return;
  job_.reset(new Job(this));
  job_->SetToDefaultImage(default_image_index);
}

void UserImageManagerImpl::SaveUserImage(
    const user_manager::UserImage& user_image) {
  if (IsUserImageManaged())
    return;
  job_.reset(new Job(this));
  job_->SetToImage(user_manager::User::USER_IMAGE_EXTERNAL, user_image);
}

void UserImageManagerImpl::SaveUserImageFromFile(const base::FilePath& path) {
  if (IsUserImageManaged())
    return;
  job_.reset(new Job(this));
  job_->SetToPath(path, user_manager::User::USER_IMAGE_EXTERNAL, GURL(), true);
}

void UserImageManagerImpl::SaveUserImageFromProfileImage() {
  if (IsUserImageManaged())
    return;
  // Use the profile image if it has been downloaded already. Otherwise, use a
  // stub image (gray avatar).
  job_.reset(new Job(this));
  job_->SetToImage(user_manager::User::USER_IMAGE_PROFILE,
                   downloaded_profile_image_.isNull()
                       ? user_manager::UserImage()
                       : user_manager::UserImage::CreateAndEncode(
                             downloaded_profile_image_));
  // If no profile image has been downloaded yet, ensure that a download is
  // started.
  if (downloaded_profile_image_.isNull())
    DownloadProfileData(kProfileDownloadReasonProfileImageChosen);
}

void UserImageManagerImpl::DeleteUserImage() {
  job_.reset();
  DeleteUserImageAndLocalStateEntry(kUserImages);
  DeleteUserImageAndLocalStateEntry(kUserImageProperties);
}

void UserImageManagerImpl::DownloadProfileImage(const std::string& reason) {
  profile_image_requested_ = true;
  DownloadProfileData(reason);
}

const gfx::ImageSkia& UserImageManagerImpl::DownloadedProfileImage() const {
  return downloaded_profile_image_;
}

UserImageSyncObserver* UserImageManagerImpl::GetSyncObserver() const {
  return user_image_sync_observer_.get();
}

void UserImageManagerImpl::Shutdown() {
  profile_downloader_.reset();
  user_image_sync_observer_.reset();
}

void UserImageManagerImpl::OnExternalDataSet(const std::string& policy) {
  DCHECK_EQ(policy::key::kUserAvatarImage, policy);
  if (IsUserImageManaged())
    return;

  has_managed_image_ = true;
  job_.reset();

  const user_manager::User* user = GetUser();
  // If the user image for the currently logged-in user became managed, stop the
  // sync observer so that the policy-set image does not get synced out.
  if (user && user->is_logged_in())
    user_image_sync_observer_.reset();
}

void UserImageManagerImpl::OnExternalDataCleared(const std::string& policy) {
  DCHECK_EQ(policy::key::kUserAvatarImage, policy);
  has_managed_image_ = false;
  SetInitialUserImage();
  TryToCreateImageSyncObserver();
}

void UserImageManagerImpl::OnExternalDataFetched(const std::string& policy,
                                                 scoped_ptr<std::string> data) {
  DCHECK_EQ(policy::key::kUserAvatarImage, policy);
  DCHECK(IsUserImageManaged());
  if (data) {
    job_.reset(new Job(this));
    job_->SetToImageData(data.Pass());
  }
}

// static
void UserImageManagerImpl::IgnoreProfileDataDownloadDelayForTesting() {
  g_ignore_profile_data_download_delay_ = true;
}

bool UserImageManagerImpl::NeedsProfilePicture() const {
  return downloading_profile_image_;
}

int UserImageManagerImpl::GetDesiredImageSideLength() const {
  return GetCurrentUserImageSize();
}

Profile* UserImageManagerImpl::GetBrowserProfile() {
  return ProfileHelper::Get()->GetProfileByUser(GetUser());
}

std::string UserImageManagerImpl::GetCachedPictureURL() const {
  return profile_image_url_.spec();
}

void UserImageManagerImpl::OnProfileDownloadSuccess(
    ProfileDownloader* downloader) {
  // Ensure that the |profile_downloader_| is deleted when this method returns.
  scoped_ptr<ProfileDownloader> profile_downloader(
      profile_downloader_.release());
  DCHECK_EQ(downloader, profile_downloader.get());

  user_manager_->UpdateUserAccountData(
      user_id(),
      UserManager::UserAccountData(downloader->GetProfileFullName(),
                                   downloader->GetProfileGivenName(),
                                   downloader->GetProfileLocale()));
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
  } else {
    profile_image_requested_ = false;
  }

  // Nothing to do if the picture is cached or is the default avatar.
  if (result != kDownloadSuccess)
    return;

  downloaded_profile_image_ = gfx::ImageSkia::CreateFrom1xBitmap(
      downloader->GetProfilePicture());
  profile_image_url_ = GURL(downloader->GetProfilePictureURL());

  const user_manager::User* user = GetUser();
  if (user->image_index() == user_manager::User::USER_IMAGE_PROFILE) {
    VLOG(1) << "Updating profile image for logged-in user.";
    UMA_HISTOGRAM_ENUMERATION("UserImage.ProfileDownloadResult",
                              kDownloadSuccessChanged,
                              kDownloadResultsCount);
    // This will persist |downloaded_profile_image_| to disk.
    SaveUserImageFromProfileImage();
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

bool UserImageManagerImpl::IsUserImageManaged() const {
  return has_managed_image_;
}

void UserImageManagerImpl::SetInitialUserImage() {
  // Choose a random default image.
  SaveUserDefaultImageIndex(
      base::RandInt(user_manager::kFirstDefaultImageIndex,
                    user_manager::kDefaultImagesCount - 1));
}

void UserImageManagerImpl::TryToInitDownloadedProfileImage() {
  const user_manager::User* user = GetUser();
  if (user->image_index() == user_manager::User::USER_IMAGE_PROFILE &&
      downloaded_profile_image_.isNull() && !user->image_is_stub()) {
    // Initialize the |downloaded_profile_image_| for the currently logged-in
    // user if it has not been initialized already, the user image is the
    // profile image and the user image has been loaded successfully.
    VLOG(1) << "Profile image initialized from disk.";
    downloaded_profile_image_ = user->GetImage();
    profile_image_url_ = user->image_url();
  }
}

bool UserImageManagerImpl::NeedProfileImage() const {
  const user_manager::User* user = GetUser();
  return IsUserLoggedInAndRegular() &&
         (user->image_index() == user_manager::User::USER_IMAGE_PROFILE ||
          profile_image_requested_);
}

void UserImageManagerImpl::DownloadProfileData(const std::string& reason) {
  // GAIA profiles exist for regular users only.
  if (!IsUserLoggedInAndRegular())
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

void UserImageManagerImpl::DeleteUserImageAndLocalStateEntry(
    const char* prefs_dict_root) {
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              prefs_dict_root);
  const base::DictionaryValue* image_properties;
  if (!update->GetDictionaryWithoutPathExpansion(user_id(), &image_properties))
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
  update->RemoveWithoutPathExpansion(user_id(), NULL);
}

void UserImageManagerImpl::OnJobChangedUserImage() {
  if (GetUser()->is_logged_in())
    TryToInitDownloadedProfileImage();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
      content::Source<UserImageManagerImpl>(this),
      content::Details<const user_manager::User>(GetUser()));
}

void UserImageManagerImpl::OnJobDone() {
  if (job_.get())
    base::MessageLoopProxy::current()->DeleteSoon(FROM_HERE, job_.release());
  else
    NOTREACHED();

  if (!user_needs_migration_)
    return;
  // Migration completed for |user_id|.
  user_needs_migration_ = false;

  const base::DictionaryValue* prefs_images_unsafe =
      g_browser_process->local_state()->GetDictionary(kUserImages);
  const base::DictionaryValue* image_properties = NULL;
  if (!prefs_images_unsafe->GetDictionaryWithoutPathExpansion(
          user_id(), &image_properties)) {
    NOTREACHED();
    return;
  }

  int image_index = user_manager::User::USER_IMAGE_INVALID;
  image_properties->GetInteger(kImageIndexNodeName, &image_index);
  UMA_HISTOGRAM_ENUMERATION("UserImage.Migration",
                            ImageIndexToHistogramIndex(image_index),
                            user_manager::kHistogramImagesCount);

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
                   weak_factory_.GetWeakPtr()));
  } else {
    // If no old image exists, remove |user_id| from the old prefs dictionary.
    UpdateLocalStateAfterMigration();
  }
}

void UserImageManagerImpl::UpdateLocalStateAfterMigration() {
  DictionaryPrefUpdate update(g_browser_process->local_state(),
                              kUserImages);
  update->RemoveWithoutPathExpansion(user_id(), NULL);
}

void UserImageManagerImpl::TryToCreateImageSyncObserver() {
  const user_manager::User* user = GetUser();
  // If the currently logged-in user's user image is managed, the sync observer
  // must not be started so that the policy-set image does not get synced out.
  if (!user_image_sync_observer_ &&
      user && user->CanSyncImage() &&
      !IsUserImageManaged()) {
    user_image_sync_observer_.reset(new UserImageSyncObserver(user));
  }
}

const user_manager::User* UserImageManagerImpl::GetUser() const {
  return user_manager_->FindUser(user_id());
}

user_manager::User* UserImageManagerImpl::GetUserAndModify() const {
  return user_manager_->FindUserAndModify(user_id());
}

bool UserImageManagerImpl::IsUserLoggedInAndRegular() const {
  const user_manager::User* user = GetUser();
  if (!user)
    return false;
  return user->is_logged_in() &&
         user->GetType() == user_manager::USER_TYPE_REGULAR;
}

}  // namespace chromeos
