// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_theme_provider.h"

#include "app/gfx/codec/png_codec.h"
#include "app/gfx/skbitmap_operations.h"
#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_theme_pack.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/theme_resources_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

// No optimizations under windows until we know what's up with the crashing.
#if defined(OS_WIN)
#pragma optimize("", off)
#pragma warning(disable:4748)
#endif

// Strings used in alignment properties.
const char* BrowserThemeProvider::kAlignmentTop = "top";
const char* BrowserThemeProvider::kAlignmentBottom = "bottom";
const char* BrowserThemeProvider::kAlignmentLeft = "left";
const char* BrowserThemeProvider::kAlignmentRight = "right";

// Strings used in background tiling repetition properties.
const char* BrowserThemeProvider::kTilingNoRepeat = "no-repeat";
const char* BrowserThemeProvider::kTilingRepeatX = "repeat-x";
const char* BrowserThemeProvider::kTilingRepeatY = "repeat-y";
const char* BrowserThemeProvider::kTilingRepeat = "repeat";

// Saved default values.
const char* BrowserThemeProvider::kDefaultThemeID = "";

namespace {

SkColor TintForUnderline(SkColor input) {
  return SkColorSetA(input, SkColorGetA(input) / 3);
}

// Default colors.
const SkColor kDefaultColorFrame = SkColorSetRGB(77, 139, 217);
const SkColor kDefaultColorFrameInactive = SkColorSetRGB(152, 188, 233);
const SkColor kDefaultColorFrameIncognito = SkColorSetRGB(83, 106, 139);
const SkColor kDefaultColorFrameIncognitoInactive =
    SkColorSetRGB(126, 139, 156);
const SkColor kDefaultColorToolbar = SkColorSetRGB(210, 225, 246);
const SkColor kDefaultColorTabText = SK_ColorBLACK;
const SkColor kDefaultColorBackgroundTabText = SkColorSetRGB(64, 64, 64);
const SkColor kDefaultColorBookmarkText = SkColorSetRGB(18, 50, 114);
#if defined(OS_WIN)
const SkColor kDefaultColorNTPBackground =
    color_utils::GetSysSkColor(COLOR_WINDOW);
const SkColor kDefaultColorNTPText =
    color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
const SkColor kDefaultColorNTPLink =
    color_utils::GetSysSkColor(COLOR_HOTLIGHT);
#else
// TODO(beng): source from theme provider.
const SkColor kDefaultColorNTPBackground = SK_ColorWHITE;
const SkColor kDefaultColorNTPText = SK_ColorBLACK;
const SkColor kDefaultColorNTPLink = SkColorSetRGB(6, 55, 116);
#endif
const SkColor kDefaultColorNTPHeader = SkColorSetRGB(75, 140, 220);
const SkColor kDefaultColorNTPSection = SkColorSetRGB(229, 239, 254);
const SkColor kDefaultColorNTPSectionText = SK_ColorBLACK;
const SkColor kDefaultColorNTPSectionLink = SkColorSetRGB(6, 55, 116);
const SkColor kDefaultColorControlBackground = SkColorSetARGB(0, 0, 0, 0);
const SkColor kDefaultColorButtonBackground = SkColorSetARGB(0, 0, 0, 0);

// Default tints.
const color_utils::HSL kDefaultTintButtons = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrame = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrameInactive = { -1, -1, 0.75f };
const color_utils::HSL kDefaultTintFrameIncognito = { -1, 0.2f, 0.35f };
const color_utils::HSL kDefaultTintFrameIncognitoInactive = { -1, 0.3f, 0.6f };
const color_utils::HSL kDefaultTintBackgroundTab = { -1, 0.5, 0.75 };

// Default display properties.
const int kDefaultDisplayPropertyNTPAlignment =
    BrowserThemeProvider::ALIGN_BOTTOM;
const int kDefaultDisplayPropertyNTPTiling =
    BrowserThemeProvider::NO_REPEAT;
const int kDefaultDisplayPropertyNTPInverseLogo = 0;

// The sum of kFrameBorderThickness and kNonClientRestoredExtraThickness from
// OpaqueBrowserFrameView.
const int kRestoredTabVerticalOffset = 15;

// The image resources we will allow people to theme.
const int kThemeableImages[] = {
  IDR_THEME_FRAME,
  IDR_THEME_FRAME_INACTIVE,
  IDR_THEME_FRAME_INCOGNITO,
  IDR_THEME_FRAME_INCOGNITO_INACTIVE,
  IDR_THEME_TOOLBAR,
  IDR_THEME_TAB_BACKGROUND,
  IDR_THEME_TAB_BACKGROUND_INCOGNITO,
  IDR_THEME_TAB_BACKGROUND_V,
  IDR_THEME_NTP_BACKGROUND,
  IDR_THEME_FRAME_OVERLAY,
  IDR_THEME_FRAME_OVERLAY_INACTIVE,
  IDR_THEME_BUTTON_BACKGROUND,
  IDR_THEME_NTP_ATTRIBUTION,
  IDR_THEME_WINDOW_CONTROL_BACKGROUND
};

bool HasThemeableImage(int themeable_image_id) {
  static std::set<int> themeable_images;
  if (themeable_images.empty()) {
    themeable_images.insert(
        kThemeableImages, kThemeableImages + arraysize(kThemeableImages));
  }
  return themeable_images.count(themeable_image_id) > 0;
}

// The image resources that will be tinted by the 'button' tint value.
const int kToolbarButtonIDs[] = {
  IDR_BACK, IDR_BACK_D, IDR_BACK_H, IDR_BACK_P,
  IDR_FORWARD, IDR_FORWARD_D, IDR_FORWARD_H, IDR_FORWARD_P,
  IDR_RELOAD, IDR_RELOAD_H, IDR_RELOAD_P,
  IDR_HOME, IDR_HOME_H, IDR_HOME_P,
  IDR_STAR, IDR_STAR_NOBORDER, IDR_STAR_NOBORDER_CENTER, IDR_STAR_D, IDR_STAR_H,
  IDR_STAR_P,
  IDR_STARRED, IDR_STARRED_NOBORDER, IDR_STARRED_NOBORDER_CENTER, IDR_STARRED_H,
  IDR_STARRED_P,
  IDR_GO, IDR_GO_NOBORDER, IDR_GO_NOBORDER_CENTER, IDR_GO_H, IDR_GO_P,
  IDR_STOP, IDR_STOP_NOBORDER, IDR_STOP_NOBORDER_CENTER, IDR_STOP_H, IDR_STOP_P,
  IDR_MENU_BOOKMARK,
  IDR_MENU_PAGE, IDR_MENU_PAGE_RTL,
  IDR_MENU_CHROME, IDR_MENU_CHROME_RTL,
  IDR_MENU_DROPARROW,
  IDR_THROBBER, IDR_THROBBER_WAITING, IDR_THROBBER_LIGHT,
  IDR_LOCATIONBG
};

// Writes the theme pack to disk on a separate thread.
class WritePackToDiskTask : public Task {
 public:
  WritePackToDiskTask(BrowserThemePack* pack, const FilePath& path)
      : theme_pack_(pack), pack_path_(path) {}

