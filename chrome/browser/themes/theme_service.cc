// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/ui_resources.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "ui/views/widget/native_widget_win.h"
#endif

using content::BrowserThread;
using content::UserMetricsAction;
using ui::ResourceBundle;

// Strings used in alignment properties.
const char* ThemeService::kAlignmentCenter = "center";
const char* ThemeService::kAlignmentTop = "top";
const char* ThemeService::kAlignmentBottom = "bottom";
const char* ThemeService::kAlignmentLeft = "left";
const char* ThemeService::kAlignmentRight = "right";

// Strings used in background tiling repetition properties.
const char* ThemeService::kTilingNoRepeat = "no-repeat";
const char* ThemeService::kTilingRepeatX = "repeat-x";
const char* ThemeService::kTilingRepeatY = "repeat-y";
const char* ThemeService::kTilingRepeat = "repeat";

// The default theme if we haven't installed a theme yet or if we've clicked
// the "Use Classic" button.
const char* ThemeService::kDefaultThemeID = "";

namespace {

// The default theme if we've gone to the theme gallery and installed the
// "Default" theme. We have to detect this case specifically. (By the time we
// realize we've installed the default theme, we already have an extension
// unpacked on the filesystem.)
const char* kDefaultThemeGalleryID = "hkacjpbfdknhflllbcmjibkdeoafencn";

SkColor TintForUnderline(SkColor input) {
  return SkColorSetA(input, SkColorGetA(input) / 3);
}

SkColor IncreaseLightness(SkColor color, double percent) {
  color_utils::HSL result;
  color_utils::SkColorToHSL(color, &result);
  result.l += (1 - result.l) * percent;
  return color_utils::HSLToSkColor(result, SkColorGetA(color));
}

// Default colors.
#if defined(USE_AURA)
// TODO(jamescook): Revert this when Aura is using its own window frame
// implementation by default, specifically BrowserNonClientFrameViewAsh.
const SkColor kDefaultColorFrame = SkColorSetRGB(109, 109, 109);
const SkColor kDefaultColorFrameInactive = SkColorSetRGB(176, 176, 176);
#else
const SkColor kDefaultColorFrame = SkColorSetRGB(66, 116, 201);
const SkColor kDefaultColorFrameInactive = SkColorSetRGB(161, 182, 228);
#endif  // USE_AURA
const SkColor kDefaultColorFrameIncognito = SkColorSetRGB(83, 106, 139);
const SkColor kDefaultColorFrameIncognitoInactive =
    SkColorSetRGB(126, 139, 156);
#if defined(OS_MACOSX)
const SkColor kDefaultColorToolbar = SkColorSetRGB(230, 230, 230);
#else
const SkColor kDefaultColorToolbar = SkColorSetRGB(223, 223, 223);
#endif
#if defined(USE_AURA)
const SkColor kDefaultColorToolbarSeparator = SkColorSetRGB(128, 128, 128);
#else
const SkColor kDefaultColorToolbarSeparator = SkColorSetRGB(182, 186, 192);
#endif
const SkColor kDefaultColorTabText = SK_ColorBLACK;
#if defined(OS_MACOSX)
const SkColor kDefaultColorBackgroundTabText = SK_ColorBLACK;
#else
const SkColor kDefaultColorBackgroundTabText = SkColorSetRGB(64, 64, 64);
#endif
const SkColor kDefaultColorBookmarkText = SK_ColorBLACK;
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
const SkColor kDefaultColorNTPHeader = SkColorSetRGB(150, 150, 150);
const SkColor kDefaultColorNTPSection = SkColorSetRGB(229, 229, 229);
const SkColor kDefaultColorNTPSectionText = SK_ColorBLACK;
const SkColor kDefaultColorNTPSectionLink = SkColorSetRGB(6, 55, 116);
const SkColor kDefaultColorControlBackground = SkColorSetARGB(0, 0, 0, 0);
const SkColor kDefaultColorButtonBackground = SkColorSetARGB(0, 0, 0, 0);
#if defined(OS_MACOSX)
const SkColor kDefaultColorToolbarButtonStroke = SkColorSetARGB(75, 81, 81, 81);
const SkColor kDefaultColorToolbarButtonStrokeInactive =
    SkColorSetARGB(75, 99, 99, 99);
const SkColor kDefaultColorToolbarBezel = SkColorSetRGB(247, 247, 247);
const SkColor kDefaultColorToolbarStroke = SkColorSetRGB(103, 103, 103);
const SkColor kDefaultColorToolbarStrokeInactive = SkColorSetRGB(123, 123, 123);
#endif

// Default tints.
const color_utils::HSL kDefaultTintButtons = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrame = { -1, -1, -1 };
const color_utils::HSL kDefaultTintFrameInactive = { -1, -1, 0.75f };
const color_utils::HSL kDefaultTintFrameIncognito = { -1, 0.2f, 0.35f };
const color_utils::HSL kDefaultTintFrameIncognitoInactive = { -1, 0.3f, 0.6f };
const color_utils::HSL kDefaultTintBackgroundTab = { -1, 0.5, 0.75 };

// Default display properties.
const int kDefaultDisplayPropertyNTPAlignment =
    ThemeService::ALIGN_BOTTOM;
const int kDefaultDisplayPropertyNTPTiling =
    ThemeService::NO_REPEAT;
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
  CR_DEFINE_STATIC_LOCAL(std::set<int>, themeable_images, ());
  if (themeable_images.empty()) {
    themeable_images.insert(
        kThemeableImages, kThemeableImages + arraysize(kThemeableImages));
  }
  return themeable_images.count(themeable_image_id) > 0;
}

