// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/strings/grit/app_locale_settings.h"
#include "url/gurl.h"

using base::BinaryValue;
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

namespace {

#if defined(GOOGLE_CHROME_BUILD)
const char kWallpaperManifestBaseURL[] =
    "https://storage.googleapis.com/chromeos-wallpaper-public/manifest_";
#endif

bool IsOEMDefaultWallpaper() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDefaultWallpaperIsOem);
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

// WindowStateManager remembers which windows have been minimized in order to
// restore them when the wallpaper viewer is hidden.
class WindowStateManager : public aura::WindowObserver {
 public:
  typedef std::map<std::string, std::set<aura::Window*> >
      UserIDHashWindowListMap;

  // Minimizes all windows except the active window.
  static void MinimizeInactiveWindows(const std::string& user_id_hash);

  // Unminimizes all minimized windows restoring them to their previous state.
  // This should only be called after calling MinimizeInactiveWindows.
  static void RestoreWindows(const std::string& user_id_hash);

 private:
  WindowStateManager();

  ~WindowStateManager() override;

  // Store all unminimized windows except |active_window| and minimize them.
  // All the windows are saved in a map and the key value is |user_id_hash|.
  void BuildWindowListAndMinimizeInactiveForUser(
      const std::string& user_id_hash, aura::Window* active_window);

  // Unminimize all the stored windows for |user_id_hash|.
  void RestoreMinimizedWindows(const std::string& user_id_hash);

  // Remove the observer from |window| if |window| is no longer referenced in
  // user_id_hash_window_list_map_.
  void RemoveObserverIfUnreferenced(aura::Window* window);

  // aura::WindowObserver overrides.
  void OnWindowDestroyed(aura::Window* window) override;

  // aura::WindowObserver overrides.
  void OnWindowStackingChanged(aura::Window* window) override;

  // Map of user id hash and associated list of minimized windows.
  UserIDHashWindowListMap user_id_hash_window_list_map_;

  DISALLOW_COPY_AND_ASSIGN(WindowStateManager);
};

// static
WindowStateManager* g_window_state_manager = NULL;

// static
void WindowStateManager::MinimizeInactiveWindows(
    const std::string& user_id_hash) {
  if (!g_window_state_manager)
    g_window_state_manager = new WindowStateManager();
  g_window_state_manager->BuildWindowListAndMinimizeInactiveForUser(
      user_id_hash, ash::wm::GetActiveWindow());
}

// static
void WindowStateManager::RestoreWindows(const std::string& user_id_hash) {
  if (!g_window_state_manager) {
    DCHECK(false) << "This should only be called after calling "
                  << "MinimizeInactiveWindows.";
    return;
  }

  g_window_state_manager->RestoreMinimizedWindows(user_id_hash);
  if (g_window_state_manager->user_id_hash_window_list_map_.empty()) {
    delete g_window_state_manager;
    g_window_state_manager = NULL;
  }
}

WindowStateManager::WindowStateManager() {}

WindowStateManager::~WindowStateManager() {}

void WindowStateManager::BuildWindowListAndMinimizeInactiveForUser(
    const std::string& user_id_hash, aura::Window* active_window) {
  if (user_id_hash_window_list_map_.find(user_id_hash) ==
      user_id_hash_window_list_map_.end()) {
    user_id_hash_window_list_map_[user_id_hash] = std::set<aura::Window*>();
  }
  std::set<aura::Window*>* results =
      &user_id_hash_window_list_map_[user_id_hash];

  std::vector<aura::Window*> windows = ash::Shell::GetInstance()->
      mru_window_tracker()->BuildWindowListIgnoreModal();

  for (std::vector<aura::Window*>::iterator iter = windows.begin();
       iter != windows.end(); ++iter) {
    // Ignore active window and minimized windows.
    if (*iter == active_window || ash::wm::GetWindowState(*iter)->IsMinimized())
      continue;

    if (!(*iter)->HasObserver(this))
      (*iter)->AddObserver(this);

    results->insert(*iter);
    ash::wm::GetWindowState(*iter)->Minimize();
  }
}

