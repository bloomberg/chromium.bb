// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"

#include <utility>

#include "ash/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_decoder.h"
#include "ash/wallpaper/wallpaper_window_state_manager.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/extensions/wallpaper_manager_util.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "components/wallpaper/wallpaper_files_id.h"
#include "components/wallpaper/wallpaper_resizer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "crypto/sha2.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"

using content::BrowserThread;
using wallpaper::WallpaperInfo;

namespace chromeos {

namespace {

WallpaperManager* wallpaper_manager = nullptr;

// The Wallpaper App that the user is using right now on Chrome OS. It's the
// app that is used when the user right clicks on desktop and selects "Set
// wallpaper" or when the user selects "Set wallpaper" from chrome://settings
// page. This is recorded at user login. It's used to back an UMA histogram so
// it should be treated as append-only.
enum WallpaperAppsType {
  WALLPAPERS_PICKER_APP_CHROMEOS = 0,
  WALLPAPERS_APP_ANDROID = 1,
  WALLPAPERS_APPS_NUM = 2,
};

// These global default values are used to set customized default
// wallpaper path in WallpaperManager::InitializeWallpaper().
base::FilePath GetCustomizedWallpaperDefaultRescaledFileName(
    const std::string& suffix) {
  const base::FilePath default_downloaded_file_name =
      ServicesCustomizationDocument::GetCustomizedWallpaperDownloadedFileName();
  const base::FilePath default_cache_dir =
      ServicesCustomizationDocument::GetCustomizedWallpaperCacheDir();
  if (default_downloaded_file_name.empty() || default_cache_dir.empty())
    return base::FilePath();
  return default_cache_dir.Append(
      default_downloaded_file_name.BaseName().value() + suffix);
}

// Whether WallpaperController should start with customized default
// wallpaper in WallpaperManager::InitializeWallpaper() or not.
bool ShouldUseCustomizedDefaultWallpaper() {
  PrefService* pref_service = g_browser_process->local_state();

  return !pref_service->FindPreference(
      prefs::kCustomizationDefaultWallpaperURL)->IsDefaultValue();
}

// Returns index of the first public session user found in |users|
// or -1 otherwise.
int FindPublicSession(const user_manager::UserList& users) {
  int index = -1;
  int i = 0;
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end(); ++it, ++i) {
    if ((*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
      index = i;
      break;
    }
  }

  return index;
}

// Returns true if |users| contains users other than device local account users.
bool HasNonDeviceLocalAccounts(const user_manager::UserList& users) {
  for (const auto* user : users) {
    if (!policy::IsDeviceLocalAccountUser(user->GetAccountId().GetUserEmail(),
                                          nullptr))
      return true;
  }
  return false;
}

// Call |closure| when HashWallpaperFilesIdStr will not assert().
void CallWhenCanGetFilesId(const base::Closure& closure) {
  SystemSaltGetter::Get()->AddOnSystemSaltReady(closure);
}

// A helper to set the wallpaper image for Classic Ash and Mash.
void SetWallpaper(const gfx::ImageSkia& image, wallpaper::WallpaperInfo info) {
  if (ash_util::IsRunningInMash()) {
    // In mash, connect to the WallpaperController interface via mojo.
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    if (!connector)
      return;

    ash::mojom::WallpaperControllerPtr wallpaper_controller;
    connector->BindInterface(ash::mojom::kServiceName, &wallpaper_controller);
    // TODO(crbug.com/655875): Optimize ash wallpaper transport; avoid sending
    // large bitmaps over Mojo; use shared memory like BitmapUploader, etc.
    wallpaper_controller->SetWallpaper(*image.bitmap(), info);
  } else if (ash::Shell::HasInstance()) {
    // Note: Wallpaper setting is skipped in unit tests without shell instances.
    // In classic ash, interact with the WallpaperController class directly.
    ash::Shell::Get()->wallpaper_controller()->SetWallpaperImage(image, info);
  }
}

// A helper function to check the existing/downloaded device wallpaper file's
// hash value matches with the hash value provided in the policy settings.
bool CheckDeviceWallpaperMatchHash(const base::FilePath& device_wallpaper_file,
                                   const std::string& hash) {
  std::string image_data;
  if (base::ReadFileToString(device_wallpaper_file, &image_data)) {
    std::string sha_hash = crypto::SHA256HashString(image_data);
    if (base::ToLowerASCII(base::HexEncode(
            sha_hash.c_str(), sha_hash.size())) == base::ToLowerASCII(hash)) {
      return true;
    }
  }
  return false;
}

}  // namespace

void AssertCalledOnWallpaperSequence(base::SequencedTaskRunner* task_runner) {
#if DCHECK_IS_ON()
  DCHECK(task_runner->RunsTasksInCurrentSequence());
#endif
}

const char kWallpaperSequenceTokenName[] = "wallpaper-sequence";

// CustomizedWallpaperRescaledFiles:
const base::FilePath&
WallpaperManager::CustomizedWallpaperRescaledFiles::path_downloaded() const {
  return path_downloaded_;
}

const base::FilePath&
WallpaperManager::CustomizedWallpaperRescaledFiles::path_rescaled_small()
    const {
  return path_rescaled_small_;
}

const base::FilePath&
WallpaperManager::CustomizedWallpaperRescaledFiles::path_rescaled_large()
    const {
  return path_rescaled_large_;
}

bool WallpaperManager::CustomizedWallpaperRescaledFiles::downloaded_exists()
    const {
  return downloaded_exists_;
}

bool WallpaperManager::CustomizedWallpaperRescaledFiles::rescaled_small_exists()
    const {
  return rescaled_small_exists_;
}

bool WallpaperManager::CustomizedWallpaperRescaledFiles::rescaled_large_exists()
    const {
  return rescaled_large_exists_;
}

WallpaperManager::CustomizedWallpaperRescaledFiles::
    CustomizedWallpaperRescaledFiles(const base::FilePath& path_downloaded,
                                     const base::FilePath& path_rescaled_small,
                                     const base::FilePath& path_rescaled_large)
    : path_downloaded_(path_downloaded),
      path_rescaled_small_(path_rescaled_small),
      path_rescaled_large_(path_rescaled_large),
      downloaded_exists_(false),
      rescaled_small_exists_(false),
      rescaled_large_exists_(false) {}

base::Closure
WallpaperManager::CustomizedWallpaperRescaledFiles::CreateCheckerClosure() {
  return base::Bind(&WallpaperManager::CustomizedWallpaperRescaledFiles::
                        CheckCustomizedWallpaperFilesExist,
                    base::Unretained(this));
}

void WallpaperManager::CustomizedWallpaperRescaledFiles::
    CheckCustomizedWallpaperFilesExist() {
  downloaded_exists_ = base::PathExists(path_downloaded_);
  rescaled_small_exists_ = base::PathExists(path_rescaled_small_);
  rescaled_large_exists_ = base::PathExists(path_rescaled_large_);
}

bool WallpaperManager::CustomizedWallpaperRescaledFiles::AllSizesExist() const {
  return rescaled_small_exists_ && rescaled_large_exists_;
}

// TestApi:
WallpaperManager::TestApi::TestApi(WallpaperManager* wallpaper_manager)
    : wallpaper_manager_(wallpaper_manager) {}

WallpaperManager::TestApi::~TestApi() {}

bool WallpaperManager::TestApi::GetWallpaperFromCache(
    const AccountId& account_id,
    gfx::ImageSkia* image) {
  return wallpaper_manager_->GetWallpaperFromCache(account_id, image);
}

bool WallpaperManager::TestApi::GetPathFromCache(const AccountId& account_id,
                                                 base::FilePath* path) {
  return wallpaper_manager_->GetPathFromCache(account_id, path);
}

void WallpaperManager::TestApi::SetWallpaperCache(const AccountId& account_id,
                                                  const base::FilePath& path,
                                                  const gfx::ImageSkia& image) {
  DCHECK(!image.isNull());
  (*wallpaper_manager_->GetWallpaperCacheMap())[account_id] =
      ash::CustomWallpaperElement(path, image);
}

// WallpaperManager, public: ---------------------------------------------------

WallpaperManager::~WallpaperManager() {
  show_user_name_on_signin_subscription_.reset();
  device_wallpaper_image_subscription_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

// static
void WallpaperManager::Initialize() {
  CHECK(!wallpaper_manager);
  wallpaper_manager = new WallpaperManager();
}

// static
WallpaperManager* WallpaperManager::Get() {
  DCHECK(wallpaper_manager);
  return wallpaper_manager;
}

// static
void WallpaperManager::Shutdown() {
  CHECK(wallpaper_manager);
  delete wallpaper_manager;
  wallpaper_manager = nullptr;
}

void WallpaperManager::SetCustomizedDefaultWallpaper(
    const GURL& wallpaper_url,
    const base::FilePath& file_path,
    const base::FilePath& resized_directory) {
  // Should fail if this ever happens in tests.
  DCHECK(wallpaper_url.is_valid());
  if (!wallpaper_url.is_valid()) {
    if (!wallpaper_url.is_empty()) {
      LOG(WARNING) << "Invalid Customized Wallpaper URL '"
                   << wallpaper_url.spec() << "'";
    }
    return;
  }
  std::string downloaded_file_name = file_path.BaseName().value();
  std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files(
      new CustomizedWallpaperRescaledFiles(
          file_path,
          resized_directory.Append(
              downloaded_file_name +
              ash::WallpaperController::kSmallWallpaperSuffix),
          resized_directory.Append(
              downloaded_file_name +
              ash::WallpaperController::kLargeWallpaperSuffix)));

  base::Closure check_file_exists = rescaled_files->CreateCheckerClosure();
  base::Closure on_checked_closure =
      base::Bind(&WallpaperManager::SetCustomizedDefaultWallpaperAfterCheck,
                 weak_factory_.GetWeakPtr(), wallpaper_url, file_path,
                 base::Passed(std::move(rescaled_files)));
  if (!task_runner_->PostTaskAndReply(FROM_HERE, check_file_exists,
                                      on_checked_closure)) {
    LOG(WARNING) << "Failed to start check CheckCustomizedWallpaperFilesExist.";
  }
}

void WallpaperManager::ShowUserWallpaper(const AccountId& account_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Some unit tests come here without a UserManager or without a pref system.
  if (!user_manager::UserManager::IsInitialized() ||
      !g_browser_process->local_state()) {
    return;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);

  // User is unknown or there is no visible wallpaper in kiosk mode.
  if (!user || user->GetType() == user_manager::USER_TYPE_KIOSK_APP ||
      user->GetType() == user_manager::USER_TYPE_ARC_KIOSK_APP) {
    return;
  }

  // For a enterprise managed user, set the device wallpaper if we're at the
  // login screen.
  if (!user_manager::UserManager::Get()->IsUserLoggedIn() &&
      SetDeviceWallpaperIfApplicable(account_id))
    return;

  // TODO(crbug.com/776464): Move the above to
  // |WallpaperController::ShowUserWallpaper| after
  // |SetDeviceWallpaperIfApplicable| is migrated.
  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);
}

void WallpaperManager::ShowSigninWallpaper() {
  if (SetDeviceWallpaperIfApplicable(user_manager::SignInAccountId()))
    return;
  WallpaperControllerClient::Get()->ShowSigninWallpaper();
}

void WallpaperManager::SetUserWallpaperInfo(const AccountId& account_id,
                                            const WallpaperInfo& info,
                                            bool is_persistent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return;
  }
  ash::Shell::Get()->wallpaper_controller()->SetUserWallpaperInfo(
      account_id, info, is_persistent);
}

