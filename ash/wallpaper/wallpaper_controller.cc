// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/display/window_tree_host_manager.h"
#include "ash/login/ui/login_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wallpaper/wallpaper_controller_observer.h"
#include "ash/wallpaper/wallpaper_decoder.h"
#include "ash/wallpaper/wallpaper_delegate.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/values.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/wallpaper/wallpaper_color_calculator.h"
#include "components/wallpaper/wallpaper_color_profile.h"
#include "components/wallpaper/wallpaper_resizer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"

using color_utils::ColorProfile;
using color_utils::LumaRange;
using color_utils::SaturationRange;
using wallpaper::ColorProfileType;
using wallpaper::WallpaperInfo;

namespace ash {

namespace {

// Names of nodes with wallpaper info in |kUserWallpaperInfo| dictionary.
const char kNewWallpaperDateNodeName[] = "date";
const char kNewWallpaperLayoutNodeName[] = "layout";
const char kNewWallpaperLocationNodeName[] = "file";
const char kNewWallpaperTypeNodeName[] = "type";

// The directory and file name to save the downloaded device policy controlled
// wallpaper.
const char kDeviceWallpaperDir[] = "device_wallpaper";
const char kDeviceWallpaperFile[] = "device_wallpaper_image.jpg";

// How long to wait reloading the wallpaper after the display size has changed.
constexpr int kWallpaperReloadDelayMs = 100;

// How long to wait for resizing of the the wallpaper.
constexpr int kCompositorLockTimeoutMs = 750;

// Caches color calculation results in local state pref service.
void CacheProminentColors(const std::vector<SkColor>& colors,
                          const std::string& current_location) {
  // Local state can be null in tests.
  if (!Shell::Get()->GetLocalStatePrefService())
    return;
  DictionaryPrefUpdate wallpaper_colors_update(
      Shell::Get()->GetLocalStatePrefService(), prefs::kWallpaperColors);
  auto wallpaper_colors = std::make_unique<base::ListValue>();
  for (SkColor color : colors)
    wallpaper_colors->AppendDouble(static_cast<double>(color));
  wallpaper_colors_update->SetWithoutPathExpansion(current_location,
                                                   std::move(wallpaper_colors));
}

// Gets prominent color cache from local state pref service. Returns an empty
// value if cache is not available.
base::Optional<std::vector<SkColor>> GetCachedColors(
    const std::string& current_location) {
  base::Optional<std::vector<SkColor>> cached_colors_out;
  const base::ListValue* prominent_colors = nullptr;
  // Local state can be null in tests.
  if (!Shell::Get()->GetLocalStatePrefService() ||
      !Shell::Get()
           ->GetLocalStatePrefService()
           ->GetDictionary(prefs::kWallpaperColors)
           ->GetListWithoutPathExpansion(current_location, &prominent_colors)) {
    return cached_colors_out;
  }
  cached_colors_out = std::vector<SkColor>();
  for (base::ListValue::const_iterator iter = prominent_colors->begin();
       iter != prominent_colors->end(); ++iter) {
    cached_colors_out.value().push_back(
        static_cast<SkColor>(iter->GetDouble()));
  }
  return cached_colors_out;
}

// Returns true if a color should be extracted from the wallpaper based on the
// command kAshShelfColor line arg.
bool IsShelfColoringEnabled() {
  const bool kDefaultValue = true;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshShelfColor)) {
    return kDefaultValue;
  }

  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAshShelfColor);
  if (switch_value != switches::kAshShelfColorEnabled &&
      switch_value != switches::kAshShelfColorDisabled) {
    LOG(WARNING) << "Invalid '--" << switches::kAshShelfColor << "' value of '"
                 << switch_value << "'. Defaulting to "
                 << (kDefaultValue ? "enabled." : "disabled.");
    return kDefaultValue;
  }

  return switch_value == switches::kAshShelfColorEnabled;
}

// Gets the color profiles for extracting wallpaper prominent colors.
std::vector<ColorProfile> GetProminentColorProfiles() {
  return {ColorProfile(LumaRange::DARK, SaturationRange::VIBRANT),
          ColorProfile(LumaRange::NORMAL, SaturationRange::VIBRANT),
          ColorProfile(LumaRange::LIGHT, SaturationRange::VIBRANT),
          ColorProfile(LumaRange::DARK, SaturationRange::MUTED),
          ColorProfile(LumaRange::NORMAL, SaturationRange::MUTED),
          ColorProfile(LumaRange::LIGHT, SaturationRange::MUTED)};
}

// Gets the corresponding color profile type based on the given
// |color_profile|.
ColorProfileType GetColorProfileType(ColorProfile color_profile) {
  if (color_profile.saturation == SaturationRange::VIBRANT) {
    switch (color_profile.luma) {
      case LumaRange::DARK:
        return ColorProfileType::DARK_VIBRANT;
      case LumaRange::NORMAL:
        return ColorProfileType::NORMAL_VIBRANT;
      case LumaRange::LIGHT:
        return ColorProfileType::LIGHT_VIBRANT;
    }
  } else {
    switch (color_profile.luma) {
      case LumaRange::DARK:
        return ColorProfileType::DARK_MUTED;
      case LumaRange::NORMAL:
        return ColorProfileType::NORMAL_MUTED;
      case LumaRange::LIGHT:
        return ColorProfileType::LIGHT_MUTED;
    }
  }
  NOTREACHED();
  return ColorProfileType::DARK_MUTED;
}

// Deletes a list of wallpaper files in |file_list|.
void DeleteWallpaperInList(const std::vector<base::FilePath>& file_list) {
  for (const base::FilePath& path : file_list) {
    if (!base::DeleteFile(path, true))
      LOG(ERROR) << "Failed to remove user wallpaper at " << path.value();
  }
}

}  // namespace

