// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_window_state_manager.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/backdrop_wallpaper_handlers/backdrop_wallpaper_handlers.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_info.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/strings/grit/app_locale_settings.h"
#include "url/gurl.h"

using base::Value;
using content::BrowserThread;

namespace wallpaper_base = extensions::api::wallpaper;
namespace wallpaper_private = extensions::api::wallpaper_private;
namespace set_wallpaper_if_exists = wallpaper_private::SetWallpaperIfExists;
namespace set_wallpaper = wallpaper_private::SetWallpaper;
namespace set_custom_wallpaper = wallpaper_private::SetCustomWallpaper;
namespace set_custom_wallpaper_layout =
    wallpaper_private::SetCustomWallpaperLayout;
namespace get_thumbnail = wallpaper_private::GetThumbnail;
namespace save_thumbnail = wallpaper_private::SaveThumbnail;
namespace get_offline_wallpaper_list =
    wallpaper_private::GetOfflineWallpaperList;
namespace record_wallpaper_uma = wallpaper_private::RecordWallpaperUMA;
namespace get_collections_info = wallpaper_private::GetCollectionsInfo;
namespace get_images_info = wallpaper_private::GetImagesInfo;
namespace get_local_image_paths = wallpaper_private::GetLocalImagePaths;
namespace get_local_image_data = wallpaper_private::GetLocalImageData;

namespace {

// The time in seconds and retry limit to re-check the profile sync service
// status. Only after the profile sync service has been configured, we can get
// the correct value of the user sync preference of "syncThemes".
constexpr int kRetryDelay = 10;
constexpr int kRetryLimit = 3;

constexpr char kPngFilePattern[] = "*.[pP][nN][gG]";
constexpr char kJpgFilePattern[] = "*.[jJ][pP][gG]";
constexpr char kJpegFilePattern[] = "*.[jJ][pP][eE][gG]";

// The url suffix used by the old wallpaper picker.
constexpr char kHighResolutionSuffix[] = "_high_resolution.jpg";

#if defined(GOOGLE_CHROME_BUILD)
const char kWallpaperManifestBaseURL[] =
    "https://storage.googleapis.com/chromeos-wallpaper-public/manifest_";
#endif

bool IsOEMDefaultWallpaper() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDefaultWallpaperIsOem);
}

bool IsUsingNewWallpaperPicker() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kNewWallpaperPicker);
}

// Returns a suffix to be appended to the base url of Backdrop wallpapers.
std::string GetBackdropWallpaperSuffix() {
  // FIFE url is used for Backdrop wallpapers and the desired image size should
  // be specified. Currently we are using two times the display size. This is
  // determined by trial and error and is subject to change.
  const gfx::Size& display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  return "=w" + std::to_string(
                    2 * std::max(display_size.width(), display_size.height()));
}

// Saves |data| as |file_name| to directory with |key|. Return false if the
// directory can not be found/created or failed to write file.
bool SaveData(int key,
              const std::string& file_name,
              const std::vector<char>& data) {
  base::FilePath data_dir;
  CHECK(PathService::Get(key, &data_dir));
  if (!base::DirectoryExists(data_dir) &&
      !base::CreateDirectory(data_dir)) {
    return false;
  }
  base::FilePath file_path = data_dir.Append(file_name);

  return base::PathExists(file_path) ||
         base::WriteFile(file_path, data.data(), data.size()) != -1;
}

// Gets |file_name| from directory with |key|. Return false if the directory can
// not be found or failed to read file to string |data|. Note if the |file_name|
// can not be found in the directory, return true with empty |data|. It is
// expected that we may try to access file which did not saved yet.
bool GetData(const base::FilePath& path, std::string* data) {
  base::FilePath data_dir = path.DirName();
  if (!base::DirectoryExists(data_dir) &&
      !base::CreateDirectory(data_dir))
    return false;

  return !base::PathExists(path) ||
         base::ReadFileToString(path, data);
}

// Gets the |User| for a given |BrowserContext|. The function will only return
// valid objects.
const user_manager::User* GetUserFromBrowserContext(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  return user;
}