  virtual void Run() {
    if (!theme_pack_->WriteToDisk(pack_path_)) {
      NOTREACHED() << "Could not write theme pack to disk";
    }
  }

 private:
  scoped_refptr<BrowserThemePack> theme_pack_;
  FilePath pack_path_;
};

}  // namespace

bool BrowserThemeProvider::IsThemeableImage(int resource_id) {
  return HasThemeableImage(resource_id);
}

BrowserThemeProvider::BrowserThemeProvider()
    : rb_(ResourceBundle::GetSharedInstance()),
      profile_(NULL),
      number_of_infobars_(0) {
  // Initialize the themeable image map so we can use it on other threads.
  HasThemeableImage(0);
}

BrowserThemeProvider::~BrowserThemeProvider() {
  FreePlatformCaches();

  RemoveUnusedThemes();
}

void BrowserThemeProvider::Init(Profile* profile) {
  DCHECK(CalledOnValidThread());
  profile_ = profile;

  LoadThemePrefs();
}

SkBitmap* BrowserThemeProvider::GetBitmapNamed(int id) const {
  DCHECK(CalledOnValidThread());

  SkBitmap* bitmap = NULL;

  if (theme_pack_.get())
    bitmap = theme_pack_->GetBitmapNamed(id);

  if (!bitmap)
    bitmap = rb_.GetBitmapNamed(id);

  return bitmap;
}

SkColor BrowserThemeProvider::GetColor(int id) const {
  DCHECK(CalledOnValidThread());

  SkColor color;
  if (theme_pack_.get() && theme_pack_->GetColor(id, &color))
    return color;

  return GetDefaultColor(id);
}

bool BrowserThemeProvider::GetDisplayProperty(int id, int* result) const {
  if (theme_pack_.get())
    return theme_pack_->GetDisplayProperty(id, result);

  return GetDefaultDisplayProperty(id, result);
}