// The image resources that will be tinted by the 'button' tint value.
// If you change this list, you must increment the version number in
// browser_theme_pack.cc, and you should assign persistent IDs to the
// data table at the start of said file or else tinted versions of
// these resources will not be created.
const int kToolbarButtonIDs[] = {
  IDR_BACK, IDR_BACK_D, IDR_BACK_H, IDR_BACK_P,
  IDR_FORWARD, IDR_FORWARD_D, IDR_FORWARD_H, IDR_FORWARD_P,
  IDR_HOME, IDR_HOME_H, IDR_HOME_P,
  IDR_RELOAD, IDR_RELOAD_H, IDR_RELOAD_P,
  IDR_STOP, IDR_STOP_D, IDR_STOP_H, IDR_STOP_P,
  IDR_LOCATIONBG_C, IDR_LOCATIONBG_L, IDR_LOCATIONBG_R,
  IDR_BROWSER_ACTIONS_OVERFLOW, IDR_BROWSER_ACTIONS_OVERFLOW_H,
  IDR_BROWSER_ACTIONS_OVERFLOW_P,
  IDR_TOOLS, IDR_TOOLS_H, IDR_TOOLS_P,
  IDR_MENU_DROPARROW,
  IDR_THROBBER, IDR_THROBBER_WAITING, IDR_THROBBER_LIGHT,
};

// Writes the theme pack to disk on a separate thread.
void WritePackToDiskCallback(BrowserThemePack* pack, const FilePath& path) {
  if (!pack->WriteToDisk(path))
    NOTREACHED() << "Could not write theme pack to disk";
}

}  // namespace

bool ThemeService::IsThemeableImage(int resource_id) {
  return HasThemeableImage(resource_id);
}

ThemeService::ThemeService()
    : rb_(ResourceBundle::GetSharedInstance()),
      profile_(NULL),
      number_of_infobars_(0) {
  // Initialize the themeable image map so we can use it on other threads.
  HasThemeableImage(0);
}

ThemeService::~ThemeService() {
  FreePlatformCaches();
}

void ThemeService::Init(Profile* profile) {
  DCHECK(CalledOnValidThread());
  profile_ = profile;

  // Listen to EXTENSION_LOADED instead of EXTENSION_INSTALLED because
  // the extension cannot yet be found via GetExtensionById() if it is
  // installed but not loaded (which may confuse listeners to
  // BROWSER_THEME_CHANGED).
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));

  LoadThemePrefs();
}