wallpaper::WallpaperType getWallpaperType(
    wallpaper_private::WallpaperSource source) {
  switch (source) {
    case wallpaper_private::WALLPAPER_SOURCE_ONLINE:
      return wallpaper::ONLINE;
    case wallpaper_private::WALLPAPER_SOURCE_DAILY:
      return wallpaper::DAILY;
    case wallpaper_private::WALLPAPER_SOURCE_CUSTOM:
      return wallpaper::CUSTOMIZED;
    case wallpaper_private::WALLPAPER_SOURCE_OEM:
      return wallpaper::DEFAULT;
    case wallpaper_private::WALLPAPER_SOURCE_THIRDPARTY:
      return wallpaper::THIRDPARTY;
    default:
      return wallpaper::ONLINE;
  }
}

// Helper function to get the list of image paths under |path| that match
// |pattern|.
void EnumerateImages(const base::FilePath& path,
                     const std::string& pattern,
                     std::vector<std::string>* result_out) {
  base::FileEnumerator image_enum(
      path, true /* recursive */, base::FileEnumerator::FILES,
      FILE_PATH_LITERAL(pattern),
      base::FileEnumerator::FolderSearchPolicy::ALL);

  for (base::FilePath image_path = image_enum.Next(); !image_path.empty();
       image_path = image_enum.Next()) {
    result_out->emplace_back(image_path.value());
  }
}

// Recursively retrieves the paths of the image files under |path|.
std::vector<std::string> GetImagePaths(const base::FilePath& path) {
  WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
      WallpaperFunctionBase::GetNonBlockingTaskRunner());

  // TODO(crbug.com/810575): Add metrics on the number of files retrieved, and
  // support getting paths incrementally in case the user has a large number of
  // local images.
  std::vector<std::string> image_paths;
  EnumerateImages(path, kPngFilePattern, &image_paths);
  EnumerateImages(path, kJpgFilePattern, &image_paths);
  EnumerateImages(path, kJpegFilePattern, &image_paths);

  return image_paths;
}

}  // namespace

ExtensionFunction::ResponseAction WallpaperPrivateGetStringsFunction::Run() {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

#define SET_STRING(id, idr) \
  dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("webFontFamily", IDS_WEB_FONT_FAMILY);
  SET_STRING("webFontSize", IDS_WEB_FONT_SIZE);
  SET_STRING("allCategoryLabel", IDS_WALLPAPER_MANAGER_ALL_CATEGORY_LABEL);
  SET_STRING("deleteCommandLabel", IDS_WALLPAPER_MANAGER_DELETE_COMMAND_LABEL);
  SET_STRING("customCategoryLabel",
             IsUsingNewWallpaperPicker()
                 ? IDS_WALLPAPER_MANAGER_MY_PHOTOS_CATEGORY_LABEL
                 : IDS_WALLPAPER_MANAGER_CUSTOM_CATEGORY_LABEL);
  SET_STRING("selectCustomLabel",
             IDS_WALLPAPER_MANAGER_SELECT_CUSTOM_LABEL);
  SET_STRING("positionLabel", IDS_WALLPAPER_MANAGER_POSITION_LABEL);
  SET_STRING("colorLabel", IDS_WALLPAPER_MANAGER_COLOR_LABEL);
  SET_STRING("refreshLabel", IDS_WALLPAPER_MANAGER_REFRESH_LABEL);
  SET_STRING("exploreLabel", IDS_WALLPAPER_MANAGER_EXPLORE_LABEL);
  SET_STRING("centerCroppedLayout",
             IDS_WALLPAPER_MANAGER_LAYOUT_CENTER_CROPPED);
  SET_STRING("centerLayout", IDS_WALLPAPER_MANAGER_LAYOUT_CENTER);
  SET_STRING("stretchLayout", IDS_WALLPAPER_MANAGER_LAYOUT_STRETCH);
  SET_STRING("connectionFailed", IDS_WALLPAPER_MANAGER_ACCESS_FAIL);
  SET_STRING("downloadFailed", IDS_WALLPAPER_MANAGER_DOWNLOAD_FAIL);
  SET_STRING("downloadCanceled", IDS_WALLPAPER_MANAGER_DOWNLOAD_CANCEL);
  SET_STRING("customWallpaperWarning",
             IDS_WALLPAPER_MANAGER_SHOW_CUSTOM_WALLPAPER_ON_START_WARNING);
  SET_STRING("accessFileFailure", IDS_WALLPAPER_MANAGER_ACCESS_FILE_FAILURE);
  SET_STRING("invalidWallpaper", IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER);
  SET_STRING("noImagesAvailable", IDS_WALLPAPER_MANAGER_NO_IMAGES_AVAILABLE);
  SET_STRING("surpriseMeLabel", IsUsingNewWallpaperPicker()
                                    ? IDS_WALLPAPER_MANAGER_DAILY_REFRESH_LABEL
                                    : IDS_WALLPAPER_MANAGER_SURPRISE_ME_LABEL);
  SET_STRING("learnMore", IDS_LEARN_MORE);
  SET_STRING("currentWallpaperSetByMessage",
             IDS_CURRENT_WALLPAPER_SET_BY_MESSAGE);
#undef SET_STRING

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, dict.get());

  // TODO(crbug.com/777293, 776464): Make it work under mash (most likely by
  // creating a mojo callback).
  dict->SetString("currentWallpaper",
                  ash::Shell::HasInstance()
                      ? ash::Shell::Get()
                            ->wallpaper_controller()
                            ->GetActiveUserWallpaperLocation()
                      : std::string());