const SkColor WallpaperController::kInvalidColor = SK_ColorTRANSPARENT;

base::FilePath WallpaperController::dir_user_data_path_;
base::FilePath WallpaperController::dir_chrome_os_wallpapers_path_;
base::FilePath WallpaperController::dir_chrome_os_custom_wallpapers_path_;

const char WallpaperController::kSmallWallpaperSubDir[] = "small";
const char WallpaperController::kLargeWallpaperSubDir[] = "large";
const char WallpaperController::kOriginalWallpaperSubDir[] = "original";
const char WallpaperController::kThumbnailWallpaperSubDir[] = "thumb";

const int WallpaperController::kSmallWallpaperMaxWidth = 1366;
const int WallpaperController::kSmallWallpaperMaxHeight = 800;
const int WallpaperController::kLargeWallpaperMaxWidth = 2560;
const int WallpaperController::kLargeWallpaperMaxHeight = 1700;

const int WallpaperController::kWallpaperThumbnailWidth = 108;
const int WallpaperController::kWallpaperThumbnailHeight = 68;

const SkColor WallpaperController::kDefaultWallpaperColor = SK_ColorGRAY;

WallpaperController::MovableOnDestroyCallback::MovableOnDestroyCallback(
    const base::Closure& callback)
    : callback_(callback) {}

WallpaperController::MovableOnDestroyCallback::~MovableOnDestroyCallback() {
  if (!callback_.is_null())
    callback_.Run();
}

WallpaperController::WallpaperController()
    : locked_(false),
      wallpaper_mode_(WALLPAPER_NONE),
      color_profiles_(GetProminentColorProfiles()),
      wallpaper_reload_delay_(kWallpaperReloadDelayMs),
      sequenced_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      scoped_session_observer_(this),
      weak_factory_(this) {
  prominent_colors_ =
      std::vector<SkColor>(color_profiles_.size(), kInvalidColor);
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
}

WallpaperController::~WallpaperController() {
  if (current_wallpaper_)
    current_wallpaper_->RemoveObserver(this);
  if (color_calculator_)
    color_calculator_->RemoveObserver(this);
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  weak_factory_.InvalidateWeakPtrs();
}

// static
void WallpaperController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kUserWallpaperInfo);
  registry->RegisterDictionaryPref(prefs::kWallpaperColors);
}

// static
gfx::Size WallpaperController::GetMaxDisplaySizeInNative() {
  // Return an empty size for test environments where the screen is null.
  if (!display::Screen::GetScreen())
    return gfx::Size();

  gfx::Size max;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    // Use the native size, not ManagedDisplayInfo::size_in_pixel or
    // Display::size.
    // TODO(msw): Avoid using Display::size here; see http://crbug.com/613657.
    gfx::Size size = display.size();
    if (Shell::HasInstance()) {
      display::ManagedDisplayInfo info =
          Shell::Get()->display_manager()->GetDisplayInfo(display.id());
      // TODO(mash): Mash returns a fake ManagedDisplayInfo. crbug.com/622480
      if (info.id() == display.id())
        size = info.bounds_in_native().size();
    }
    if (display.rotation() == display::Display::ROTATE_90 ||
        display.rotation() == display::Display::ROTATE_270) {
      size = gfx::Size(size.height(), size.width());
    }
    max.SetToMax(size);
  }

  return max;
}

// static
WallpaperController::WallpaperResolution
WallpaperController::GetAppropriateResolution() {
  gfx::Size size = GetMaxDisplaySizeInNative();
  return (size.width() > kSmallWallpaperMaxWidth ||
          size.height() > kSmallWallpaperMaxHeight)
             ? WALLPAPER_RESOLUTION_LARGE
             : WALLPAPER_RESOLUTION_SMALL;
}

// static
void WallpaperController::DecodeWallpaperIfApplicable(
    LoadedCallback callback,
    std::unique_ptr<std::string> data,
    bool data_is_ready) {
  // The connector for the mojo service manager is null in unit tests.
  if (!data_is_ready || !Shell::Get()->shell_delegate()->GetShellConnector()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::Passed(std::make_unique<user_manager::UserImage>())));
  } else {
    DecodeWallpaper(std::move(data), std::move(callback));
  }
}

// static
void WallpaperController::ReadAndDecodeWallpaper(
    LoadedCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::FilePath& file_path) {
  std::string* data = new std::string;
  base::PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::Bind(&base::ReadFileToString, file_path, data),
      base::Bind(&DecodeWallpaperIfApplicable, std::move(callback),
                 base::Passed(base::WrapUnique(data))));
}

// static
gfx::ImageSkia WallpaperController::CreateSolidColorWallpaper() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(kDefaultWallpaperColor);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// static
bool WallpaperController::CreateJPEGImageForTesting(
    int width,
    int height,
    SkColor color,
    std::vector<unsigned char>* output) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);

  constexpr int kQuality = 80;
  if (!gfx::JPEGCodec::Encode(bitmap, kQuality, output)) {
    LOG(ERROR) << "Unable to encode " << width << "x" << height << " bitmap";
    return false;
  }
  return true;
}

// static
bool WallpaperController::WriteJPEGFileForTesting(const base::FilePath& path,
                                                  int width,
                                                  int height,
                                                  SkColor color) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::vector<unsigned char> output;
  if (!CreateJPEGImageForTesting(width, height, color, &output))
    return false;

  size_t bytes_written = base::WriteFile(
      path, reinterpret_cast<const char*>(&output[0]), output.size());
  if (bytes_written != output.size()) {
    LOG(ERROR) << "Wrote " << bytes_written << " byte(s) instead of "
               << output.size() << " to " << path.value();
    return false;
  }
  return true;
}