const gfx::Image* ThemeService::GetImageNamed(int id) const {
  DCHECK(CalledOnValidThread());

  const gfx::Image* image = NULL;

  if (theme_pack_.get())
    image = theme_pack_->GetImageNamed(id);

  if (!image)
    image = &rb_.GetNativeImageNamed(id);

  return image;
}

SkBitmap* ThemeService::GetBitmapNamed(int id) const {
  const gfx::Image* image = GetImageNamed(id);
  if (!image)
    return NULL;

  return const_cast<SkBitmap*>(image->ToSkBitmap());
}

gfx::ImageSkia* ThemeService::GetImageSkiaNamed(int id) const {
  const gfx::Image* image = GetImageNamed(id);
  if (!image)
    return NULL;
  // TODO(pkotwicz): Remove this const cast.  The gfx::Image interface returns
  // its images const. GetImageSkiaNamed() also should but has many callsites.
  return const_cast<gfx::ImageSkia*>(image->ToImageSkia());
}

SkColor ThemeService::GetColor(int id) const {
  DCHECK(CalledOnValidThread());

  SkColor color;
  if (theme_pack_.get() && theme_pack_->GetColor(id, &color))
    return color;

  // For backward compat with older themes, some newer colors are generated from
  // older ones if they are missing.
  switch (id) {
    case COLOR_NTP_SECTION_HEADER_TEXT:
      return IncreaseLightness(GetColor(COLOR_NTP_TEXT), 0.30);
    case COLOR_NTP_SECTION_HEADER_TEXT_HOVER:
      return GetColor(COLOR_NTP_TEXT);
    case COLOR_NTP_SECTION_HEADER_RULE:
      return IncreaseLightness(GetColor(COLOR_NTP_TEXT), 0.70);
    case COLOR_NTP_SECTION_HEADER_RULE_LIGHT:
      return IncreaseLightness(GetColor(COLOR_NTP_TEXT), 0.86);
    case COLOR_NTP_TEXT_LIGHT:
      return IncreaseLightness(GetColor(COLOR_NTP_TEXT), 0.40);
  }

  return GetDefaultColor(id);
}

bool ThemeService::GetDisplayProperty(int id, int* result) const {
  if (theme_pack_.get())
    return theme_pack_->GetDisplayProperty(id, result);

  return GetDefaultDisplayProperty(id, result);
}

bool ThemeService::ShouldUseNativeFrame() const {
  if (HasCustomImage(IDR_THEME_FRAME))
    return false;
#if defined(OS_WIN) && !defined(USE_AURA)
  return views::NativeWidgetWin::IsAeroGlassEnabled();
#else
  return false;
#endif
}

bool ThemeService::HasCustomImage(int id) const {
  if (!HasThemeableImage(id))
    return false;

  if (theme_pack_)
    return theme_pack_->HasCustomImage(id);

  return false;
}

base::RefCountedMemory* ThemeService::GetRawData(int id) const {
  // Check to see whether we should substitute some images.
  int ntp_alternate;
  GetDisplayProperty(NTP_LOGO_ALTERNATE, &ntp_alternate);
  if (id == IDR_PRODUCT_LOGO && ntp_alternate != 0)
    id = IDR_PRODUCT_LOGO_WHITE;

  base::RefCountedMemory* data = NULL;
  if (theme_pack_.get())
    data = theme_pack_->GetRawData(id);
  if (!data)
    data = rb_.LoadDataResourceBytes(id, ui::SCALE_FACTOR_100P);

  return data;
}

void ThemeService::SetTheme(const Extension* extension) {
  // Clear our image cache.
  FreePlatformCaches();

  DCHECK(extension);
  DCHECK(extension->is_theme());

  BuildFromExtension(extension);
  SaveThemeID(extension->id());

  NotifyThemeChanged();
  content::RecordAction(UserMetricsAction("Themes_Installed"));
}

void ThemeService::RemoveUnusedThemes() {
  if (!profile_)
    return;
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return;
  std::string current_theme = GetThemeID();
  std::vector<std::string> remove_list;
  const ExtensionSet* extensions = service->extensions();
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    if ((*it)->is_theme() && (*it)->id() != current_theme) {
      remove_list.push_back((*it)->id());
    }
  }
  for (size_t i = 0; i < remove_list.size(); ++i)
    service->UninstallExtension(remove_list[i], false, NULL);
}