#if defined(GOOGLE_CHROME_BUILD)
  dict->SetString("manifestBaseURL", kWallpaperManifestBaseURL);
#endif

  dict->SetBoolean("isOEMDefaultWallpaper", IsOEMDefaultWallpaper());
  dict->SetString("canceledWallpaper",
                  wallpaper_api_util::kCancelWallpaperMessage);
  dict->SetBoolean("useNewWallpaperPicker", IsUsingNewWallpaperPicker());
  dict->SetString("highResolutionSuffix", IsUsingNewWallpaperPicker()
                                              ? GetBackdropWallpaperSuffix()
                                              : kHighResolutionSuffix);

  return RespondNow(OneArgument(std::move(dict)));
}

ExtensionFunction::ResponseAction
WallpaperPrivateGetSyncSettingFunction::Run() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&WallpaperPrivateGetSyncSettingFunction::
                         CheckProfileSyncServiceStatus,
                     this));
  return RespondLater();
}

void WallpaperPrivateGetSyncSettingFunction::CheckProfileSyncServiceStatus() {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  if (retry_number > kRetryLimit) {
    // It's most likely that the wallpaper synchronization is enabled (It's
    // enabled by default so unless the user disables it explicitly it remains
    // enabled).
    dict->SetBoolean("syncThemes", true);
    Respond(OneArgument(std::move(dict)));
    return;
  }

  Profile* profile =  Profile::FromBrowserContext(browser_context());
  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (!sync_service) {
    dict->SetBoolean("syncThemes", false);
    Respond(OneArgument(std::move(dict)));
    return;
  }

  if (sync_service->IsSyncActive() && sync_service->ConfigurationDone()) {
    dict->SetBoolean("syncThemes",
                     sync_service->GetActiveDataTypes().Has(syncer::THEMES));
    Respond(OneArgument(std::move(dict)));
    return;
  }

  // It's possible that the profile sync service hasn't finished configuring yet
  // when we're trying to query the user preference (this seems only happen for
  // the first time configuration). In this case GetActiveDataTypes() returns an
  // empty set. So re-check the status later.
  retry_number++;
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&WallpaperPrivateGetSyncSettingFunction::
                         CheckProfileSyncServiceStatus,
                     this),
      base::TimeDelta::FromSeconds(retry_number * kRetryDelay));
}

WallpaperPrivateSetWallpaperIfExistsFunction::
    WallpaperPrivateSetWallpaperIfExistsFunction() {}

WallpaperPrivateSetWallpaperIfExistsFunction::
    ~WallpaperPrivateSetWallpaperIfExistsFunction() {}

bool WallpaperPrivateSetWallpaperIfExistsFunction::RunAsync() {
  params = set_wallpaper_if_exists::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // Gets account id from the caller, ensuring multiprofile compatibility.
  const user_manager::User* user = GetUserFromBrowserContext(browser_context());
  account_id_ = user->GetAccountId();

  base::FilePath wallpaper_path;
  base::FilePath fallback_path;
  ash::WallpaperController::WallpaperResolution resolution =
      ash::WallpaperController::GetAppropriateResolution();

  std::string file_name = GURL(params->url).ExtractFileName();
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS,
                         &wallpaper_path));
  fallback_path = wallpaper_path.Append(file_name);
  if (params->layout != wallpaper_base::WALLPAPER_LAYOUT_STRETCH &&
      resolution == ash::WallpaperController::WALLPAPER_RESOLUTION_SMALL) {
    file_name = base::FilePath(file_name)
                    .InsertBeforeExtension(
                        ash::WallpaperController::kSmallWallpaperSuffix)
                    .value();
  }
  wallpaper_path = wallpaper_path.Append(file_name);

  GetNonBlockingTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&WallpaperPrivateSetWallpaperIfExistsFunction::
                                    ReadFileAndInitiateStartDecode,
                                this, wallpaper_path, fallback_path));
  return true;
}