// static
base::FilePath WallpaperController::GetDevicePolicyWallpaperFilePath() {
  DCHECK(!dir_chrome_os_wallpapers_path_.empty());
  return dir_chrome_os_wallpapers_path_.Append(kDeviceWallpaperDir)
      .Append(kDeviceWallpaperFile);
}

void WallpaperController::BindRequest(
    mojom::WallpaperControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

gfx::ImageSkia WallpaperController::GetWallpaper() const {
  if (current_wallpaper_)
    return current_wallpaper_->image();
  return gfx::ImageSkia();
}

uint32_t WallpaperController::GetWallpaperOriginalImageId() const {
  if (current_wallpaper_)
    return current_wallpaper_->original_image_id();
  return 0;
}

void WallpaperController::AddObserver(WallpaperControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void WallpaperController::RemoveObserver(
    WallpaperControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

SkColor WallpaperController::GetProminentColor(
    ColorProfile color_profile) const {
  ColorProfileType type = GetColorProfileType(color_profile);
  return prominent_colors_[static_cast<int>(type)];
}

wallpaper::WallpaperLayout WallpaperController::GetWallpaperLayout() const {
  if (current_wallpaper_)
    return current_wallpaper_->wallpaper_info().layout;
  return wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
}

void WallpaperController::SetDefaultWallpaperImpl(
    const AccountId& account_id,
    const user_manager::UserType& user_type,
    bool show_wallpaper,
    MovableOnDestroyCallbackHolder on_finish) {
  // There is no visible wallpaper in kiosk mode.
  base::Optional<user_manager::UserType> active_user_type =
      Shell::Get()->session_controller()->GetUserType();
  // |active_user_type| is empty when there's no active user.
  if (active_user_type &&
      *active_user_type == user_manager::USER_TYPE_KIOSK_APP) {
    return;
  }

  wallpaper_cache_map_.erase(account_id);

  const bool use_small =
      (GetAppropriateResolution() == WALLPAPER_RESOLUTION_SMALL);
  wallpaper::WallpaperLayout layout =
      use_small ? wallpaper::WALLPAPER_LAYOUT_CENTER
                : wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::FilePath file_path;

  // The wallpaper is determined in the following order:
  // Guest wallpaper, child wallpaper, customized default wallpaper, and regular
  // default wallpaper.
  // TODO(wzang|xdai): The current code intentionally distinguishes between
  // |active_user_type| and |user_type|. We should try to unify them.
  if (active_user_type && *active_user_type == user_manager::USER_TYPE_GUEST) {
    const std::string switch_string =
        use_small ? chromeos::switches::kGuestWallpaperSmall
                  : chromeos::switches::kGuestWallpaperLarge;
    file_path = command_line->GetSwitchValuePath(switch_string);
  } else if (user_type == user_manager::USER_TYPE_CHILD) {
    const std::string switch_string =
        use_small ? chromeos::switches::kChildWallpaperSmall
                  : chromeos::switches::kChildWallpaperLarge;
    file_path = command_line->GetSwitchValuePath(switch_string);
  } else if (!customized_default_wallpaper_small_.empty()) {
    DCHECK(!customized_default_wallpaper_large_.empty());
    file_path = use_small ? customized_default_wallpaper_small_
                          : customized_default_wallpaper_large_;
  } else {
    const std::string switch_string =
        use_small ? chromeos::switches::kDefaultWallpaperSmall
                  : chromeos::switches::kDefaultWallpaperLarge;
    file_path = command_line->GetSwitchValuePath(switch_string);
  }

  // We need to decode the image if the cached default wallpaper doesn't exist,
  // or if the two file paths don't match; otherwise, directly run the callback
  // with the cached decoded image.
  if (!default_wallpaper_image_.get() ||
      default_wallpaper_image_->file_path() != file_path) {
    default_wallpaper_image_.reset();
    ReadAndDecodeWallpaper(
        base::Bind(&WallpaperController::OnDefaultWallpaperDecoded,
                   weak_factory_.GetWeakPtr(), file_path, layout,
                   show_wallpaper, base::Passed(std::move(on_finish))),
        sequenced_task_runner_, file_path);
    num_decode_request_for_testing_++;
  } else {
    OnDefaultWallpaperDecoded(file_path, layout, show_wallpaper,
                              std::move(on_finish),
                              std::move(default_wallpaper_image_));
  }
}

void WallpaperController::SetCustomizedDefaultWallpaperImpl(
    const base::FilePath& customized_default_wallpaper_file_small,
    std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
    const base::FilePath& customized_default_wallpaper_file_large,
    std::unique_ptr<gfx::ImageSkia> large_wallpaper_image) {
  customized_default_wallpaper_small_ = customized_default_wallpaper_file_small;
  customized_default_wallpaper_large_ = customized_default_wallpaper_file_large;

  // |show_wallpaper| is true if the previous default wallpaper is visible now,
  // so we need to update wallpaper on the screen. Layout is ignored here, so
  // wallpaper::WALLPAPER_LAYOUT_CENTER is used as a placeholder only.
  const bool show_wallpaper =
      default_wallpaper_image_.get() &&
      WallpaperIsAlreadyLoaded(default_wallpaper_image_->image(),
                               false /*compare_layouts=*/,
                               wallpaper::WALLPAPER_LAYOUT_CENTER);

  default_wallpaper_image_.reset();
  if (GetAppropriateResolution() == WALLPAPER_RESOLUTION_SMALL) {
    if (small_wallpaper_image) {
      default_wallpaper_image_.reset(
          new user_manager::UserImage(*small_wallpaper_image));
      default_wallpaper_image_->set_file_path(
          customized_default_wallpaper_file_small);
    }
  } else {
    if (large_wallpaper_image) {
      default_wallpaper_image_.reset(
          new user_manager::UserImage(*large_wallpaper_image));
      default_wallpaper_image_->set_file_path(
          customized_default_wallpaper_file_large);
    }
  }

  // Customized default wallpapers are subject to the same restrictions as other
  // default wallpapers, e.g. they should not be set during guest sessions.
  // TODO(crbug.com/776464): Find a way to directly set wallpaper from here, or
  // combine this with |SetDefaultWallpaperImpl| since there's duplicate code.
  SetDefaultWallpaperImpl(EmptyAccountId(), user_manager::USER_TYPE_REGULAR,
                          show_wallpaper, MovableOnDestroyCallbackHolder());
}

void WallpaperController::SetWallpaperImage(const gfx::ImageSkia& image,
                                            const WallpaperInfo& info) {
  wallpaper::WallpaperLayout layout = info.layout;
  VLOG(1) << "SetWallpaper: image_id="
          << wallpaper::WallpaperResizer::GetImageId(image)
          << " layout=" << layout;

  if (WallpaperIsAlreadyLoaded(image, true /*compare_layouts=*/, layout)) {
    VLOG(1) << "Wallpaper is already loaded";
    return;
  }

  current_location_ = info.location;
  // Cancel any in-flight color calculation because we have a new wallpaper.
  if (color_calculator_) {
    color_calculator_->RemoveObserver(this);
    color_calculator_.reset();
  }
  current_wallpaper_.reset(new wallpaper::WallpaperResizer(
      image, GetMaxDisplaySizeInNative(), info, sequenced_task_runner_));
  current_wallpaper_->AddObserver(this);
  current_wallpaper_->StartResize();

  for (auto& observer : observers_)
    observer.OnWallpaperDataChanged();
  wallpaper_mode_ = WALLPAPER_IMAGE;
  InstallDesktopControllerForAllWindows();
  wallpaper_count_for_testing_++;
}

void WallpaperController::CreateEmptyWallpaper() {
  SetProminentColors(
      std::vector<SkColor>(color_profiles_.size(), kInvalidColor));
  current_wallpaper_.reset();
  wallpaper_mode_ = WALLPAPER_IMAGE;
  InstallDesktopControllerForAllWindows();
}

base::FilePath WallpaperController::GetCustomWallpaperDir(
    const std::string& sub_dir) {
  DCHECK(!dir_chrome_os_custom_wallpapers_path_.empty());
  return dir_chrome_os_custom_wallpapers_path_.Append(sub_dir);
}

void WallpaperController::PrepareWallpaperForLockScreenChange(bool locking) {
  bool needs_blur = locking && IsBlurEnabled();
  if (needs_blur != is_wallpaper_blurred_) {
    for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
      WallpaperWidgetController* wallpaper_widget_controller =
          root_window_controller->wallpaper_widget_controller();
      if (wallpaper_widget_controller) {
        wallpaper_widget_controller->SetWallpaperBlur(
            needs_blur ? login_constants::kBlurSigma
                       : login_constants::kClearBlurSigma);
      }
    }
    is_wallpaper_blurred_ = needs_blur;
    // TODO(crbug.com/776464): Replace the observer with mojo calls so that
    // it works under mash and it's easier to add tests.
    for (auto& observer : observers_)
      observer.OnWallpaperBlurChanged();
  }
}

void WallpaperController::OnDisplayConfigurationChanged() {
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (wallpaper_mode_ == WALLPAPER_IMAGE && current_wallpaper_) {
      timer_.Stop();
      GetInternalDisplayCompositorLock();
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(wallpaper_reload_delay_),
                   base::Bind(&WallpaperController::UpdateWallpaper,
                              base::Unretained(this), false /* clear cache */));
    }
  }
}