void WallpaperManager::InitializeWallpaper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Apply device customization.
  if (ShouldUseCustomizedDefaultWallpaper()) {
    SetCustomizedDefaultWallpaperImpl(
        GetCustomizedWallpaperDefaultRescaledFileName(
            ash::WallpaperController::kSmallWallpaperSuffix),
        std::unique_ptr<gfx::ImageSkia>(),
        GetCustomizedWallpaperDefaultRescaledFileName(
            ash::WallpaperController::kLargeWallpaperSuffix),
        std::unique_ptr<gfx::ImageSkia>());
  }

  base::CommandLine* command_line = GetCommandLine();
  if (command_line->HasSwitch(chromeos::switches::kGuestSession)) {
    // Guest wallpaper should be initialized when guest login.
    // Note: This maybe called before login. So IsLoggedInAsGuest can not be
    // used here to determine if current user is guest.
    return;
  }

  if (command_line->HasSwitch(::switches::kTestType))
    WizardController::SetZeroDelays();

  // Zero delays is also set in autotests.
  if (WizardController::IsZeroDelayEnabled()) {
    // Ensure tests have some sort of wallpaper.
    ash::Shell::Get()->wallpaper_controller()->CreateEmptyWallpaper();
    return;
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsUserLoggedIn()) {
    if (!StartupUtils::IsDeviceRegistered())
      ShowSigninWallpaper();
    else
      InitializeRegisteredDeviceWallpaper();
    return;
  }
  ShowUserWallpaper(user_manager->GetActiveUser()->GetAccountId());
}