void WallpaperPrivateSetWallpaperIfExistsFunction::
    ReadFileAndInitiateStartDecode(const base::FilePath& file_path,
                                   const base::FilePath& fallback_path) {
  AssertCalledOnWallpaperSequence(GetNonBlockingTaskRunner());
  base::FilePath path = file_path;

  if (!base::PathExists(file_path))
    path = fallback_path;

  std::string data;
  if (base::PathExists(path) &&
      base::ReadFileToString(path, &data)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &WallpaperPrivateSetWallpaperIfExistsFunction::StartDecode, this,
            std::vector<char>(data.begin(), data.end())));
    return;
  }
  std::string error = base::StringPrintf(
        "Failed to set wallpaper %s from file system.",
        path.BaseName().value().c_str());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &WallpaperPrivateSetWallpaperIfExistsFunction::OnFileNotExists, this,
          error));
}

void WallpaperPrivateSetWallpaperIfExistsFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& image) {
  // Set unsafe_wallpaper_decoder_ to null since the decoding already finished.
  unsafe_wallpaper_decoder_ = NULL;

  wallpaper::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));

  WallpaperControllerClient::Get()->SetOnlineWallpaper(
      account_id_, image, params->url, layout, params->preview_mode);
  SetResult(std::make_unique<base::Value>(true));
  SendResponse(true);
}

void WallpaperPrivateSetWallpaperIfExistsFunction::OnFileNotExists(
    const std::string& error) {
  SetResult(std::make_unique<base::Value>(false));
  OnFailure(error);
}

WallpaperPrivateSetWallpaperFunction::WallpaperPrivateSetWallpaperFunction() {
}

WallpaperPrivateSetWallpaperFunction::~WallpaperPrivateSetWallpaperFunction() {
}

bool WallpaperPrivateSetWallpaperFunction::RunAsync() {
  params = set_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // Gets account id from the caller, ensuring multiprofile compatibility.
  const user_manager::User* user = GetUserFromBrowserContext(browser_context());
  account_id_ = user->GetAccountId();

  StartDecode(params->wallpaper);
  return true;
}

void WallpaperPrivateSetWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& image) {
  wallpaper_ = image;
  // Set unsafe_wallpaper_decoder_ to null since the decoding already finished.
  unsafe_wallpaper_decoder_ = NULL;

  GetBlockingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WallpaperPrivateSetWallpaperFunction::SaveToFile, this));
}

void WallpaperPrivateSetWallpaperFunction::SaveToFile() {
  AssertCalledOnWallpaperSequence(GetBlockingTaskRunner());
  std::string file_name = GURL(params->url).ExtractFileName();
  if (SaveData(chrome::DIR_CHROMEOS_WALLPAPERS, file_name, params->wallpaper)) {
    wallpaper_.EnsureRepsForSupportedScales();
    std::unique_ptr<gfx::ImageSkia> deep_copy(wallpaper_.DeepCopy());
    // ImageSkia is not RefCountedThreadSafe. Use a deep copied ImageSkia if
    // post to another thread.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &WallpaperPrivateSetWallpaperFunction::SetDecodedWallpaper, this,
            std::move(deep_copy)));

    base::FilePath wallpaper_dir;
    CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
    base::FilePath file_path =
        wallpaper_dir.Append(file_name).InsertBeforeExtension(
            ash::WallpaperController::kSmallWallpaperSuffix);
    if (base::PathExists(file_path))
      return;
    // Generates and saves small resolution wallpaper. Uses CENTER_CROPPED to
    // maintain the aspect ratio after resize.
    ash::WallpaperController::ResizeAndSaveWallpaper(
        wallpaper_, file_path, wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
        ash::WallpaperController::kSmallWallpaperMaxWidth,
        ash::WallpaperController::kSmallWallpaperMaxHeight, NULL);
  } else {
    std::string error = base::StringPrintf(
        "Failed to create/write wallpaper to %s.", file_name.c_str());
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&WallpaperPrivateSetWallpaperFunction::OnFailure, this,
                       error));
  }
}