void ThemeService::UseDefaultTheme() {
  ClearAllThemeData();
  NotifyThemeChanged();
  content::RecordAction(UserMetricsAction("Themes_Reset"));
}

void ThemeService::SetNativeTheme() {
  UseDefaultTheme();
}

bool ThemeService::UsingDefaultTheme() const {
  std::string id = GetThemeID();
  return id == ThemeService::kDefaultThemeID ||
      id == kDefaultThemeGalleryID;
}

bool ThemeService::UsingNativeTheme() const {
  return UsingDefaultTheme();
}

std::string ThemeService::GetThemeID() const {
  return profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);
}

// static
std::string ThemeService::AlignmentToString(int alignment) {
  // Convert from an AlignmentProperty back into a string.
  std::string vertical_string(kAlignmentCenter);
  std::string horizontal_string(kAlignmentCenter);

  if (alignment & ThemeService::ALIGN_TOP)
    vertical_string = kAlignmentTop;
  else if (alignment & ThemeService::ALIGN_BOTTOM)
    vertical_string = kAlignmentBottom;

  if (alignment & ThemeService::ALIGN_LEFT)
    horizontal_string = kAlignmentLeft;
  else if (alignment & ThemeService::ALIGN_RIGHT)
    horizontal_string = kAlignmentRight;

  return horizontal_string + " " + vertical_string;
}

// static
int ThemeService::StringToAlignment(const std::string& alignment) {
  std::vector<std::string> split;
  base::SplitStringAlongWhitespace(alignment, &split);

  int alignment_mask = 0;
  for (std::vector<std::string>::iterator component(split.begin());
       component != split.end(); ++component) {
    if (LowerCaseEqualsASCII(*component, kAlignmentTop))
      alignment_mask |= ThemeService::ALIGN_TOP;
    else if (LowerCaseEqualsASCII(*component, kAlignmentBottom))
      alignment_mask |= ThemeService::ALIGN_BOTTOM;
    else if (LowerCaseEqualsASCII(*component, kAlignmentLeft))
      alignment_mask |= ThemeService::ALIGN_LEFT;
    else if (LowerCaseEqualsASCII(*component, kAlignmentRight))
      alignment_mask |= ThemeService::ALIGN_RIGHT;
  }
  return alignment_mask;
}

// static
std::string ThemeService::TilingToString(int tiling) {
  // Convert from a TilingProperty back into a string.
  if (tiling == ThemeService::REPEAT_X)
    return kTilingRepeatX;
  if (tiling == ThemeService::REPEAT_Y)
    return kTilingRepeatY;
  if (tiling == ThemeService::REPEAT)
    return kTilingRepeat;
  return kTilingNoRepeat;
}

// static
int ThemeService::StringToTiling(const std::string& tiling) {
  const char* component = tiling.c_str();

  if (base::strcasecmp(component, kTilingRepeatX) == 0)
    return ThemeService::REPEAT_X;
  if (base::strcasecmp(component, kTilingRepeatY) == 0)
    return ThemeService::REPEAT_Y;
  if (base::strcasecmp(component, kTilingRepeat) == 0)
    return ThemeService::REPEAT;
  // NO_REPEAT is the default choice.
  return ThemeService::NO_REPEAT;
}

// static
color_utils::HSL ThemeService::GetDefaultTint(int id) {
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
SkColor ThemeService::GetDefaultColor(int id) {
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
    case COLOR_TOOLBAR_SEPARATOR:
      return kDefaultColorToolbarSeparator;
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
#if defined(OS_MACOSX)
    case COLOR_TOOLBAR_BUTTON_STROKE:
      return kDefaultColorToolbarButtonStroke;
    case COLOR_TOOLBAR_BUTTON_STROKE_INACTIVE:
      return kDefaultColorToolbarButtonStrokeInactive;
    case COLOR_TOOLBAR_BEZEL:
      return kDefaultColorToolbarBezel;
    case COLOR_TOOLBAR_STROKE:
      return kDefaultColorToolbarStroke;
    case COLOR_TOOLBAR_STROKE_INACTIVE:
      return kDefaultColorToolbarStrokeInactive;
#endif
    default:
      // Return a debugging red color.
      return 0xffff0000;
  }
}