void WallpaperManager::UpdateWallpaper(bool clear_cache) {
  for (auto& observer : observers_)
    observer.OnUpdateWallpaperForTesting();
  if (clear_cache)
    GetWallpaperCacheMap()->clear();

  // For GAIA login flow, |current_user_| may not be set before user login. If
  // UpdateWallpaper is called at GAIA login screen, no wallpaper will be set.
  // It could result a black screen on external monitors.
  // See http://crbug.com/265689 for detail.
  AccountId current_user_account_id = GetCurrentUserAccountId();
  if (current_user_account_id.empty())
    ShowSigninWallpaper();
  else
    ShowUserWallpaper(current_user_account_id);
}

bool WallpaperManager::GetLoggedInUserWallpaperInfo(WallpaperInfo* info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (user_manager::UserManager::Get()->IsLoggedInAsStub()) {
    info->location = "";
    info->layout = wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
    info->type = wallpaper::DEFAULT;
    info->date = base::Time::Now().LocalMidnight();
    *GetCachedWallpaperInfo() = *info;
    return true;
  }

  return GetUserWallpaperInfo(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(), info);
}

void WallpaperManager::AddObservers() {
  show_user_name_on_signin_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kAccountsPrefShowUserNamesOnSignIn,
          base::Bind(&WallpaperManager::InitializeRegisteredDeviceWallpaper,
                     weak_factory_.GetWeakPtr()));
  device_wallpaper_image_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kDeviceWallpaperImage,
          base::Bind(&WallpaperManager::OnDeviceWallpaperPolicyChanged,
                     weak_factory_.GetWeakPtr()));
}