void WallpaperPrivateSetWallpaperFunction::SetDecodedWallpaper(
    std::unique_ptr<gfx::ImageSkia> image) {
  wallpaper::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));

  WallpaperControllerClient::Get()->SetOnlineWallpaper(
      account_id_, *image.get(), params->url, layout, params->preview_mode);
  SendResponse(true);
}

WallpaperPrivateResetWallpaperFunction::
    WallpaperPrivateResetWallpaperFunction() {}

WallpaperPrivateResetWallpaperFunction::
    ~WallpaperPrivateResetWallpaperFunction() {}

bool WallpaperPrivateResetWallpaperFunction::RunAsync() {
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();

  WallpaperControllerClient::Get()->SetDefaultWallpaper(
      account_id, true /* show_wallpaper */);
  return true;
}

WallpaperPrivateSetCustomWallpaperFunction::
    WallpaperPrivateSetCustomWallpaperFunction() {}

WallpaperPrivateSetCustomWallpaperFunction::
    ~WallpaperPrivateSetCustomWallpaperFunction() {}

bool WallpaperPrivateSetCustomWallpaperFunction::RunAsync() {
  params = set_custom_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // Gets account id from the caller, ensuring multiprofile compatibility.
  const user_manager::User* user = GetUserFromBrowserContext(browser_context());
  account_id_ = user->GetAccountId();
  wallpaper_files_id_ =
      WallpaperControllerClient::Get()->GetFilesId(account_id_);

  StartDecode(params->wallpaper);

  return true;
}

void WallpaperPrivateSetCustomWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& image) {
  base::FilePath thumbnail_path =
      ash::WallpaperController::GetCustomWallpaperPath(
          ash::WallpaperController::kThumbnailWallpaperSubDir,
          wallpaper_files_id_.id(), params->file_name);

  wallpaper::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));
  wallpaper_api_util::RecordCustomWallpaperLayout(layout);

  WallpaperControllerClient::Get()->SetCustomWallpaper(
      account_id_, wallpaper_files_id_, params->file_name, layout, image,
      params->preview_mode);
  unsafe_wallpaper_decoder_ = NULL;

  if (params->generate_thumbnail) {
    image.EnsureRepsForSupportedScales();
    std::unique_ptr<gfx::ImageSkia> deep_copy(image.DeepCopy());
    // Generates thumbnail before call api function callback. We can then
    // request thumbnail in the javascript callback.
    GetBlockingTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WallpaperPrivateSetCustomWallpaperFunction::GenerateThumbnail,
            this, thumbnail_path, std::move(deep_copy)));
  } else {
    SendResponse(true);
  }
}

void WallpaperPrivateSetCustomWallpaperFunction::GenerateThumbnail(
    const base::FilePath& thumbnail_path,
    std::unique_ptr<gfx::ImageSkia> image) {
  AssertCalledOnWallpaperSequence(GetBlockingTaskRunner());
  if (!base::PathExists(thumbnail_path.DirName()))
    base::CreateDirectory(thumbnail_path.DirName());

  scoped_refptr<base::RefCountedBytes> data;
  ash::WallpaperController::ResizeImage(
      *image, wallpaper::WALLPAPER_LAYOUT_STRETCH,
      ash::WallpaperController::kWallpaperThumbnailWidth,
      ash::WallpaperController::kWallpaperThumbnailHeight, &data, NULL);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &WallpaperPrivateSetCustomWallpaperFunction::ThumbnailGenerated, this,
          base::RetainedRef(data)));
}

void WallpaperPrivateSetCustomWallpaperFunction::ThumbnailGenerated(
    base::RefCountedBytes* data) {
  SetResult(Value::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(data->front()), data->size()));
  SendResponse(true);
}

WallpaperPrivateSetCustomWallpaperLayoutFunction::
    WallpaperPrivateSetCustomWallpaperLayoutFunction() {}

WallpaperPrivateSetCustomWallpaperLayoutFunction::
    ~WallpaperPrivateSetCustomWallpaperLayoutFunction() {}

bool WallpaperPrivateSetCustomWallpaperLayoutFunction::RunAsync() {
  std::unique_ptr<set_custom_wallpaper_layout::Params> params(
      set_custom_wallpaper_layout::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  wallpaper::WallpaperLayout new_layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));
  wallpaper_api_util::RecordCustomWallpaperLayout(new_layout);
  WallpaperControllerClient::Get()->UpdateCustomWallpaperLayout(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
      new_layout);
  SendResponse(true);

  return true;
}