void WallpaperController::OnRootWindowAdded(aura::Window* root_window) {
  // The wallpaper hasn't been set yet.
  if (wallpaper_mode_ == WALLPAPER_NONE)
    return;

  // Handle resolution change for "built-in" images.
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (wallpaper_mode_ == WALLPAPER_IMAGE && current_wallpaper_)
      UpdateWallpaper(true /* clear cache */);
  }

  InstallDesktopController(root_window);
}

void WallpaperController::OnLocalStatePrefServiceInitialized(
    PrefService* pref_service) {
  Shell::Get()->wallpaper_delegate()->InitializeWallpaper();
}

void WallpaperController::OnSessionStateChanged(
    session_manager::SessionState state) {
  CalculateWallpaperColors();

  // The wallpaper may be dimmed/blurred based on session state. The color of
  // the dimming overlay depends on the prominent color cached from a previous
  // calculation, or a default color if cache is not available. It should never
  // depend on any in-flight color calculation.
  if (wallpaper_mode_ == WALLPAPER_IMAGE &&
      (state == session_manager::SessionState::ACTIVE ||
       state == session_manager::SessionState::LOCKED ||
       state == session_manager::SessionState::LOGIN_SECONDARY)) {
    // TODO(crbug.com/753518): Reuse the existing WallpaperWidgetController for
    // dimming/blur purpose.
    InstallDesktopControllerForAllWindows();
  }

  if (state == session_manager::SessionState::ACTIVE)
    MoveToUnlockedContainer();
  else
    MoveToLockedContainer();
}

bool WallpaperController::WallpaperIsAlreadyLoaded(
    const gfx::ImageSkia& image,
    bool compare_layouts,
    wallpaper::WallpaperLayout layout) const {
  if (!current_wallpaper_)
    return false;

  // Compare layouts only if necessary.
  if (compare_layouts && layout != current_wallpaper_->wallpaper_info().layout)
    return false;

  return wallpaper::WallpaperResizer::GetImageId(image) ==
         current_wallpaper_->original_image_id();
}