void WallpaperManager::EnsureLoggedInUserWallpaperLoaded() {
  WallpaperInfo info;
  if (GetLoggedInUserWallpaperInfo(&info)) {
    UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Type", info.type,
                              wallpaper::WALLPAPER_TYPE_COUNT);
    RecordWallpaperAppType();
    if (info == *GetCachedWallpaperInfo())
      return;
  }
  ShowUserWallpaper(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
}

void WallpaperManager::OnPolicyFetched(const std::string& policy,
                                       const AccountId& account_id,
                                       std::unique_ptr<std::string> data) {
  if (!data)
    return;

  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    user_image_loader::StartWithData(
        task_runner_, std::move(data), ImageDecoder::ROBUST_JPEG_CODEC,
        0,  // Do not crop.
        base::Bind(&WallpaperManager::SetPolicyControlledWallpaper,
                   weak_factory_.GetWeakPtr(), account_id));
  } else {
    ash::WallpaperController::DecodeWallpaperIfApplicable(
        base::Bind(&WallpaperManager::SetPolicyControlledWallpaper,
                   weak_factory_.GetWeakPtr(), account_id),
        std::move(data), true /* data_is_ready */);
  }
}

bool WallpaperManager::IsPolicyControlled(const AccountId& account_id) const {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return false;
  }
  bool is_persistent =
      !user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          account_id);
  return ash::Shell::Get()->wallpaper_controller()->IsPolicyControlled(
      account_id, is_persistent);
}

void WallpaperManager::OnPolicySet(const std::string& policy,
                                   const AccountId& account_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(account_id, &info);
  info.type = wallpaper::POLICY;
  SetUserWallpaperInfo(account_id, info, true /*is_persistent=*/);
}

void WallpaperManager::OnPolicyCleared(const std::string& policy,
                                       const AccountId& account_id) {
  WallpaperInfo info;
  GetUserWallpaperInfo(account_id, &info);
  info.type = wallpaper::DEFAULT;
  SetUserWallpaperInfo(account_id, info, true /*is_persistent=*/);

  // If we're at the login screen, do not change the wallpaper but defer it
  // until the user logs in to the system.
  if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
    SetDefaultWallpaperImpl(account_id, true /*show_wallpaper=*/);
  }
}

void WallpaperManager::AddObserver(WallpaperManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void WallpaperManager::RemoveObserver(WallpaperManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WallpaperManager::OpenWallpaperPicker() {
  if (wallpaper_manager_util::ShouldUseAndroidWallpapersApp(
          ProfileHelper::Get()->GetProfileByUser(
              user_manager::UserManager::Get()->GetActiveUser())) &&
      !ash_util::IsRunningInMash()) {
    // Window activation watch to minimize all inactive windows is only needed
    // by Android Wallpaper app. Legacy Chrome OS Wallpaper Picker app does that
    // via extension API.
    activation_client_observer_.Add(ash::Shell::Get()->activation_client());
  }
  wallpaper_manager_util::OpenWallpaperManager();
}

void WallpaperManager::OnWindowActivated(ActivationReason reason,
                                         aura::Window* gained_active,
                                         aura::Window* lost_active) {
  if (!gained_active)
    return;

  const std::string arc_wallpapers_app_id = ArcAppListPrefs::GetAppId(
      wallpaper_manager_util::kAndroidWallpapersAppPackage,
      wallpaper_manager_util::kAndroidWallpapersAppActivity);
  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(gained_active->GetProperty(ash::kShelfIDKey));
  if (shelf_id.app_id == arc_wallpapers_app_id) {
    ash::WallpaperWindowStateManager::MinimizeInactiveWindows(
        user_manager::UserManager::Get()->GetActiveUser()->username_hash());
    DCHECK(!ash_util::IsRunningInMash() && ash::Shell::Get());
    activation_client_observer_.Remove(ash::Shell::Get()->activation_client());
    window_observer_.Add(gained_active);
  }
}

void WallpaperManager::OnWindowDestroying(aura::Window* window) {
  window_observer_.Remove(window);
  ash::WallpaperWindowStateManager::RestoreWindows(
      user_manager::UserManager::Get()->GetActiveUser()->username_hash());
}

// WallpaperManager, private: --------------------------------------------------

WallpaperManager::WallpaperManager()
    : activation_client_observer_(this),
      window_observer_(this),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
}

void WallpaperManager::ResizeCustomizedDefaultWallpaper(
    std::unique_ptr<gfx::ImageSkia> image,
    const CustomizedWallpaperRescaledFiles* rescaled_files,
    bool* success,
    gfx::ImageSkia* small_wallpaper_image,
    gfx::ImageSkia* large_wallpaper_image) {
  *success = true;

  *success &= ash::WallpaperController::ResizeAndSaveWallpaper(
      *image, rescaled_files->path_rescaled_small(),
      wallpaper::WALLPAPER_LAYOUT_STRETCH,
      ash::WallpaperController::kSmallWallpaperMaxWidth,
      ash::WallpaperController::kSmallWallpaperMaxHeight,
      small_wallpaper_image);

  *success &= ash::WallpaperController::ResizeAndSaveWallpaper(
      *image, rescaled_files->path_rescaled_large(),
      wallpaper::WALLPAPER_LAYOUT_STRETCH,
      ash::WallpaperController::kLargeWallpaperMaxWidth,
      ash::WallpaperController::kLargeWallpaperMaxHeight,
      large_wallpaper_image);
}

void WallpaperManager::InitializeUserWallpaperInfo(
    const AccountId& account_id) {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return;
  }
  bool is_persistent =
      !user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          account_id);
  ash::Shell::Get()->wallpaper_controller()->InitializeUserWallpaperInfo(
      account_id, is_persistent);
}