WallpaperPrivateMinimizeInactiveWindowsFunction::
    WallpaperPrivateMinimizeInactiveWindowsFunction() {
}

WallpaperPrivateMinimizeInactiveWindowsFunction::
    ~WallpaperPrivateMinimizeInactiveWindowsFunction() {
}

ExtensionFunction::ResponseAction
WallpaperPrivateMinimizeInactiveWindowsFunction::Run() {
  // TODO(mash): Convert to mojo API. http://crbug.com/557405
  if (!ash_util::IsRunningInMash()) {
    ash::WallpaperWindowStateManager::MinimizeInactiveWindows(
        user_manager::UserManager::Get()->GetActiveUser()->username_hash());
  }
  return RespondNow(NoArguments());
}

WallpaperPrivateRestoreMinimizedWindowsFunction::
    WallpaperPrivateRestoreMinimizedWindowsFunction() {
}

WallpaperPrivateRestoreMinimizedWindowsFunction::
    ~WallpaperPrivateRestoreMinimizedWindowsFunction() {
}

ExtensionFunction::ResponseAction
WallpaperPrivateRestoreMinimizedWindowsFunction::Run() {
  // TODO(mash): Convert to mojo API. http://crbug.com/557405
  if (!ash_util::IsRunningInMash()) {
    ash::WallpaperWindowStateManager::RestoreWindows(
        user_manager::UserManager::Get()->GetActiveUser()->username_hash());
  }
  return RespondNow(NoArguments());
}

WallpaperPrivateGetThumbnailFunction::WallpaperPrivateGetThumbnailFunction() {
}

WallpaperPrivateGetThumbnailFunction::~WallpaperPrivateGetThumbnailFunction() {
}

bool WallpaperPrivateGetThumbnailFunction::RunAsync() {
  std::unique_ptr<get_thumbnail::Params> params(
      get_thumbnail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath thumbnail_path;
  if (params->source == wallpaper_private::WALLPAPER_SOURCE_ONLINE) {
    std::string file_name = GURL(params->url_or_file).ExtractFileName();
    CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS,
                           &thumbnail_path));
    thumbnail_path = thumbnail_path.Append(file_name);
  } else {
    if (!IsOEMDefaultWallpaper()) {
      SetError("No OEM wallpaper.");
      return false;
    }

    // TODO(bshe): Small resolution wallpaper is used here as wallpaper
    // thumbnail. We should either resize it or include a wallpaper thumbnail in
    // addition to large and small wallpaper resolutions.
    thumbnail_path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        chromeos::switches::kDefaultWallpaperSmall);
  }

  WallpaperFunctionBase::GetNonBlockingTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&WallpaperPrivateGetThumbnailFunction::Get,
                                this, thumbnail_path));
  return true;
}

void WallpaperPrivateGetThumbnailFunction::Failure(
    const std::string& file_name) {
  SetError(base::StringPrintf("Failed to access wallpaper thumbnails for %s.",
                              file_name.c_str()));
  SendResponse(false);
}

void WallpaperPrivateGetThumbnailFunction::FileNotLoaded() {
  SendResponse(true);
}

void WallpaperPrivateGetThumbnailFunction::FileLoaded(
    const std::string& data) {
  SetResult(Value::CreateWithCopiedBuffer(data.c_str(), data.size()));
  SendResponse(true);
}

void WallpaperPrivateGetThumbnailFunction::Get(const base::FilePath& path) {
  WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
      WallpaperFunctionBase::GetNonBlockingTaskRunner());
  std::string data;
  if (GetData(path, &data)) {
    if (data.empty()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(&WallpaperPrivateGetThumbnailFunction::FileNotLoaded,
                         this));
    } else {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(&WallpaperPrivateGetThumbnailFunction::FileLoaded,
                         this, data));
    }
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&WallpaperPrivateGetThumbnailFunction::Failure, this,
                       path.BaseName().value()));
  }
}

WallpaperPrivateSaveThumbnailFunction::WallpaperPrivateSaveThumbnailFunction() {
}

WallpaperPrivateSaveThumbnailFunction::
    ~WallpaperPrivateSaveThumbnailFunction() {}