// static
bool ThemeService::GetDefaultDisplayProperty(int id, int* result) {
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
const std::set<int>& ThemeService::GetTintableToolbarButtons() {
  CR_DEFINE_STATIC_LOCAL(std::set<int>, button_set, ());
  if (button_set.empty()) {
    button_set = std::set<int>(
        kToolbarButtonIDs,
        kToolbarButtonIDs + arraysize(kToolbarButtonIDs));
  }

  return button_set;
}

color_utils::HSL ThemeService::GetTint(int id) const {
  DCHECK(CalledOnValidThread());

  color_utils::HSL hsl;
  if (theme_pack_.get() && theme_pack_->GetTint(id, &hsl))
    return hsl;

  return GetDefaultTint(id);
}

void ThemeService::ClearAllThemeData() {
  // Clear our image cache.
  FreePlatformCaches();
  theme_pack_ = NULL;

  profile_->GetPrefs()->ClearPref(prefs::kCurrentThemePackFilename);
  SaveThemeID(kDefaultThemeID);
}

void ThemeService::LoadThemePrefs() {
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
      content::RecordAction(UserMetricsAction("Themes.Loaded"));
    } else {
      // TODO(erg): We need to pop up a dialog informing the user that their
      // theme is being migrated.
      ExtensionService* service = profile_->GetExtensionService();
      if (service) {
        const Extension* extension =
            service->GetExtensionById(current_id, false);
        if (extension) {
          DLOG(ERROR) << "Migrating theme";
          BuildFromExtension(extension);
          content::RecordAction(UserMetricsAction("Themes.Migrated"));
        } else {
          DLOG(ERROR) << "Theme is mysteriously gone.";
          ClearAllThemeData();
          content::RecordAction(UserMetricsAction("Themes.Gone"));
        }
      }
    }
  }
}

void ThemeService::NotifyThemeChanged() {
  DVLOG(1) << "Sending BROWSER_THEME_CHANGED";
  // Redraw!
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                  content::Source<ThemeService>(this),
                  content::NotificationService::NoDetails());
#if defined(OS_MACOSX)
  NotifyPlatformThemeChanged();
#endif  // OS_MACOSX
}

#if defined(OS_WIN) || defined(USE_AURA)
void ThemeService::FreePlatformCaches() {
  // Views (Skia) has no platform image cache to clear.
}
#endif

void ThemeService::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_EXTENSION_LOADED);
  const Extension* extension = content::Details<const Extension>(details).ptr();
  if (!extension->is_theme()) {
    return;
  }
  SetTheme(extension);
}

void ThemeService::SavePackName(const FilePath& pack_path) {
  profile_->GetPrefs()->SetFilePath(
      prefs::kCurrentThemePackFilename, pack_path);
}

void ThemeService::SaveThemeID(const std::string& id) {
  profile_->GetPrefs()->SetString(prefs::kCurrentThemeID, id);
}

void ThemeService::BuildFromExtension(const Extension* extension) {
  scoped_refptr<BrowserThemePack> pack(
      BrowserThemePack::BuildFromExtension(extension));
  if (!pack.get()) {
    // TODO(erg): We've failed to install the theme; perhaps we should tell the
    // user? http://crbug.com/34780
    LOG(ERROR) << "Could not load theme.";
    return;
  }

  // Write the packed file to disk.
  FilePath pack_path = extension->path().Append(chrome::kThemePackFilename);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&WritePackToDiskCallback, pack, pack_path));

  SavePackName(pack_path);
  theme_pack_ = pack;
}

void ThemeService::OnInfobarDisplayed() {
  number_of_infobars_++;
}

void ThemeService::OnInfobarDestroyed() {
  number_of_infobars_--;

  if (number_of_infobars_ == 0)
    RemoveUnusedThemes();
}