bool WallpaperManager::SetDeviceWallpaperIfApplicable(
    const AccountId& account_id) {
  std::string url;
  std::string hash;
  if (ShouldSetDeviceWallpaper(account_id, &url, &hash)) {
    // Check if the device wallpaper exists and matches the hash. If so, use it
    // directly. Otherwise download it first.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::Bind(&base::PathExists,
                   ash::WallpaperController::GetDeviceWallpaperFilePath()),
        base::Bind(&WallpaperManager::OnDeviceWallpaperExists,
                   weak_factory_.GetWeakPtr(), account_id, url, hash));
    return true;
  }
  return false;
}

bool WallpaperManager::GetWallpaperFromCache(const AccountId& account_id,
                                             gfx::ImageSkia* image) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return false;
  }
  return ash::Shell::Get()->wallpaper_controller()->GetWallpaperFromCache(
      account_id, image);
}

bool WallpaperManager::GetPathFromCache(const AccountId& account_id,
                                        base::FilePath* path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return false;
  }
  return ash::Shell::Get()->wallpaper_controller()->GetPathFromCache(account_id,
                                                                     path);
}

int WallpaperManager::loaded_wallpapers_for_test() const {
  return loaded_wallpapers_for_test_;
}

base::CommandLine* WallpaperManager::GetCommandLine() {
  base::CommandLine* command_line =
      command_line_for_testing_ ? command_line_for_testing_
                                : base::CommandLine::ForCurrentProcess();
  return command_line;
}

void WallpaperManager::InitializeRegisteredDeviceWallpaper() {
  if (user_manager::UserManager::Get()->IsUserLoggedIn())
    return;

  bool disable_boot_animation =
      GetCommandLine()->HasSwitch(switches::kDisableBootAnimation);
  bool show_users = true;
  bool result = CrosSettings::Get()->GetBoolean(
      kAccountsPrefShowUserNamesOnSignIn, &show_users);
  DCHECK(result) << "Unable to fetch setting "
                 << kAccountsPrefShowUserNamesOnSignIn;
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();
  int public_session_user_index = FindPublicSession(users);
  if ((!show_users && public_session_user_index == -1) ||
      !HasNonDeviceLocalAccounts(users)) {
    // Boot into the sign in screen.
    ShowSigninWallpaper();
    return;
  }

  if (!disable_boot_animation) {
    int index = public_session_user_index != -1 ? public_session_user_index : 0;
    // Normal boot, load user wallpaper.
    // If normal boot animation is disabled wallpaper would be set
    // asynchronously once user pods are loaded.
    ShowUserWallpaper(users[index]->GetAccountId());
  }
}

bool WallpaperManager::GetUserWallpaperInfo(const AccountId& account_id,
                                            WallpaperInfo* info) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    return false;
  }
  bool is_persistent =
      !user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          account_id);
  return ash::Shell::Get()->wallpaper_controller()->GetUserWallpaperInfo(
      account_id, info, is_persistent);
}