bool BrowserThemeProvider::ShouldUseNativeFrame() const {
  if (HasCustomImage(IDR_THEME_FRAME))
    return false;
#if defined(OS_WIN)
  return win_util::ShouldUseVistaFrame();
#else
  return false;
#endif
}

bool BrowserThemeProvider::HasCustomImage(int id) const {
  if (!HasThemeableImage(id))
    return false;

  if (theme_pack_)
    return theme_pack_->HasCustomImage(id);

  return false;
}

RefCountedMemory* BrowserThemeProvider::GetRawData(int id) const {
  // Check to see whether we should substitute some images.
  int ntp_alternate;
  GetDisplayProperty(NTP_LOGO_ALTERNATE, &ntp_alternate);
  if (id == IDR_PRODUCT_LOGO && ntp_alternate != 0)
    id = IDR_PRODUCT_LOGO_WHITE;

  RefCountedMemory* data = NULL;
  if (theme_pack_.get())
    data = theme_pack_->GetRawData(id);
  if (!data)
    data = rb_.LoadDataResourceBytes(id);

  return data;
}

void BrowserThemeProvider::SetTheme(Extension* extension) {
  // Clear our image cache.
  FreePlatformCaches();

  DCHECK(extension);
  DCHECK(extension->IsTheme());

  BuildFromExtension(extension, false);
  SaveThemeID(extension->id());

  NotifyThemeChanged();
  UserMetrics::RecordAction("Themes_Installed", profile_);
}

void BrowserThemeProvider::RemoveUnusedThemes() {
  if (!profile_)
    return;
  ExtensionsService* service = profile_->GetExtensionsService();
  if (!service)
    return;
  std::string current_theme = GetThemeID();
  std::vector<std::string> remove_list;
  const ExtensionList* extensions = service->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    if ((*it)->IsTheme() && (*it)->id() != current_theme) {
      remove_list.push_back((*it)->id());
    }
  }
  for (size_t i = 0; i < remove_list.size(); ++i)
    service->UninstallExtension(remove_list[i], false);
}

void BrowserThemeProvider::UseDefaultTheme() {
  ClearAllThemeData();
  NotifyThemeChanged();
  UserMetrics::RecordAction("Themes_Reset", profile_);
}

std::string BrowserThemeProvider::GetThemeID() const {
  std::wstring id = profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);
  return WideToUTF8(id);
}

// static
std::string BrowserThemeProvider::AlignmentToString(int alignment) {
  // Convert from an AlignmentProperty back into a string.
  std::string vertical_string;
  std::string horizontal_string;

  if (alignment & BrowserThemeProvider::ALIGN_TOP)
    vertical_string = kAlignmentTop;
  else if (alignment & BrowserThemeProvider::ALIGN_BOTTOM)
    vertical_string = kAlignmentBottom;

  if (alignment & BrowserThemeProvider::ALIGN_LEFT)
    horizontal_string = kAlignmentLeft;
  else if (alignment & BrowserThemeProvider::ALIGN_RIGHT)
    horizontal_string = kAlignmentRight;

  if (vertical_string.empty())
    return horizontal_string;
  if (horizontal_string.empty())
    return vertical_string;
  return vertical_string + " " + horizontal_string;
}

// static
int BrowserThemeProvider::StringToAlignment(const std::string& alignment) {
  std::vector<std::wstring> split;
  SplitStringAlongWhitespace(UTF8ToWide(alignment), &split);

  int alignment_mask = 0;
  for (std::vector<std::wstring>::iterator alignments(split.begin());
       alignments != split.end(); ++alignments) {
    std::string comp = WideToUTF8(*alignments);
    const char* component = comp.c_str();

    if (base::strcasecmp(component, kAlignmentTop) == 0)
      alignment_mask |= BrowserThemeProvider::ALIGN_TOP;
    else if (base::strcasecmp(component, kAlignmentBottom) == 0)
      alignment_mask |= BrowserThemeProvider::ALIGN_BOTTOM;

    if (base::strcasecmp(component, kAlignmentLeft) == 0)
      alignment_mask |= BrowserThemeProvider::ALIGN_LEFT;
    else if (base::strcasecmp(component, kAlignmentRight) == 0)
      alignment_mask |= BrowserThemeProvider::ALIGN_RIGHT;
  }
  return alignment_mask;
}

// static
std::string BrowserThemeProvider::TilingToString(int tiling) {
  // Convert from a TilingProperty back into a string.
  if (tiling == BrowserThemeProvider::REPEAT_X)
    return kTilingRepeatX;
  if (tiling == BrowserThemeProvider::REPEAT_Y)
    return kTilingRepeatY;
  if (tiling == BrowserThemeProvider::REPEAT)
    return kTilingRepeat;
  return kTilingNoRepeat;
}