bool WallpaperPrivateSaveThumbnailFunction::RunAsync() {
  std::unique_ptr<save_thumbnail::Params> params(
      save_thumbnail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  WallpaperFunctionBase::GetNonBlockingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WallpaperPrivateSaveThumbnailFunction::Save, this,
                     params->data, GURL(params->url).ExtractFileName()));
  return true;
}

void WallpaperPrivateSaveThumbnailFunction::Failure(
    const std::string& file_name) {
  SetError(base::StringPrintf("Failed to create/write thumbnail of %s.",
                              file_name.c_str()));
  SendResponse(false);
}

void WallpaperPrivateSaveThumbnailFunction::Success() {
  SendResponse(true);
}

void WallpaperPrivateSaveThumbnailFunction::Save(const std::vector<char>& data,
                                                 const std::string& file_name) {
  WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
      WallpaperFunctionBase::GetNonBlockingTaskRunner());
  if (SaveData(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS, file_name, data)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&WallpaperPrivateSaveThumbnailFunction::Success, this));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&WallpaperPrivateSaveThumbnailFunction::Failure, this,
                       file_name));
  }
}

WallpaperPrivateGetOfflineWallpaperListFunction::
    WallpaperPrivateGetOfflineWallpaperListFunction() {
}

WallpaperPrivateGetOfflineWallpaperListFunction::
    ~WallpaperPrivateGetOfflineWallpaperListFunction() {
}

bool WallpaperPrivateGetOfflineWallpaperListFunction::RunAsync() {
  WallpaperFunctionBase::GetNonBlockingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WallpaperPrivateGetOfflineWallpaperListFunction::GetList,
                     this));
  return true;
}

void WallpaperPrivateGetOfflineWallpaperListFunction::GetList() {
  WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
      WallpaperFunctionBase::GetNonBlockingTaskRunner());
  std::vector<std::string> file_list;
  base::FilePath wallpaper_dir;
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
  if (base::DirectoryExists(wallpaper_dir)) {
    base::FileEnumerator files(wallpaper_dir, false,
                               base::FileEnumerator::FILES);
    for (base::FilePath current = files.Next(); !current.empty();
         current = files.Next()) {
      std::string file_name = current.BaseName().RemoveExtension().value();
      // Do not add file name of small resolution wallpaper to the list.
      if (!base::EndsWith(file_name,
                          ash::WallpaperController::kSmallWallpaperSuffix,
                          base::CompareCase::SENSITIVE))
        file_list.push_back(current.BaseName().value());
    }
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &WallpaperPrivateGetOfflineWallpaperListFunction::OnComplete, this,
          file_list));
}

void WallpaperPrivateGetOfflineWallpaperListFunction::OnComplete(
    const std::vector<std::string>& file_list) {
  std::unique_ptr<base::ListValue> results(new base::ListValue());
  results->AppendStrings(file_list);
  SetResult(std::move(results));
  SendResponse(true);
}