bool WallpaperManager::ShouldSetDeviceWallpaper(const AccountId& account_id,
                                                std::string* url,
                                                std::string* hash) {
  // Only allow the device wallpaper for enterprise managed devices.
  if (!g_browser_process->platform_part()
           ->browser_policy_connector_chromeos()
           ->IsEnterpriseManaged()) {
    return false;
  }

  const base::DictionaryValue* dict = nullptr;
  if (!CrosSettings::Get()->GetDictionary(kDeviceWallpaperImage, &dict) ||
      !dict->GetStringWithoutPathExpansion("url", url) ||
      !dict->GetStringWithoutPathExpansion("hash", hash)) {
    return false;
  }

  // Only set the device wallpaper if we're at the login screen.
  if (user_manager::UserManager::Get()->IsUserLoggedIn())
    return false;

  return true;
}

void WallpaperManager::SetDefaultWallpaperImpl(const AccountId& account_id,
                                               bool show_wallpaper) {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    // Some unit tests come here without a Shell instance.
    // TODO(crbug.com/776464): This is intended not to work under mash. Make it
    // work again after WallpaperManager is removed.
    WallpaperInfo info("", wallpaper::WALLPAPER_LAYOUT_STRETCH,
                       wallpaper::DEFAULT, base::Time::Now().LocalMidnight());
    SetWallpaper(ash::WallpaperController::CreateSolidColorWallpaper(), info);
    return;
  }
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user) {
    LOG(ERROR) << "User of " << account_id
               << " cannot be found. This should never happen"
               << " within WallpaperManager::SetDefaultWallpaperImpl.";
    return;
  }
  ash::Shell::Get()->wallpaper_controller()->SetDefaultWallpaperImpl(
      account_id, user->GetType(), show_wallpaper);
}

void WallpaperManager::NotifyAnimationFinished() {
  for (auto& observer : observers_)
    observer.OnWallpaperAnimationFinished(GetCurrentUserAccountId());
}

void WallpaperManager::SetCustomizedDefaultWallpaperAfterCheck(
    const GURL& wallpaper_url,
    const base::FilePath& file_path,
    std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files) {
  PrefService* pref_service = g_browser_process->local_state();

  std::string current_url =
      pref_service->GetString(prefs::kCustomizationDefaultWallpaperURL);
  if (current_url != wallpaper_url.spec() || !rescaled_files->AllSizesExist()) {
    DCHECK(rescaled_files->downloaded_exists());

    // Either resized images do not exist or cached version is incorrect.
    // Need to start decoding again.
    if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
      user_image_loader::StartWithFilePath(
          task_runner_, file_path, ImageDecoder::ROBUST_JPEG_CODEC,
          0,  // Do not crop.
          base::Bind(&WallpaperManager::OnCustomizedDefaultWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), wallpaper_url,
                     base::Passed(std::move(rescaled_files))));
    } else {
      ash::Shell::Get()->wallpaper_controller()->ReadAndDecodeWallpaper(
          base::Bind(&WallpaperManager::OnCustomizedDefaultWallpaperDecoded,
                     weak_factory_.GetWeakPtr(), wallpaper_url,
                     base::Passed(std::move(rescaled_files))),
          task_runner_, file_path);
    }
  } else {
    SetCustomizedDefaultWallpaperImpl(rescaled_files->path_rescaled_small(),
                                      std::unique_ptr<gfx::ImageSkia>(),
                                      rescaled_files->path_rescaled_large(),
                                      std::unique_ptr<gfx::ImageSkia>());
  }
}

void WallpaperManager::OnCustomizedDefaultWallpaperDecoded(
    const GURL& wallpaper_url,
    std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
    std::unique_ptr<user_manager::UserImage> wallpaper) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If decoded wallpaper is empty, we have probably failed to decode the file.
  if (wallpaper->image().isNull()) {
    LOG(WARNING) << "Failed to decode customized wallpaper.";
    return;
  }

  wallpaper->image().EnsureRepsForSupportedScales();
  // TODO(crbug.com/593251): DeepCopy() may be unnecessary as this function
  // owns |wallpaper| as scoped_ptr whereas it used to be a const reference.
  std::unique_ptr<gfx::ImageSkia> deep_copy(wallpaper->image().DeepCopy());

  std::unique_ptr<bool> success(new bool(false));
  std::unique_ptr<gfx::ImageSkia> small_wallpaper_image(new gfx::ImageSkia);
  std::unique_ptr<gfx::ImageSkia> large_wallpaper_image(new gfx::ImageSkia);

  base::Closure resize_closure = base::Bind(
      &WallpaperManager::ResizeCustomizedDefaultWallpaper,
      base::Passed(&deep_copy), base::Unretained(rescaled_files.get()),
      base::Unretained(success.get()),
      base::Unretained(small_wallpaper_image.get()),
      base::Unretained(large_wallpaper_image.get()));
  base::Closure on_resized_closure = base::Bind(
      &WallpaperManager::OnCustomizedDefaultWallpaperResized,
      weak_factory_.GetWeakPtr(), wallpaper_url,
      base::Passed(std::move(rescaled_files)), base::Passed(std::move(success)),
      base::Passed(std::move(small_wallpaper_image)),
      base::Passed(std::move(large_wallpaper_image)));

  if (!task_runner_->PostTaskAndReply(FROM_HERE, resize_closure,
                                      on_resized_closure)) {
    LOG(WARNING) << "Failed to start Customized Wallpaper resize.";
  }
}