void WallpaperController::OpenSetWallpaperPage() {
  if (wallpaper_controller_client_ &&
      Shell::Get()->wallpaper_delegate()->CanOpenSetWallpaperPage()) {
    wallpaper_controller_client_->OpenWallpaperPicker();
  }
}

bool WallpaperController::ShouldApplyDimming() const {
  return Shell::Get()->session_controller()->IsUserSessionBlocked() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshDisableLoginDimAndBlur);
}

bool WallpaperController::IsBlurEnabled() const {
  return !IsDevicePolicyWallpaper() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshDisableLoginDimAndBlur);
}

void WallpaperController::SetUserWallpaperInfo(const AccountId& account_id,
                                               const WallpaperInfo& info,
                                               bool is_persistent) {
  current_user_wallpaper_info_ = info;
  if (!is_persistent)
    return;

  PrefService* local_state = Shell::Get()->GetLocalStatePrefService();
  // Local state can be null in tests.
  if (!local_state)
    return;
  WallpaperInfo old_info;
  if (GetUserWallpaperInfo(account_id, &old_info, is_persistent)) {
    // Remove the color cache of the previous wallpaper if it exists.
    DictionaryPrefUpdate wallpaper_colors_update(local_state,
                                                 prefs::kWallpaperColors);
    wallpaper_colors_update->RemoveWithoutPathExpansion(old_info.location,
                                                        nullptr);
  }

  DictionaryPrefUpdate wallpaper_update(local_state, prefs::kUserWallpaperInfo);
  auto wallpaper_info_dict = std::make_unique<base::DictionaryValue>();
  wallpaper_info_dict->SetString(
      kNewWallpaperDateNodeName,
      base::Int64ToString(info.date.ToInternalValue()));
  wallpaper_info_dict->SetString(kNewWallpaperLocationNodeName, info.location);
  wallpaper_info_dict->SetInteger(kNewWallpaperLayoutNodeName, info.layout);
  wallpaper_info_dict->SetInteger(kNewWallpaperTypeNodeName, info.type);
  wallpaper_update->SetWithoutPathExpansion(account_id.GetUserEmail(),
                                            std::move(wallpaper_info_dict));
}

bool WallpaperController::GetUserWallpaperInfo(const AccountId& account_id,
                                               WallpaperInfo* info,
                                               bool is_persistent) {
  if (!is_persistent) {
    // Default to the values cached in memory.
    *info = current_user_wallpaper_info_;

    // Ephemeral users do not save anything to local state. But we have got
    // wallpaper info from memory. Returns true.
    return true;
  }

  PrefService* local_state = Shell::Get()->GetLocalStatePrefService();
  // Local state can be null in tests.
  if (!local_state)
    return false;
  const base::DictionaryValue* info_dict;
  if (!local_state->GetDictionary(prefs::kUserWallpaperInfo)
           ->GetDictionaryWithoutPathExpansion(account_id.GetUserEmail(),
                                               &info_dict)) {
    return false;
  }

  // Use temporary variables to keep |info| untouched in the error case.
  std::string location;
  if (!info_dict->GetString(kNewWallpaperLocationNodeName, &location))
    return false;
  int layout;
  if (!info_dict->GetInteger(kNewWallpaperLayoutNodeName, &layout))
    return false;
  int type;
  if (!info_dict->GetInteger(kNewWallpaperTypeNodeName, &type))
    return false;
  std::string date_string;
  if (!info_dict->GetString(kNewWallpaperDateNodeName, &date_string))
    return false;
  int64_t date_val;
  if (!base::StringToInt64(date_string, &date_val))
    return false;

  info->location = location;
  info->layout = static_cast<wallpaper::WallpaperLayout>(layout);
  info->type = static_cast<wallpaper::WallpaperType>(type);
  info->date = base::Time::FromInternalValue(date_val);
  return true;
}

void WallpaperController::InitializeUserWallpaperInfo(
    const AccountId& account_id,
    bool is_persistent) {
  const wallpaper::WallpaperInfo info = {
      std::string(), wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
      wallpaper::DEFAULT, base::Time::Now().LocalMidnight()};
  SetUserWallpaperInfo(account_id, info, is_persistent);
}

bool WallpaperController::GetWallpaperFromCache(const AccountId& account_id,
                                                gfx::ImageSkia* image) {
  CustomWallpaperMap::const_iterator it = wallpaper_cache_map_.find(account_id);
  if (it != wallpaper_cache_map_.end() && !it->second.second.isNull()) {
    *image = it->second.second;
    return true;
  }
  return false;
}

bool WallpaperController::GetPathFromCache(const AccountId& account_id,
                                           base::FilePath* path) {
  CustomWallpaperMap::const_iterator it = wallpaper_cache_map_.find(account_id);
  if (it != wallpaper_cache_map_.end()) {
    *path = (*it).second.first;
    return true;
  }
  return false;
}

wallpaper::WallpaperInfo* WallpaperController::GetCurrentUserWallpaperInfo() {
  return &current_user_wallpaper_info_;
}

CustomWallpaperMap* WallpaperController::GetWallpaperCacheMap() {
  return &wallpaper_cache_map_;
}

void WallpaperController::SetClientAndPaths(
    mojom::WallpaperControllerClientPtr client,
    const base::FilePath& user_data_path,
    const base::FilePath& chromeos_wallpapers_path,
    const base::FilePath& chromeos_custom_wallpapers_path) {
  wallpaper_controller_client_ = std::move(client);
  dir_user_data_path_ = user_data_path;
  dir_chrome_os_wallpapers_path_ = chromeos_wallpapers_path;
  dir_chrome_os_custom_wallpapers_path_ = chromeos_custom_wallpapers_path;
}