ExtensionFunction::ResponseAction
WallpaperPrivateRecordWallpaperUMAFunction::Run() {
  std::unique_ptr<record_wallpaper_uma::Params> params(
      record_wallpaper_uma::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  wallpaper::WallpaperType source = getWallpaperType(params->source);
  UMA_HISTOGRAM_ENUMERATION("Ash.Wallpaper.Source", source,
                            wallpaper::WALLPAPER_TYPE_COUNT);
  return RespondNow(NoArguments());
}

WallpaperPrivateGetCollectionsInfoFunction::
    WallpaperPrivateGetCollectionsInfoFunction() = default;

WallpaperPrivateGetCollectionsInfoFunction::
    ~WallpaperPrivateGetCollectionsInfoFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetCollectionsInfoFunction::Run() {
  collection_info_fetcher_ =
      std::make_unique<backdrop_wallpaper_handlers::CollectionInfoFetcher>();
  collection_info_fetcher_->Start(base::BindOnce(
      &WallpaperPrivateGetCollectionsInfoFunction::OnCollectionsInfoFetched,
      this));
  return RespondLater();
}

void WallpaperPrivateGetCollectionsInfoFunction::OnCollectionsInfoFetched(
    bool success,
    const std::vector<extensions::api::wallpaper_private::CollectionInfo>&
        collections_info_list) {
  if (!success) {
    Respond(Error("Collection names are not available."));
    return;
  }
  Respond(ArgumentList(
      get_collections_info::Results::Create(collections_info_list)));
}

WallpaperPrivateGetImagesInfoFunction::WallpaperPrivateGetImagesInfoFunction() =
    default;

WallpaperPrivateGetImagesInfoFunction::
    ~WallpaperPrivateGetImagesInfoFunction() = default;

ExtensionFunction::ResponseAction WallpaperPrivateGetImagesInfoFunction::Run() {
  std::unique_ptr<get_images_info::Params> params(
      get_images_info::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  image_info_fetcher_ =
      std::make_unique<backdrop_wallpaper_handlers::ImageInfoFetcher>(
          params->collection_id);
  image_info_fetcher_->Start(base::BindOnce(
      &WallpaperPrivateGetImagesInfoFunction::OnImagesInfoFetched, this));
  return RespondLater();
}

void WallpaperPrivateGetImagesInfoFunction::OnImagesInfoFetched(
    bool success,
    const std::vector<extensions::api::wallpaper_private::ImageInfo>&
        images_info_list) {
  if (!success) {
    Respond(Error("Images info is not available."));
    return;
  }
  Respond(ArgumentList(get_images_info::Results::Create(images_info_list)));
}

WallpaperPrivateGetLocalImagePathsFunction::
    WallpaperPrivateGetLocalImagePathsFunction() = default;

WallpaperPrivateGetLocalImagePathsFunction::
    ~WallpaperPrivateGetLocalImagePathsFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetLocalImagePathsFunction::Run() {
  base::FilePath path = file_manager::util::GetDownloadsFolderForProfile(
      Profile::FromBrowserContext(browser_context()));
  base::PostTaskAndReplyWithResult(
      WallpaperFunctionBase::GetNonBlockingTaskRunner(), FROM_HERE,
      base::BindOnce(&GetImagePaths, path),
      base::BindOnce(
          &WallpaperPrivateGetLocalImagePathsFunction::OnGetImagePathsComplete,
          this));
  return RespondLater();
}

void WallpaperPrivateGetLocalImagePathsFunction::OnGetImagePathsComplete(
    const std::vector<std::string>& image_paths) {
  Respond(ArgumentList(get_local_image_paths::Results::Create(image_paths)));
}

WallpaperPrivateGetLocalImageDataFunction::
    WallpaperPrivateGetLocalImageDataFunction() = default;

WallpaperPrivateGetLocalImageDataFunction::
    ~WallpaperPrivateGetLocalImageDataFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetLocalImageDataFunction::Run() {
  std::unique_ptr<get_local_image_data::Params> params(
      get_local_image_data::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // TODO(crbug.com/811564): Create file backed blob instead.
  auto image_data = std::make_unique<std::string>();
  std::string* image_data_ptr = image_data.get();
  base::PostTaskAndReplyWithResult(
      WallpaperFunctionBase::GetNonBlockingTaskRunner(), FROM_HERE,
      base::BindOnce(&base::ReadFileToString,
                     base::FilePath(params->image_path), image_data_ptr),
      base::BindOnce(
          &WallpaperPrivateGetLocalImageDataFunction::OnReadImageDataComplete,
          this, std::move(image_data)));

  return RespondLater();
}

void WallpaperPrivateGetLocalImageDataFunction::OnReadImageDataComplete(
    std::unique_ptr<std::string> image_data,
    bool success) {
  if (!success) {
    Respond(Error("Reading image data failed."));
    return;
  }

  Respond(ArgumentList(get_local_image_data::Results::Create(
      std::vector<char>(image_data->begin(), image_data->end()))));
}

WallpaperPrivateConfirmPreviewWallpaperFunction::
    WallpaperPrivateConfirmPreviewWallpaperFunction() = default;

WallpaperPrivateConfirmPreviewWallpaperFunction::
    ~WallpaperPrivateConfirmPreviewWallpaperFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateConfirmPreviewWallpaperFunction::Run() {
  WallpaperControllerClient::Get()->ConfirmPreviewWallpaper();
  return RespondNow(NoArguments());
}

WallpaperPrivateCancelPreviewWallpaperFunction::
    WallpaperPrivateCancelPreviewWallpaperFunction() = default;

WallpaperPrivateCancelPreviewWallpaperFunction::
    ~WallpaperPrivateCancelPreviewWallpaperFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateCancelPreviewWallpaperFunction::Run() {
  WallpaperControllerClient::Get()->CancelPreviewWallpaper();
  return RespondNow(NoArguments());
}