void WallpaperManager::OnCustomizedDefaultWallpaperResized(
    const GURL& wallpaper_url,
    std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
    std::unique_ptr<bool> success,
    std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
    std::unique_ptr<gfx::ImageSkia> large_wallpaper_image) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(rescaled_files);
  DCHECK(success.get());
  if (!*success) {
    LOG(WARNING) << "Failed to save resized customized default wallpaper";
    return;
  }
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(prefs::kCustomizationDefaultWallpaperURL,
                          wallpaper_url.spec());
  SetCustomizedDefaultWallpaperImpl(
      rescaled_files->path_rescaled_small(), std::move(small_wallpaper_image),
      rescaled_files->path_rescaled_large(), std::move(large_wallpaper_image));
  VLOG(1) << "Customized default wallpaper applied.";
}

void WallpaperManager::RecordWallpaperAppType() {
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);

  UMA_HISTOGRAM_ENUMERATION(
      "Ash.Wallpaper.Apps",
      wallpaper_manager_util::ShouldUseAndroidWallpapersApp(profile)
          ? WALLPAPERS_APP_ANDROID
          : WALLPAPERS_PICKER_APP_CHROMEOS,
      WALLPAPERS_APPS_NUM);
}

const char* WallpaperManager::GetCustomWallpaperSubdirForCurrentResolution() {
  ash::WallpaperController::WallpaperResolution resolution =
      ash::WallpaperController::GetAppropriateResolution();
  return resolution == ash::WallpaperController::WALLPAPER_RESOLUTION_SMALL
             ? ash::WallpaperController::kSmallWallpaperSubDir
             : ash::WallpaperController::kLargeWallpaperSubDir;
}

void WallpaperManager::SetPolicyControlledWallpaper(
    const AccountId& account_id,
    std::unique_ptr<user_manager::UserImage> user_image) {
  if (!WallpaperControllerClient::Get()->CanGetWallpaperFilesId()) {
    CallWhenCanGetFilesId(
        base::Bind(&WallpaperManager::SetPolicyControlledWallpaper,
                   weak_factory_.GetWeakPtr(), account_id,
                   base::Passed(std::move(user_image))));
    return;
  }

  const wallpaper::WallpaperFilesId wallpaper_files_id =
      WallpaperControllerClient::Get()->GetFilesId(account_id);

  if (!wallpaper_files_id.is_valid())
    LOG(FATAL) << "Wallpaper flies id if invalid!";

  // If we're at the login screen, do not change the wallpaper to the user
  // policy controlled wallpaper but only update the cache. It will be later
  // updated after the user logs in.
  WallpaperControllerClient::Get()->SetCustomWallpaper(
      account_id, wallpaper_files_id, "policy-controlled.jpeg",
      wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED, wallpaper::POLICY,
      user_image->image(),
      user_manager::UserManager::Get()
          ->IsUserLoggedIn() /* update wallpaper */);
}

void WallpaperManager::OnDeviceWallpaperPolicyChanged() {
  SetDeviceWallpaperIfApplicable(
      user_manager::UserManager::Get()->IsUserLoggedIn()
          ? user_manager::UserManager::Get()->GetActiveUser()->GetAccountId()
          : user_manager::SignInAccountId());
}

void WallpaperManager::OnDeviceWallpaperExists(const AccountId& account_id,
                                               const std::string& url,
                                               const std::string& hash,
                                               bool exist) {
  if (exist) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::Bind(&CheckDeviceWallpaperMatchHash,
                   ash::WallpaperController::GetDeviceWallpaperFilePath(),
                   hash),
        base::Bind(&WallpaperManager::OnCheckDeviceWallpaperMatchHash,
                   weak_factory_.GetWeakPtr(), account_id, url, hash));
  } else {
    GURL device_wallpaper_url(url);
    device_wallpaper_downloader_.reset(new CustomizationWallpaperDownloader(
        g_browser_process->system_request_context(), device_wallpaper_url,
        ash::WallpaperController::GetDeviceWallpaperDir(),
        ash::WallpaperController::GetDeviceWallpaperFilePath(),
        base::Bind(&WallpaperManager::OnDeviceWallpaperDownloaded,
                   weak_factory_.GetWeakPtr(), account_id, hash)));
    device_wallpaper_downloader_->Start();
  }
}