void WindowStateManager::RestoreMinimizedWindows(
    const std::string& user_id_hash) {
  UserIDHashWindowListMap::iterator it =
      user_id_hash_window_list_map_.find(user_id_hash);
  if (it == user_id_hash_window_list_map_.end()) {
    DCHECK(false) << "This should only be called after calling "
                  << "MinimizeInactiveWindows.";
    return;
  }

  std::set<aura::Window*> removed_windows;
  removed_windows.swap(it->second);
  user_id_hash_window_list_map_.erase(it);

  for (std::set<aura::Window*>::iterator iter = removed_windows.begin();
       iter != removed_windows.end(); ++iter) {
    ash::wm::GetWindowState(*iter)->Unminimize();
    RemoveObserverIfUnreferenced(*iter);
  }
}

void WindowStateManager::RemoveObserverIfUnreferenced(aura::Window* window) {
  for (UserIDHashWindowListMap::iterator iter =
           user_id_hash_window_list_map_.begin();
       iter != user_id_hash_window_list_map_.end();
       ++iter) {
    if (iter->second.find(window) != iter->second.end())
      return;
  }
  // Remove observer if |window| is not observed by any users.
  window->RemoveObserver(this);
}

void WindowStateManager::OnWindowDestroyed(aura::Window* window) {
  for (UserIDHashWindowListMap::iterator iter =
           user_id_hash_window_list_map_.begin();
       iter != user_id_hash_window_list_map_.end();
       ++iter) {
    iter->second.erase(window);
  }
}

void WindowStateManager::OnWindowStackingChanged(aura::Window* window) {
  // If user interacted with the |window| while wallpaper picker is opening,
  // removes the |window| from observed list.
  for (auto iter = user_id_hash_window_list_map_.begin();
       iter != user_id_hash_window_list_map_.end(); ++iter) {
    iter->second.erase(window);
  }
  window->RemoveObserver(this);
}

}  // namespace

bool WallpaperPrivateGetStringsFunction::RunSync() {
  base::DictionaryValue* dict = new base::DictionaryValue();
  SetResult(dict);

#define SET_STRING(id, idr) \
  dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("webFontFamily", IDS_WEB_FONT_FAMILY);
  SET_STRING("webFontSize", IDS_WEB_FONT_SIZE);
  SET_STRING("allCategoryLabel", IDS_WALLPAPER_MANAGER_ALL_CATEGORY_LABEL);
  SET_STRING("deleteCommandLabel", IDS_WALLPAPER_MANAGER_DELETE_COMMAND_LABEL);
  SET_STRING("customCategoryLabel",
             IDS_WALLPAPER_MANAGER_CUSTOM_CATEGORY_LABEL);
  SET_STRING("selectCustomLabel",
             IDS_WALLPAPER_MANAGER_SELECT_CUSTOM_LABEL);
  SET_STRING("positionLabel", IDS_WALLPAPER_MANAGER_POSITION_LABEL);
  SET_STRING("colorLabel", IDS_WALLPAPER_MANAGER_COLOR_LABEL);
  SET_STRING("centerCroppedLayout",
             IDS_OPTIONS_WALLPAPER_CENTER_CROPPED_LAYOUT);
  SET_STRING("centerLayout", IDS_OPTIONS_WALLPAPER_CENTER_LAYOUT);
  SET_STRING("stretchLayout", IDS_OPTIONS_WALLPAPER_STRETCH_LAYOUT);
  SET_STRING("connectionFailed", IDS_WALLPAPER_MANAGER_ACCESS_FAIL);
  SET_STRING("downloadFailed", IDS_WALLPAPER_MANAGER_DOWNLOAD_FAIL);
  SET_STRING("downloadCanceled", IDS_WALLPAPER_MANAGER_DOWNLOAD_CANCEL);
  SET_STRING("customWallpaperWarning",
             IDS_WALLPAPER_MANAGER_SHOW_CUSTOM_WALLPAPER_ON_START_WARNING);
  SET_STRING("accessFileFailure", IDS_WALLPAPER_MANAGER_ACCESS_FILE_FAILURE);
  SET_STRING("invalidWallpaper", IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER);
  SET_STRING("surpriseMeLabel", IDS_WALLPAPER_MANAGER_SURPRISE_ME_LABEL);
  SET_STRING("learnMore", IDS_LEARN_MORE);
  SET_STRING("currentWallpaperSetByMessage",
             IDS_CURRENT_WALLPAPER_SET_BY_MESSAGE);
#undef SET_STRING

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, dict);

  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  wallpaper::WallpaperInfo info;

  if (wallpaper_manager->GetLoggedInUserWallpaperInfo(&info))
    dict->SetString("currentWallpaper", info.location);