void WallpaperController::SetCustomWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    const std::string& file_name,
    wallpaper::WallpaperLayout layout,
    wallpaper::WallpaperType type,
    const SkBitmap& image,
    bool show_wallpaper) {
  NOTIMPLEMENTED();
}

void WallpaperController::SetOnlineWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const SkBitmap& image,
    const std::string& url,
    wallpaper::WallpaperLayout layout,
    bool show_wallpaper) {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());

  // There is no visible wallpaper in kiosk mode.
  base::Optional<user_manager::UserType> active_user_type =
      Shell::Get()->session_controller()->GetUserType();
  // |active_user_type| is empty when there's no active user.
  if (!active_user_type ||
      *active_user_type == user_manager::USER_TYPE_KIOSK_APP) {
    return;
  }

  WallpaperInfo info = {url, layout, wallpaper::ONLINE,
                        base::Time::Now().LocalMidnight()};
  SetUserWallpaperInfo(user_info->account_id, info, !user_info->is_ephemeral);

  if (show_wallpaper) {
    // TODO(crbug.com/776464): This should ideally go through PendingWallpaper.
    SetWallpaper(image, info);
  }

  // Leave the file path empty, because in most cases the file path is not used
  // when fetching cache, but in case it needs to be checked, we should avoid
  // confusing the URL with a real file path.
  wallpaper_cache_map_[user_info->account_id] = CustomWallpaperElement(
      base::FilePath(), gfx::ImageSkia::CreateFrom1xBitmap(image));
}

void WallpaperController::SetDefaultWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    bool show_wallpaper) {
  const AccountId account_id = user_info->account_id;
  const bool is_persistent = !user_info->is_ephemeral;
  const user_manager::UserType type = user_info->type;

  RemoveUserWallpaper(std::move(user_info), wallpaper_files_id);
  InitializeUserWallpaperInfo(account_id, is_persistent);
  if (show_wallpaper) {
    // TODO(crbug.com/776464): This should ideally go through PendingWallpaper.
    // The callback is specific to PendingWallpaper and is left empty for now.
    SetDefaultWallpaperImpl(account_id, type, true /*show_wallpaper=*/,
                            MovableOnDestroyCallbackHolder());
  }
}

void WallpaperController::SetCustomizedDefaultWallpaper(
    const GURL& wallpaper_url,
    const base::FilePath& file_path,
    const base::FilePath& resized_directory) {
  NOTIMPLEMENTED();
}

void WallpaperController::SetDeviceWallpaperPolicyEnforced(bool enforced) {
  bool previous_enforced = is_device_wallpaper_policy_enforced_;
  is_device_wallpaper_policy_enforced_ = enforced;

  if (ShouldSetDevicePolicyWallpaper()) {
    SetDevicePolicyWallpaper();
  } else if ((previous_enforced != enforced) && !enforced) {
    // If the device wallpaper policy is cleared, the wallpaper should revert to
    // the wallpaper of the current user with the large pod in the users list in
    // the login screen. If there is no such user, use the first user in the
    // users list.
    // TODO(xdai): Get the account id from the session controller and then call
    // ShowUserWallpaper() to display it.
  }
}

void WallpaperController::ShowUserWallpaper(
    mojom::WallpaperUserInfoPtr user_info) {
  NOTIMPLEMENTED();
}

void WallpaperController::ShowSigninWallpaper() {
  // TODO(crbug.com/791654): Call |SetDeviceWallpaperIfApplicable| from here.
  SetDefaultWallpaperImpl(EmptyAccountId(), user_manager::USER_TYPE_REGULAR,
                          true /*show_wallpaper=*/,
                          MovableOnDestroyCallbackHolder());
}

void WallpaperController::RemoveUserWallpaper(
    mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id) {
  RemoveUserWallpaperInfo(user_info->account_id, !user_info->is_ephemeral);
  RemoveUserWallpaperImpl(user_info->account_id, wallpaper_files_id);
}

void WallpaperController::SetWallpaper(const SkBitmap& wallpaper,
                                       const WallpaperInfo& info) {
  if (wallpaper.isNull())
    return;
  SetWallpaperImage(gfx::ImageSkia::CreateFrom1xBitmap(wallpaper), info);
}

void WallpaperController::AddObserver(
    mojom::WallpaperObserverAssociatedPtrInfo observer) {
  mojom::WallpaperObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));
  observer_ptr->OnWallpaperColorsChanged(prominent_colors_);
  mojo_observers_.AddPtr(std::move(observer_ptr));
}

void WallpaperController::GetWallpaperColors(
    GetWallpaperColorsCallback callback) {
  std::move(callback).Run(prominent_colors_);
}

void WallpaperController::OnWallpaperResized() {
  CalculateWallpaperColors();
  compositor_lock_.reset();
}

void WallpaperController::OnColorCalculationComplete() {
  const std::vector<SkColor> colors = color_calculator_->prominent_colors();
  color_calculator_.reset();
  // TODO(crbug.com/787134): The prominent colors of wallpapers with empty
  // location should be cached as well.
  if (!current_location_.empty())
    CacheProminentColors(colors, current_location_);
  SetProminentColors(colors);
}

void WallpaperController::ShowDefaultWallpaperForTesting() {
  default_wallpaper_image_.reset(
      new user_manager::UserImage(CreateSolidColorWallpaper()));
  SetWallpaperImage(default_wallpaper_image_->image(),
                    wallpaper::WallpaperInfo(
                        "", wallpaper::WALLPAPER_LAYOUT_STRETCH,
                        wallpaper::DEFAULT, base::Time::Now().LocalMidnight()));
}

void WallpaperController::SetClientForTesting(
    mojom::WallpaperControllerClientPtr client) {
  SetClientAndPaths(std::move(client), base::FilePath(), base::FilePath(),
                    base::FilePath());
}