// static
int BrowserThemeProvider::StringToTiling(const std::string& tiling) {
  const char* component = tiling.c_str();

  if (base::strcasecmp(component, kTilingRepeatX) == 0)
    return BrowserThemeProvider::REPEAT_X;
  if (base::strcasecmp(component, kTilingRepeatY) == 0)
    return BrowserThemeProvider::REPEAT_Y;
  if (base::strcasecmp(component, kTilingRepeat) == 0)
    return BrowserThemeProvider::REPEAT;
  // NO_REPEAT is the default choice.
  return BrowserThemeProvider::NO_REPEAT;
}

// static
color_utils::HSL BrowserThemeProvider::GetDefaultTint(int id) {
  switch (id) {
    case TINT_FRAME:
      return kDefaultTintFrame;
    case TINT_FRAME_INACTIVE:
      return kDefaultTintFrameInactive;
    case TINT_FRAME_INCOGNITO:
      return kDefaultTintFrameIncognito;
    case TINT_FRAME_INCOGNITO_INACTIVE:
      return kDefaultTintFrameIncognitoInactive;
    case TINT_BUTTONS:
      return kDefaultTintButtons;
    case TINT_BACKGROUND_TAB:
      return kDefaultTintBackgroundTab;
    default:
      color_utils::HSL result = {-1, -1, -1};
      return result;
  }
}

// static
SkColor BrowserThemeProvider::GetDefaultColor(int id) {
  switch (id) {
    case COLOR_FRAME:
      return kDefaultColorFrame;
    case COLOR_FRAME_INACTIVE:
      return kDefaultColorFrameInactive;
    case COLOR_FRAME_INCOGNITO:
      return kDefaultColorFrameIncognito;
    case COLOR_FRAME_INCOGNITO_INACTIVE:
      return kDefaultColorFrameIncognitoInactive;
    case COLOR_TOOLBAR:
      return kDefaultColorToolbar;
    case COLOR_TAB_TEXT:
      return kDefaultColorTabText;
    case COLOR_BACKGROUND_TAB_TEXT:
      return kDefaultColorBackgroundTabText;
    case COLOR_BOOKMARK_TEXT:
      return kDefaultColorBookmarkText;
    case COLOR_NTP_BACKGROUND:
      return kDefaultColorNTPBackground;
    case COLOR_NTP_TEXT:
      return kDefaultColorNTPText;
    case COLOR_NTP_LINK:
      return kDefaultColorNTPLink;
    case COLOR_NTP_LINK_UNDERLINE:
      return TintForUnderline(kDefaultColorNTPLink);
    case COLOR_NTP_HEADER:
      return kDefaultColorNTPHeader;
    case COLOR_NTP_SECTION:
      return kDefaultColorNTPSection;
    case COLOR_NTP_SECTION_TEXT:
      return kDefaultColorNTPSectionText;
    case COLOR_NTP_SECTION_LINK:
      return kDefaultColorNTPSectionLink;
    case COLOR_NTP_SECTION_LINK_UNDERLINE:
      return TintForUnderline(kDefaultColorNTPSectionLink);
    case COLOR_CONTROL_BACKGROUND:
      return kDefaultColorControlBackground;
    case COLOR_BUTTON_BACKGROUND:
      return kDefaultColorButtonBackground;
    default:
      // Return a debugging red color.
      return 0xffff0000;
  }
}

// static
bool BrowserThemeProvider::GetDefaultDisplayProperty(int id, int* result) {
  switch (id) {
    case NTP_BACKGROUND_ALIGNMENT:
      *result = kDefaultDisplayPropertyNTPAlignment;
      return true;
    case NTP_BACKGROUND_TILING:
      *result = kDefaultDisplayPropertyNTPTiling;
      return true;
    case NTP_LOGO_ALTERNATE:
      *result = kDefaultDisplayPropertyNTPInverseLogo;
      return true;
  }

  return false;
}

// static
const std::set<int>& BrowserThemeProvider::GetTintableToolbarButtons() {
  static std::set<int> button_set;
  if (button_set.empty()) {
    button_set = std::set<int>(
        kToolbarButtonIDs,
        kToolbarButtonIDs + arraysize(kToolbarButtonIDs));
  }

  return button_set;
}

color_utils::HSL BrowserThemeProvider::GetTint(int id) const {
  DCHECK(CalledOnValidThread());

  color_utils::HSL hsl;
  if (theme_pack_.get() && theme_pack_->GetTint(id, &hsl))
    return hsl;

  return GetDefaultTint(id);
}