void WallpaperManager::OnDeviceWallpaperDownloaded(const AccountId& account_id,
                                                   const std::string& hash,
                                                   bool success,
                                                   const GURL& url) {
  if (!success) {
    LOG(ERROR) << "Failed to download the device wallpaper. Fallback to "
                  "default wallpaper.";
    SetDefaultWallpaperImpl(account_id, true /*show_wallpaper=*/);
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::Bind(&CheckDeviceWallpaperMatchHash,
                 ash::WallpaperController::GetDeviceWallpaperFilePath(), hash),
      base::Bind(&WallpaperManager::OnCheckDeviceWallpaperMatchHash,
                 weak_factory_.GetWeakPtr(), account_id, url.spec(), hash));
}

void WallpaperManager::OnCheckDeviceWallpaperMatchHash(
    const AccountId& account_id,
    const std::string& url,
    const std::string& hash,
    bool match) {
  if (!match) {
    if (retry_download_if_failed_) {
      // We only retry to download the device wallpaper one more time if the
      // hash doesn't match.
      retry_download_if_failed_ = false;
      GURL device_wallpaper_url(url);
      device_wallpaper_downloader_.reset(new CustomizationWallpaperDownloader(
          g_browser_process->system_request_context(), device_wallpaper_url,
          ash::WallpaperController::GetDeviceWallpaperDir(),
          ash::WallpaperController::GetDeviceWallpaperFilePath(),
          base::Bind(&WallpaperManager::OnDeviceWallpaperDownloaded,
                     weak_factory_.GetWeakPtr(), account_id, hash)));
      device_wallpaper_downloader_->Start();
    } else {
      LOG(ERROR) << "The device wallpaper hash doesn't match with provided "
                    "hash value. Fallback to default wallpaper! ";
      SetDefaultWallpaperImpl(account_id, true /*show_wallpaper=*/);

      // Reset the boolean variable so that it can retry to download when the
      // next device wallpaper request comes in.
      retry_download_if_failed_ = true;
    }
    return;
  }

  const base::FilePath file_path =
      ash::WallpaperController::GetDeviceWallpaperFilePath();
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash()) {
    user_image_loader::StartWithFilePath(
        task_runner_, ash::WallpaperController::GetDeviceWallpaperFilePath(),
        ImageDecoder::ROBUST_JPEG_CODEC,
        0,  // Do not crop.
        base::Bind(&WallpaperManager::OnDeviceWallpaperDecoded,
                   weak_factory_.GetWeakPtr(), account_id));
  } else {
    ash::Shell::Get()->wallpaper_controller()->ReadAndDecodeWallpaper(
        base::Bind(&WallpaperManager::OnDeviceWallpaperDecoded,
                   weak_factory_.GetWeakPtr(), account_id),
        task_runner_, file_path);
  }
}

void WallpaperManager::OnDeviceWallpaperDecoded(
    const AccountId& account_id,
    std::unique_ptr<user_manager::UserImage> user_image) {
  // It might be possible that the device policy controlled wallpaper finishes
  // decoding after the user logs in. In this case do nothing.
  if (!user_manager::UserManager::Get()->IsUserLoggedIn()) {
    WallpaperInfo wallpaper_info = {
        ash::WallpaperController::GetDeviceWallpaperFilePath().value(),
        wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED, wallpaper::DEVICE,
        base::Time::Now().LocalMidnight()};
    // TODO(crbug.com/776464): This should go through PendingWallpaper after
    // moving to WallpaperController.
    SetWallpaper(user_image->image(), wallpaper_info);
  }
}

void WallpaperManager::SetCustomizedDefaultWallpaperImpl(
    const base::FilePath& default_small_wallpaper_file,
    std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
    const base::FilePath& default_large_wallpaper_file,
    std::unique_ptr<gfx::ImageSkia> large_wallpaper_image) {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash())
    return;
  ash::Shell::Get()->wallpaper_controller()->SetCustomizedDefaultWallpaperImpl(
      default_small_wallpaper_file, std::move(small_wallpaper_image),
      default_large_wallpaper_file, std::move(large_wallpaper_image));
}

wallpaper::WallpaperInfo* WallpaperManager::GetCachedWallpaperInfo() {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash())
    return &dummy_current_user_wallpaper_info_;
  return ash::Shell::Get()
      ->wallpaper_controller()
      ->GetCurrentUserWallpaperInfo();
}

ash::CustomWallpaperMap* WallpaperManager::GetWallpaperCacheMap() {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash())
    return &dummy_wallpaper_cache_map_;
  return ash::Shell::Get()->wallpaper_controller()->GetWallpaperCacheMap();
}

AccountId WallpaperManager::GetCurrentUserAccountId() {
  if (!ash::Shell::HasInstance() || ash_util::IsRunningInMash())
    return EmptyAccountId();
  return ash::Shell::Get()->wallpaper_controller()->GetCurrentUserAccountId();
}

}  // namespace chromeos