void WallpaperController::FlushForTesting() {
  if (wallpaper_controller_client_)
    wallpaper_controller_client_.FlushForTesting();
  mojo_observers_.FlushForTesting();
}

void WallpaperController::InstallDesktopController(aura::Window* root_window) {
  WallpaperWidgetController* component = nullptr;
  int container_id = GetWallpaperContainerId(locked_);

  switch (wallpaper_mode_) {
    case WALLPAPER_IMAGE: {
      component = new WallpaperWidgetController(
          CreateWallpaper(root_window, container_id));
      break;
    }
    case WALLPAPER_NONE:
      NOTREACHED();
      return;
  }

  bool is_wallpaper_blurred = false;
  auto* session_controller = Shell::Get()->session_controller();
  if ((session_controller->IsUserSessionBlocked() ||
       session_controller->IsUnlocking()) &&
      IsBlurEnabled()) {
    component->SetWallpaperBlur(login_constants::kBlurSigma);
    is_wallpaper_blurred = true;
  }

  if (is_wallpaper_blurred_ != is_wallpaper_blurred) {
    is_wallpaper_blurred_ = is_wallpaper_blurred;
    // TODO(crbug.com/776464): Replace the observer with mojo calls so that it
    // works under mash and it's easier to add tests.
    for (auto& observer : observers_)
      observer.OnWallpaperBlurChanged();
  }

  RootWindowController* controller =
      RootWindowController::ForWindow(root_window);
  controller->SetAnimatingWallpaperWidgetController(
      new AnimatingWallpaperWidgetController(component));
  component->StartAnimating(controller);
}

void WallpaperController::InstallDesktopControllerForAllWindows() {
  for (aura::Window* root : Shell::GetAllRootWindows())
    InstallDesktopController(root);
  current_max_display_size_ = GetMaxDisplaySizeInNative();
}

bool WallpaperController::ReparentWallpaper(int container) {
  bool moved = false;
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    // In the steady state (no animation playing) the wallpaper widget
    // controller exists in the RootWindowController.
    WallpaperWidgetController* wallpaper_widget_controller =
        root_window_controller->wallpaper_widget_controller();
    if (wallpaper_widget_controller) {
      moved |= wallpaper_widget_controller->Reparent(
          root_window_controller->GetRootWindow(), container);
    }
    // During wallpaper show animations the controller lives in
    // AnimatingWallpaperWidgetController owned by RootWindowController.
    // NOTE: If an image load happens during a wallpaper show animation there
    // can temporarily be two wallpaper widgets. We must reparent both of them,
    // one above and one here.
    WallpaperWidgetController* animating_controller =
        root_window_controller->animating_wallpaper_widget_controller()
            ? root_window_controller->animating_wallpaper_widget_controller()
                  ->GetController(false)
            : nullptr;
    if (animating_controller) {
      moved |= animating_controller->Reparent(
          root_window_controller->GetRootWindow(), container);
    }
  }
  return moved;
}

int WallpaperController::GetWallpaperContainerId(bool locked) {
  return locked ? kShellWindowId_LockScreenWallpaperContainer
                : kShellWindowId_WallpaperContainer;
}

void WallpaperController::UpdateWallpaper(bool clear_cache) {
  current_wallpaper_.reset();
  Shell::Get()->wallpaper_delegate()->UpdateWallpaper(clear_cache);
}

void WallpaperController::RemoveUserWallpaperInfo(const AccountId& account_id,
                                                  bool is_persistent) {
  if (wallpaper_cache_map_.find(account_id) != wallpaper_cache_map_.end())
    wallpaper_cache_map_.erase(account_id);

  PrefService* local_state = Shell::Get()->GetLocalStatePrefService();
  // Local state can be null in tests.
  if (!local_state)
    return;
  WallpaperInfo info;
  GetUserWallpaperInfo(account_id, &info, is_persistent);
  DictionaryPrefUpdate prefs_wallpapers_info_update(local_state,
                                                    prefs::kUserWallpaperInfo);
  prefs_wallpapers_info_update->RemoveWithoutPathExpansion(
      account_id.GetUserEmail(), nullptr);
  // Remove the color cache of the previous wallpaper if it exists.
  DictionaryPrefUpdate wallpaper_colors_update(local_state,
                                               prefs::kWallpaperColors);
  wallpaper_colors_update->RemoveWithoutPathExpansion(info.location, nullptr);
}

void WallpaperController::RemoveUserWallpaperImpl(
    const AccountId& account_id,
    const std::string& wallpaper_files_id) {
  if (wallpaper_files_id.empty())
    return;

  std::vector<base::FilePath> file_to_remove;

  // Remove small user wallpapers.
  base::FilePath wallpaper_path = GetCustomWallpaperDir(kSmallWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id));

  // Remove large user wallpapers.
  wallpaper_path = GetCustomWallpaperDir(kLargeWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id));

  // Remove user wallpaper thumbnails.
  wallpaper_path = GetCustomWallpaperDir(kThumbnailWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id));

  // Remove original user wallpapers.
  wallpaper_path = GetCustomWallpaperDir(kOriginalWallpaperSubDir);
  file_to_remove.push_back(wallpaper_path.Append(wallpaper_files_id));

  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::BACKGROUND,
                            base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                           base::Bind(&DeleteWallpaperInList, file_to_remove));
}