void BrowserThemeProvider::ClearAllThemeData() {
  // Clear our image cache.
  FreePlatformCaches();
  theme_pack_ = NULL;

  profile_->GetPrefs()->ClearPref(prefs::kCurrentThemePackFilename);
  SaveThemeID(kDefaultThemeID);
}

void BrowserThemeProvider::LoadThemePrefs() {
  PrefService* prefs = profile_->GetPrefs();

  std::string current_id = GetThemeID();
  if (current_id != kDefaultThemeID) {
    bool loaded_pack = false;

    // If we don't have a file pack, we're updating from an old version.
    FilePath path = prefs->GetFilePath(prefs::kCurrentThemePackFilename);
    if (path != FilePath()) {
      theme_pack_ = BrowserThemePack::BuildFromDataPack(path, current_id);
      loaded_pack = theme_pack_.get() != NULL;
    }

    if (loaded_pack) {
      UserMetrics::RecordAction("Themes.Loaded", profile_);
    } else {
      // TODO(erg): We need to pop up a dialog informing the user that their
      // theme is being migrated.
      ExtensionsService* service = profile_->GetExtensionsService();
      if (service) {
        Extension* extension = service->GetExtensionById(current_id, false);
        if (extension) {
          MigrateTheme(extension, extension->name());
        } else {
          DLOG(ERROR) << "Theme is mysteriously gone.";
          ClearAllThemeData();
          UserMetrics::RecordAction("Themes.Gone", profile_);
        }
      }
    }
  }
}

void BrowserThemeProvider::NotifyThemeChanged() {
  // Redraw!
  NotificationService* service = NotificationService::current();
  service->Notify(NotificationType::BROWSER_THEME_CHANGED,
                  Source<BrowserThemeProvider>(this),
                  NotificationService::NoDetails());
}

#if defined(OS_WIN)
void BrowserThemeProvider::FreePlatformCaches() {
  // Views (Skia) has no platform image cache to clear.
}
#endif

void BrowserThemeProvider::SavePackName(const FilePath& pack_path) {
  profile_->GetPrefs()->SetFilePath(
      prefs::kCurrentThemePackFilename, pack_path);
}

void BrowserThemeProvider::SaveThemeID(const std::string& id) {
  profile_->GetPrefs()->SetString(prefs::kCurrentThemeID, UTF8ToWide(id));
}

void BrowserThemeProvider::MigrateTheme(Extension* extension,
                                        const std::string& name) {
  FilePath::CharType full_name_on_stack[512 + 1];

  // Copy names's backing string onto the stack because that's what get's
  // stored in minidumps. :(
  size_t i = 0;
  for (i = 0; i < 512 && i < name.size(); ++i) {
    full_name_on_stack[i] = name[i];
  }
  full_name_on_stack[i] = '\0';

  // TODO(erg): Remove this hack.
  //
  // This is a hack to force the name of the theme into the stack
  // frame. Hopefully.
  BuildFromExtension(extension, true);
  UserMetrics::RecordAction("Themes.Migrated", profile_);
  LOG(INFO) << "Migrating theme: " << full_name_on_stack;
}

void BrowserThemeProvider::BuildFromExtension(Extension* extension,
                                              bool synchronously) {
  CHECK(extension);

  scoped_refptr<BrowserThemePack> pack =
      BrowserThemePack::BuildFromExtension(extension);
  if (!pack.get()) {
    NOTREACHED() << "Could not load theme.";
    return;
  }

  // Write the packed file to disk.
  FilePath pack_path = extension->path().Append(chrome::kThemePackFilename);
  if (synchronously) {
    // Write to disk sychronously because we don't have a message loop yet.
    if (!pack->WriteToDisk(pack_path)) {
      NOTREACHED() << "Could not write theme pack to disk";
    }
  } else {
    // Post a task to write to disk since we have a message loop to post from.
    ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
                           new WritePackToDiskTask(pack, pack_path));
  }

  SavePackName(pack_path);
  theme_pack_ = pack;
}

void BrowserThemeProvider::OnInfobarDisplayed() {
  number_of_infobars_++;
}

void BrowserThemeProvider::OnInfobarDestroyed() {
  number_of_infobars_--;

  if (number_of_infobars_ == 0)
    RemoveUnusedThemes();
}

// No optimizations under windows until we know what's up with the crashing.
#if defined(OS_WIN)
#pragma warning(default:4748)
#pragma optimize("", on)
#endif