#if defined(GOOGLE_CHROME_BUILD)
  dict->SetString("manifestBaseURL", kWallpaperManifestBaseURL);
#endif

  Profile* profile = Profile::FromBrowserContext(browser_context());
  std::string app_name(
      profile->GetPrefs()->GetString(prefs::kCurrentWallpaperAppName));
  if (!app_name.empty())
    dict->SetString("wallpaperAppName", app_name);

  dict->SetBoolean("isOEMDefaultWallpaper", IsOEMDefaultWallpaper());
  dict->SetString("canceledWallpaper",
                  wallpaper_api_util::kCancelWallpaperMessage);
  return true;
}

bool WallpaperPrivateGetSyncSettingFunction::RunSync() {
  Profile* profile =  Profile::FromBrowserContext(browser_context());
  ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  base::DictionaryValue* dict = new base::DictionaryValue();
  SetResult(dict);
  dict->SetBoolean("syncThemes",
                   sync->GetActiveDataTypes().Has(syncer::THEMES));
  return true;
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
  chromeos::WallpaperManager::WallpaperResolution resolution =
      chromeos::WallpaperManager::Get()->GetAppropriateResolution();

  std::string file_name = GURL(params->url).ExtractFileName();
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS,
                         &wallpaper_path));
  fallback_path = wallpaper_path.Append(file_name);
  if (params->layout != wallpaper_base::WALLPAPER_LAYOUT_STRETCH &&
      resolution == chromeos::WallpaperManager::WALLPAPER_RESOLUTION_SMALL) {
    file_name = base::FilePath(file_name)
                    .InsertBeforeExtension(wallpaper::kSmallWallpaperSuffix)
                    .value();
  }
  wallpaper_path = wallpaper_path.Append(file_name);

  sequence_token_ = BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
      wallpaper::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(
          &WallpaperPrivateSetWallpaperIfExistsFunction::
              ReadFileAndInitiateStartDecode,
          this, wallpaper_path, fallback_path));
  return true;
}

void WallpaperPrivateSetWallpaperIfExistsFunction::
    ReadFileAndInitiateStartDecode(const base::FilePath& file_path,
                                   const base::FilePath& fallback_path) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  base::FilePath path = file_path;

  if (!base::PathExists(file_path))
    path = fallback_path;

  std::string data;
  if (base::PathExists(path) &&
      base::ReadFileToString(path, &data)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateSetWallpaperIfExistsFunction::StartDecode,
                   this, std::vector<char>(data.begin(), data.end())));
    return;
  }
  std::string error = base::StringPrintf(
        "Failed to set wallpaper %s from file system.",
        path.BaseName().value().c_str());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WallpaperPrivateSetWallpaperIfExistsFunction::OnFileNotExists,
                 this, error));
}

void WallpaperPrivateSetWallpaperIfExistsFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& image) {
  // Set unsafe_wallpaper_decoder_ to null since the decoding already finished.
  unsafe_wallpaper_decoder_ = NULL;

  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  wallpaper::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));

  bool update_wallpaper =
      account_id_ ==
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  wallpaper_manager->SetWallpaperFromImageSkia(account_id_, image, layout,
                                               update_wallpaper);
  bool is_persistent = !user_manager::UserManager::Get()
                            ->IsCurrentUserNonCryptohomeDataEphemeral();
  wallpaper::WallpaperInfo info = {params->url,
                                   layout,
                                   user_manager::User::ONLINE,
                                   base::Time::Now().LocalMidnight()};
  wallpaper_manager->SetUserWallpaperInfo(account_id_, info, is_persistent);
  SetResult(new base::FundamentalValue(true));
  Profile* profile = Profile::FromBrowserContext(browser_context());
  // This API is only available to the component wallpaper picker. We do not
  // need to show the app's name if it is the component wallpaper picker. So set
  // the pref to empty string.
  profile->GetPrefs()->SetString(prefs::kCurrentWallpaperAppName,
                                 std::string());
  SendResponse(true);
}