void WallpaperController::OnDefaultWallpaperDecoded(
    const base::FilePath& path,
    wallpaper::WallpaperLayout layout,
    bool show_wallpaper,
    MovableOnDestroyCallbackHolder on_finish,
    std::unique_ptr<user_manager::UserImage> user_image) {
  wallpaper_file_path_for_testing_ = path;
  if (user_image->image().isNull()) {
    // Create a solid color wallpaper if the default wallpaper decoding fails.
    default_wallpaper_image_.reset(
        new user_manager::UserImage(CreateSolidColorWallpaper()));
  } else {
    default_wallpaper_image_ = std::move(user_image);
    // Make sure the file path is updated.
    // TODO(crbug.com/776464): Use |ImageSkia| and |FilePath| directly to cache
    // the decoded image and file path. Nothing else in |UserImage| is relevant.
    default_wallpaper_image_->set_file_path(path);
  }

  if (show_wallpaper) {
    // 1x1 wallpaper is actually solid color, so it should be stretched.
    if (default_wallpaper_image_->image().width() == 1 &&
        default_wallpaper_image_->image().height() == 1) {
      layout = wallpaper::WALLPAPER_LAYOUT_STRETCH;
    }
    WallpaperInfo info(default_wallpaper_image_->file_path().value(), layout,
                       wallpaper::DEFAULT, base::Time::Now().LocalMidnight());
    SetWallpaperImage(default_wallpaper_image_->image(), info);
  }
}

void WallpaperController::SetProminentColors(
    const std::vector<SkColor>& colors) {
  if (prominent_colors_ == colors)
    return;

  prominent_colors_ = colors;
  for (auto& observer : observers_)
    observer.OnWallpaperColorsChanged();
  mojo_observers_.ForAllPtrs([this](mojom::WallpaperObserver* observer) {
    observer->OnWallpaperColorsChanged(prominent_colors_);
  });
}

void WallpaperController::CalculateWallpaperColors() {
  if (color_calculator_) {
    color_calculator_->RemoveObserver(this);
    color_calculator_.reset();
  }

  if (!current_location_.empty()) {
    base::Optional<std::vector<SkColor>> cached_colors =
        GetCachedColors(current_location_);
    if (cached_colors.has_value()) {
      SetProminentColors(cached_colors.value());
      return;
    }
  }

  // Color calculation is only allowed during an active session for performance
  // reasons. Observers outside an active session are notified of the cache, or
  // an invalid color if a previous calculation during active session failed.
  if (!ShouldCalculateColors()) {
    SetProminentColors(
        std::vector<SkColor>(color_profiles_.size(), kInvalidColor));
    return;
  }

  color_calculator_ = std::make_unique<wallpaper::WallpaperColorCalculator>(
      GetWallpaper(), color_profiles_, sequenced_task_runner_);
  color_calculator_->AddObserver(this);
  if (!color_calculator_->StartCalculation()) {
    SetProminentColors(
        std::vector<SkColor>(color_profiles_.size(), kInvalidColor));
  }
}

bool WallpaperController::ShouldCalculateColors() const {
  if (color_profiles_.empty())
    return false;

  gfx::ImageSkia image = GetWallpaper();
  return IsShelfColoringEnabled() &&
         Shell::Get()->session_controller()->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !image.isNull();
}

bool WallpaperController::MoveToLockedContainer() {
  if (locked_)
    return false;

  locked_ = true;
  return ReparentWallpaper(GetWallpaperContainerId(true));
}

bool WallpaperController::MoveToUnlockedContainer() {
  if (!locked_)
    return false;

  locked_ = false;
  return ReparentWallpaper(GetWallpaperContainerId(false));
}

bool WallpaperController::IsDevicePolicyWallpaper() const {
  if (current_wallpaper_)
    return current_wallpaper_->wallpaper_info().type ==
           wallpaper::WallpaperType::DEVICE;
  return false;
}

bool WallpaperController::ShouldSetDevicePolicyWallpaper() const {
  // Only allow the device wallpaper if the policy is in effect for enterprise
  // managed devices.
  if (!is_device_wallpaper_policy_enforced_)
    return false;

  // Only set the device wallpaper if we're at the login screen.
  if (Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return false;

  return true;
}

void WallpaperController::SetDevicePolicyWallpaper() {
  DCHECK(ShouldSetDevicePolicyWallpaper());
  ReadAndDecodeWallpaper(
      base::BindRepeating(&WallpaperController::OnDevicePolicyWallpaperDecoded,
                          weak_factory_.GetWeakPtr()),
      sequenced_task_runner_.get(), GetDevicePolicyWallpaperFilePath());
}

void WallpaperController::OnDevicePolicyWallpaperDecoded(
    std::unique_ptr<user_manager::UserImage> device_wallpaper_image) {
  // It might be possible that the device policy controlled wallpaper finishes
  // decoding after the user logs in. In this case do nothing.
  if (!ShouldSetDevicePolicyWallpaper())
    return;

  if (device_wallpaper_image->image().isNull()) {
    // If device policy wallpaper failed decoding, fall back to the default
    // wallpaper.
    SetDefaultWallpaperImpl(EmptyAccountId(), user_manager::USER_TYPE_REGULAR,
                            true /*show_wallpaper=*/,
                            MovableOnDestroyCallbackHolder());
  } else {
    WallpaperInfo info(GetDevicePolicyWallpaperFilePath().value(),
                       wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED,
                       wallpaper::DEVICE, base::Time::Now().LocalMidnight());
    SetWallpaperImage(device_wallpaper_image->image(), info);
  }
}

void WallpaperController::GetInternalDisplayCompositorLock() {
  if (display::Display::HasInternalDisplay()) {
    aura::Window* root_window =
        Shell::GetRootWindowForDisplayId(display::Display::InternalDisplayId());
    if (root_window) {
      compositor_lock_ =
          root_window->layer()->GetCompositor()->GetCompositorLock(
              this,
              base::TimeDelta::FromMilliseconds(kCompositorLockTimeoutMs));
    }
  }
}

void WallpaperController::CompositorLockTimedOut() {
  compositor_lock_.reset();
}

}  // namespace ash