void WallpaperPrivateSetWallpaperIfExistsFunction::OnFileNotExists(
    const std::string& error) {
  SetResult(new base::FundamentalValue(false));
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

  sequence_token_ = BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
      wallpaper::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::BLOCK_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperPrivateSetWallpaperFunction::SaveToFile, this));
}

void WallpaperPrivateSetWallpaperFunction::SaveToFile() {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  std::string file_name = GURL(params->url).ExtractFileName();
  if (SaveData(chrome::DIR_CHROMEOS_WALLPAPERS, file_name, params->wallpaper)) {
    wallpaper_.EnsureRepsForSupportedScales();
    scoped_ptr<gfx::ImageSkia> deep_copy(wallpaper_.DeepCopy());
    // ImageSkia is not RefCountedThreadSafe. Use a deep copied ImageSkia if
    // post to another thread.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateSetWallpaperFunction::SetDecodedWallpaper,
                   this, base::Passed(std::move(deep_copy))));

    base::FilePath wallpaper_dir;
    CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
    base::FilePath file_path =
        wallpaper_dir.Append(file_name)
            .InsertBeforeExtension(wallpaper::kSmallWallpaperSuffix);
    if (base::PathExists(file_path))
      return;
    // Generates and saves small resolution wallpaper. Uses CENTER_CROPPED to
    // maintain the aspect ratio after resize.
    chromeos::WallpaperManager::Get()->ResizeAndSaveWallpaper(
        wallpaper_, file_path, wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
        wallpaper::kSmallWallpaperMaxWidth, wallpaper::kSmallWallpaperMaxHeight,
        NULL);
  } else {
    std::string error = base::StringPrintf(
        "Failed to create/write wallpaper to %s.", file_name.c_str());
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateSetWallpaperFunction::OnFailure,
                   this, error));
  }
}

void WallpaperPrivateSetWallpaperFunction::SetDecodedWallpaper(
    scoped_ptr<gfx::ImageSkia> image) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();

  wallpaper::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));

  bool update_wallpaper =
      account_id_ ==
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  wallpaper_manager->SetWallpaperFromImageSkia(account_id_, *image.get(),
                                               layout, update_wallpaper);

  bool is_persistent = !user_manager::UserManager::Get()
                            ->IsCurrentUserNonCryptohomeDataEphemeral();
  wallpaper::WallpaperInfo info = {params->url,
                                   layout,
                                   user_manager::User::ONLINE,
                                   base::Time::Now().LocalMidnight()};
  Profile* profile = Profile::FromBrowserContext(browser_context());
  // This API is only available to the component wallpaper picker. We do not
  // need to show the app's name if it is the component wallpaper picker. So set
  // the pref to empty string.
  profile->GetPrefs()->SetString(prefs::kCurrentWallpaperAppName,
                                 std::string());
  wallpaper_manager->SetUserWallpaperInfo(account_id_, info, is_persistent);
  SendResponse(true);
}

WallpaperPrivateResetWallpaperFunction::
    WallpaperPrivateResetWallpaperFunction() {}

WallpaperPrivateResetWallpaperFunction::
    ~WallpaperPrivateResetWallpaperFunction() {}

bool WallpaperPrivateResetWallpaperFunction::RunAsync() {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const AccountId& account_id = user_manager->GetActiveUser()->GetAccountId();
  wallpaper_manager->RemoveUserWallpaperInfo(account_id);

  wallpaper::WallpaperInfo info = {std::string(),
                                   wallpaper::WALLPAPER_LAYOUT_CENTER,
                                   user_manager::User::DEFAULT,
                                   base::Time::Now().LocalMidnight()};
  bool is_persistent =
      !user_manager->IsCurrentUserNonCryptohomeDataEphemeral();
  wallpaper_manager->SetUserWallpaperInfo(account_id, info, is_persistent);

  wallpaper_manager->SetDefaultWallpaperNow(account_id);
  Profile* profile = Profile::FromBrowserContext(browser_context());
  // This API is only available to the component wallpaper picker. We do not
  // need to show the app's name if it is the component wallpaper picker. So set
  // the pref to empty string.
  profile->GetPrefs()->SetString(prefs::kCurrentWallpaperAppName,
                                 std::string());
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
  user_id_hash_ = user->username_hash();

  StartDecode(params->wallpaper);

  return true;
}

void WallpaperPrivateSetCustomWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& image) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  base::FilePath thumbnail_path = wallpaper_manager->GetCustomWallpaperPath(
      wallpaper::kThumbnailWallpaperSubDir, user_id_hash_, params->file_name);

  sequence_token_ = BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
      wallpaper::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::BLOCK_SHUTDOWN);

  wallpaper::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));
  wallpaper_api_util::RecordCustomWallpaperLayout(layout);

  bool update_wallpaper =
      account_id_ ==
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  wallpaper_manager->SetCustomWallpaper(
      account_id_, user_id_hash_, params->file_name, layout,
      user_manager::User::CUSTOMIZED, image, update_wallpaper);
  unsafe_wallpaper_decoder_ = NULL;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  // TODO(xdai): This preference is unused now. For compatiblity concern, we
  // need to keep it until it's safe to clean it up.
  profile->GetPrefs()->SetString(prefs::kCurrentWallpaperAppName,
                                 std::string());

  if (params->generate_thumbnail) {
    image.EnsureRepsForSupportedScales();
    scoped_ptr<gfx::ImageSkia> deep_copy(image.DeepCopy());
    // Generates thumbnail before call api function callback. We can then
    // request thumbnail in the javascript callback.
    task_runner->PostTask(FROM_HERE,
        base::Bind(
            &WallpaperPrivateSetCustomWallpaperFunction::GenerateThumbnail,
            this, thumbnail_path, base::Passed(&deep_copy)));
  } else {
    SendResponse(true);
  }
}

void WallpaperPrivateSetCustomWallpaperFunction::GenerateThumbnail(
    const base::FilePath& thumbnail_path, scoped_ptr<gfx::ImageSkia> image) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  if (!base::PathExists(thumbnail_path.DirName()))
    base::CreateDirectory(thumbnail_path.DirName());

  scoped_refptr<base::RefCountedBytes> data;
  chromeos::WallpaperManager::Get()->ResizeImage(
      *image, wallpaper::WALLPAPER_LAYOUT_STRETCH,
      wallpaper::kWallpaperThumbnailWidth, wallpaper::kWallpaperThumbnailHeight,
      &data, NULL);
  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &WallpaperPrivateSetCustomWallpaperFunction::ThumbnailGenerated,
            this, data));
}

void WallpaperPrivateSetCustomWallpaperFunction::ThumbnailGenerated(
    base::RefCountedBytes* data) {
  BinaryValue* result = BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(data->front()), data->size());
  SetResult(result);
  SendResponse(true);
}

WallpaperPrivateSetCustomWallpaperLayoutFunction::
    WallpaperPrivateSetCustomWallpaperLayoutFunction() {}

WallpaperPrivateSetCustomWallpaperLayoutFunction::
    ~WallpaperPrivateSetCustomWallpaperLayoutFunction() {}

bool WallpaperPrivateSetCustomWallpaperLayoutFunction::RunAsync() {
  scoped_ptr<set_custom_wallpaper_layout::Params> params(
      set_custom_wallpaper_layout::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  wallpaper::WallpaperInfo info;
  wallpaper_manager->GetLoggedInUserWallpaperInfo(&info);
  if (info.type != user_manager::User::CUSTOMIZED) {
    SetError("Only custom wallpaper can change layout.");
    SendResponse(false);
    return false;
  }
  info.layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));
  wallpaper_api_util::RecordCustomWallpaperLayout(info.layout);

  const AccountId& account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  bool is_persistent = !user_manager::UserManager::Get()
                            ->IsCurrentUserNonCryptohomeDataEphemeral();
  wallpaper_manager->SetUserWallpaperInfo(account_id, info, is_persistent);
  wallpaper_manager->UpdateWallpaper(false /* clear_cache */);
  SendResponse(true);

  return true;
}

WallpaperPrivateMinimizeInactiveWindowsFunction::
    WallpaperPrivateMinimizeInactiveWindowsFunction() {
}

WallpaperPrivateMinimizeInactiveWindowsFunction::
    ~WallpaperPrivateMinimizeInactiveWindowsFunction() {
}

bool WallpaperPrivateMinimizeInactiveWindowsFunction::RunAsync() {
  WindowStateManager::MinimizeInactiveWindows(
      user_manager::UserManager::Get()->GetActiveUser()->username_hash());
  return true;
}

WallpaperPrivateRestoreMinimizedWindowsFunction::
    WallpaperPrivateRestoreMinimizedWindowsFunction() {
}

WallpaperPrivateRestoreMinimizedWindowsFunction::
    ~WallpaperPrivateRestoreMinimizedWindowsFunction() {
}

bool WallpaperPrivateRestoreMinimizedWindowsFunction::RunAsync() {
  WindowStateManager::RestoreWindows(
      user_manager::UserManager::Get()->GetActiveUser()->username_hash());
  return true;
}

WallpaperPrivateGetThumbnailFunction::WallpaperPrivateGetThumbnailFunction() {
}

WallpaperPrivateGetThumbnailFunction::~WallpaperPrivateGetThumbnailFunction() {
}

bool WallpaperPrivateGetThumbnailFunction::RunAsync() {
  scoped_ptr<get_thumbnail::Params> params(
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
      SendResponse(false);
      return false;
    }

    // TODO(bshe): Small resolution wallpaper is used here as wallpaper
    // thumbnail. We should either resize it or include a wallpaper thumbnail in
    // addition to large and small wallpaper resolutions.
    thumbnail_path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        chromeos::switches::kDefaultWallpaperSmall);
  }

  sequence_token_ = BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
      wallpaper::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperPrivateGetThumbnailFunction::Get, this,
                 thumbnail_path));
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
  BinaryValue* thumbnail = BinaryValue::CreateWithCopiedBuffer(data.c_str(),
                                                               data.size());
  SetResult(thumbnail);
  SendResponse(true);
}

void WallpaperPrivateGetThumbnailFunction::Get(const base::FilePath& path) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  std::string data;
  if (GetData(path, &data)) {
    if (data.empty()) {
      BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateGetThumbnailFunction::FileNotLoaded, this));
    } else {
      BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateGetThumbnailFunction::FileLoaded, this,
                   data));
    }
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WallpaperPrivateGetThumbnailFunction::Failure, this,
                   path.BaseName().value()));
  }
}

WallpaperPrivateSaveThumbnailFunction::WallpaperPrivateSaveThumbnailFunction() {
}

WallpaperPrivateSaveThumbnailFunction::
    ~WallpaperPrivateSaveThumbnailFunction() {}

bool WallpaperPrivateSaveThumbnailFunction::RunAsync() {
  scoped_ptr<save_thumbnail::Params> params(
      save_thumbnail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  sequence_token_ = BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
      wallpaper::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperPrivateSaveThumbnailFunction::Save,
                 this, params->data, GURL(params->url).ExtractFileName()));
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
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  if (SaveData(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS, file_name, data)) {
    BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WallpaperPrivateSaveThumbnailFunction::Success, this));
  } else {
    BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&WallpaperPrivateSaveThumbnailFunction::Failure,
                     this, file_name));
  }
}

WallpaperPrivateGetOfflineWallpaperListFunction::
    WallpaperPrivateGetOfflineWallpaperListFunction() {
}

WallpaperPrivateGetOfflineWallpaperListFunction::
    ~WallpaperPrivateGetOfflineWallpaperListFunction() {
}

bool WallpaperPrivateGetOfflineWallpaperListFunction::RunAsync() {
  sequence_token_ = BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
      wallpaper::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  task_runner->PostTask(FROM_HERE,
      base::Bind(&WallpaperPrivateGetOfflineWallpaperListFunction::GetList,
                 this));
  return true;
}

void WallpaperPrivateGetOfflineWallpaperListFunction::GetList() {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
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
      if (!base::EndsWith(file_name, wallpaper::kSmallWallpaperSuffix,
                          base::CompareCase::SENSITIVE))
        file_list.push_back(current.BaseName().value());
    }
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WallpaperPrivateGetOfflineWallpaperListFunction::OnComplete,
                 this, file_list));
}

void WallpaperPrivateGetOfflineWallpaperListFunction::OnComplete(
    const std::vector<std::string>& file_list) {
  base::ListValue* results = new base::ListValue();
  results->AppendStrings(file_list);
  SetResult(results);
  SendResponse(true);
}
