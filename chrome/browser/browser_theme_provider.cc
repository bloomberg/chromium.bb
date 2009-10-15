// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_theme_provider.h"

#include "app/gfx/codec/png_codec.h"
#include "app/gfx/skbitmap_operations.h"
#include "base/file_util.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
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

// Strings used by themes to identify colors for different parts of our UI.
const char* BrowserThemeProvider::kColorFrame = "frame";
const char* BrowserThemeProvider::kColorFrameInactive = "frame_inactive";
const char* BrowserThemeProvider::kColorFrameIncognito = "frame_incognito";
const char* BrowserThemeProvider::kColorFrameIncognitoInactive =
    "frame_incognito_inactive";
const char* BrowserThemeProvider::kColorToolbar = "toolbar";
const char* BrowserThemeProvider::kColorTabText = "tab_text";
const char* BrowserThemeProvider::kColorBackgroundTabText =
    "tab_background_text";
const char* BrowserThemeProvider::kColorBookmarkText = "bookmark_text";
const char* BrowserThemeProvider::kColorNTPBackground = "ntp_background";
const char* BrowserThemeProvider::kColorNTPText = "ntp_text";
const char* BrowserThemeProvider::kColorNTPLink = "ntp_link";
const char* BrowserThemeProvider::kColorNTPLinkUnderline = "ntp_link_underline";
const char* BrowserThemeProvider::kColorNTPHeader = "ntp_header";
const char* BrowserThemeProvider::kColorNTPSection = "ntp_section";
const char* BrowserThemeProvider::kColorNTPSectionText = "ntp_section_text";
const char* BrowserThemeProvider::kColorNTPSectionLink = "ntp_section_link";
const char* BrowserThemeProvider::kColorNTPSectionLinkUnderline =
    "ntp_section_link_underline";
const char* BrowserThemeProvider::kColorControlBackground =
    "control_background";
const char* BrowserThemeProvider::kColorButtonBackground = "button_background";

// Strings used by themes to identify tints to apply to different parts of
// our UI. The frame tints apply to the frame color and produce the
// COLOR_FRAME* colors.
const char* BrowserThemeProvider::kTintButtons = "buttons";
const char* BrowserThemeProvider::kTintFrame = "frame";
const char* BrowserThemeProvider::kTintFrameInactive = "frame_inactive";
const char* BrowserThemeProvider::kTintFrameIncognito = "frame_incognito";
const char* BrowserThemeProvider::kTintFrameIncognitoInactive =
    "frame_incognito_inactive";
const char* BrowserThemeProvider::kTintBackgroundTab = "background_tab";

// Strings used by themes to identify miscellaneous numerical properties.
const char* BrowserThemeProvider::kDisplayPropertyNTPAlignment =
    "ntp_background_alignment";
const char* BrowserThemeProvider::kDisplayPropertyNTPTiling =
    "ntp_background_repeat";
const char* BrowserThemeProvider::kDisplayPropertyNTPInverseLogo =
    "ntp_logo_alternate";

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

// Default colors.
const SkColor BrowserThemeProvider::kDefaultColorFrame =
    SkColorSetRGB(77, 139, 217);
const SkColor BrowserThemeProvider::kDefaultColorFrameInactive =
    SkColorSetRGB(152, 188, 233);
const SkColor BrowserThemeProvider::kDefaultColorFrameIncognito =
    SkColorSetRGB(83, 106, 139);
const SkColor BrowserThemeProvider::kDefaultColorFrameIncognitoInactive =
    SkColorSetRGB(126, 139, 156);
const SkColor BrowserThemeProvider::kDefaultColorToolbar =
    SkColorSetRGB(210, 225, 246);
const SkColor BrowserThemeProvider::kDefaultColorTabText = SK_ColorBLACK;
const SkColor BrowserThemeProvider::kDefaultColorBackgroundTabText =
    SkColorSetRGB(64, 64, 64);
const SkColor BrowserThemeProvider::kDefaultColorBookmarkText =
    SkColorSetRGB(18, 50, 114);
#if defined(OS_WIN)
const SkColor BrowserThemeProvider::kDefaultColorNTPBackground =
    color_utils::GetSysSkColor(COLOR_WINDOW);
const SkColor BrowserThemeProvider::kDefaultColorNTPText =
    color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
const SkColor BrowserThemeProvider::kDefaultColorNTPLink =
    color_utils::GetSysSkColor(COLOR_HOTLIGHT);
#else
// TODO(beng): source from theme provider.
const SkColor BrowserThemeProvider::kDefaultColorNTPBackground = SK_ColorWHITE;
const SkColor BrowserThemeProvider::kDefaultColorNTPText = SK_ColorBLACK;
const SkColor BrowserThemeProvider::kDefaultColorNTPLink =
    SkColorSetRGB(6, 55, 116);
#endif
const SkColor BrowserThemeProvider::kDefaultColorNTPHeader =
    SkColorSetRGB(75, 140, 220);
const SkColor BrowserThemeProvider::kDefaultColorNTPSection =
    SkColorSetRGB(229, 239, 254);
const SkColor BrowserThemeProvider::kDefaultColorNTPSectionText = SK_ColorBLACK;
const SkColor BrowserThemeProvider::kDefaultColorNTPSectionLink =
    SkColorSetRGB(6, 55, 116);
const SkColor BrowserThemeProvider::kDefaultColorControlBackground =
    SkColorSetARGB(0, 0, 0, 0);
const SkColor BrowserThemeProvider::kDefaultColorButtonBackground =
    SkColorSetARGB(0, 0, 0, 0);

// Default tints.
const color_utils::HSL BrowserThemeProvider::kDefaultTintButtons =
    { -1, -1, -1 };
const color_utils::HSL BrowserThemeProvider::kDefaultTintFrame = { -1, -1, -1 };
const color_utils::HSL BrowserThemeProvider::kDefaultTintFrameInactive =
    { -1, -1, 0.75f };
const color_utils::HSL BrowserThemeProvider::kDefaultTintFrameIncognito =
    { -1, 0.2f, 0.35f };
const color_utils::HSL
    BrowserThemeProvider::kDefaultTintFrameIncognitoInactive =
    { -1, 0.3f, 0.6f };
const color_utils::HSL BrowserThemeProvider::kDefaultTintBackgroundTab =
    { -1, 0.5, 0.75 };

// Saved default values.
const char* BrowserThemeProvider::kDefaultThemeID = "";

// Default display properties.
static const int kDefaultDisplayPropertyNTPAlignment =
    BrowserThemeProvider::ALIGN_BOTTOM;
static const int kDefaultDisplayPropertyNTPTiling =
    BrowserThemeProvider::NO_REPEAT;
static const int kDefaultDisplayPropertyNTPInverseLogo = 0;

// The sum of kFrameBorderThickness and kNonClientRestoredExtraThickness from
// OpaqueBrowserFrameView.
static const int kRestoredTabVerticalOffset = 15;

// The image resources that will be tinted by the 'button' tint value.
static const int kToolbarButtonIDs[] = {
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

// A map for kToolbarButtonIDs.
static std::map<const int, bool> button_images_;

// The image resources we will allow people to theme.
static const int kThemeableImages[] = {
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

// A map for kThemeableImages.
static std::map<const int, bool> themeable_images;

// A map of frame image IDs to the tints for those ids.
typedef std::map<const int, int> FrameTintMap;
static FrameTintMap frame_tints;

Lock BrowserThemeProvider::themed_image_cache_lock_;

namespace {

class WriteImagesToDiskTask : public Task {
 public:
  WriteImagesToDiskTask(
      const BrowserThemeProvider::ImagesDiskCache& images_disk_cache,
      const BrowserThemeProvider::ImageCache& themed_image_cache) :
    images_disk_cache_(images_disk_cache),
    themed_image_cache_(themed_image_cache) {
  }

  virtual void Run() {
    AutoLock lock(BrowserThemeProvider::themed_image_cache_lock_);
    for (BrowserThemeProvider::ImagesDiskCache::const_iterator iter(
         images_disk_cache_.begin()); iter != images_disk_cache_.end();
         ++iter) {
      FilePath image_path = iter->first;
      BrowserThemeProvider::ImageCache::const_iterator themed_iter =
          themed_image_cache_.find(iter->second);
      if (themed_iter != themed_image_cache_.end()) {
        SkBitmap* bitmap = themed_iter->second;
        std::vector<unsigned char> image_data;
        if (!gfx::PNGCodec::EncodeBGRASkBitmap(*bitmap, false, &image_data)) {
          NOTREACHED() << "Image file could not be encoded.";
          return;
        }
        const char* image_data_ptr =
            reinterpret_cast<const char*>(&image_data[0]);
        if (!file_util::WriteFile(image_path,
            image_data_ptr, image_data.size())) {
          NOTREACHED() << "Image file could not be written to disk.";
          return;
        }
      } else {
        NOTREACHED() << "Themed image missing from cache.";
        return;
      }
    }
  }

 private:
  // References to data held in the BrowserThemeProvider.
  const BrowserThemeProvider::ImagesDiskCache& images_disk_cache_;
  const BrowserThemeProvider::ImageCache& themed_image_cache_;
};
}

BrowserThemeProvider::BrowserThemeProvider()
    : rb_(ResourceBundle::GetSharedInstance()),
      profile_(NULL),
      process_images_(false) {
  static bool initialized = false;
  if (!initialized) {
    for (size_t i = 0; i < arraysize(kToolbarButtonIDs); ++i)
      button_images_[kToolbarButtonIDs[i]] = true;
    for (size_t i = 0; i < arraysize(kThemeableImages); ++i)
      themeable_images[kThemeableImages[i]] = true;
    frame_tints[IDR_THEME_FRAME] = TINT_FRAME;
    frame_tints[IDR_THEME_FRAME_INACTIVE] = TINT_FRAME_INACTIVE;
    frame_tints[IDR_THEME_FRAME_OVERLAY] = TINT_FRAME;
    frame_tints[IDR_THEME_FRAME_OVERLAY_INACTIVE] = TINT_FRAME_INACTIVE;
    frame_tints[IDR_THEME_FRAME_INCOGNITO] = TINT_FRAME_INCOGNITO;
    frame_tints[IDR_THEME_FRAME_INCOGNITO_INACTIVE] =
        TINT_FRAME_INCOGNITO_INACTIVE;

    resource_names_[IDR_THEME_FRAME] = "theme_frame";
    resource_names_[IDR_THEME_FRAME_INACTIVE] = "theme_frame_inactive";
    resource_names_[IDR_THEME_FRAME_OVERLAY] = "theme_frame_overlay";
    resource_names_[IDR_THEME_FRAME_OVERLAY_INACTIVE] =
        "theme_frame_overlay_inactive";
    resource_names_[IDR_THEME_FRAME_INCOGNITO] = "theme_frame_incognito";
    resource_names_[IDR_THEME_FRAME_INCOGNITO_INACTIVE] =
        "theme_frame_incognito_inactive";
    resource_names_[IDR_THEME_TAB_BACKGROUND] = "theme_tab_background";
    resource_names_[IDR_THEME_TAB_BACKGROUND_INCOGNITO] =
        "theme_tab_background_incognito";
    resource_names_[IDR_THEME_TOOLBAR] = "theme_toolbar";
    resource_names_[IDR_THEME_TAB_BACKGROUND_V] = "theme_tab_background_v";
    resource_names_[IDR_THEME_NTP_BACKGROUND] = "theme_ntp_background";
    resource_names_[IDR_THEME_BUTTON_BACKGROUND] = "theme_button_background";
    resource_names_[IDR_THEME_NTP_ATTRIBUTION] = "theme_ntp_attribution";
    resource_names_[IDR_THEME_WINDOW_CONTROL_BACKGROUND] =
        "theme_window_control_background";
  }
}

BrowserThemeProvider::~BrowserThemeProvider() {
  ClearCaches();
}

void BrowserThemeProvider::Init(Profile* profile) {
  DCHECK(CalledOnValidThread());
  profile_ = profile;

  image_dir_ = profile_->GetPath().Append(chrome::kThemeImagesDirname);
  if (!file_util::PathExists(image_dir_))
    file_util::CreateDirectory(image_dir_);

  LoadThemePrefs();
}

SkBitmap* BrowserThemeProvider::GetBitmapNamed(int id) const {
  DCHECK(CalledOnValidThread());

  // First check to see if the Skia image is in the themed cache. The themed
  // cache is not changed in this method, so it can remain unlocked.
  ImageCache::const_iterator themed_iter = themed_image_cache_.find(id);
  if (themed_iter != themed_image_cache_.end())
    return themed_iter->second;

  // If Skia image is not in themed cache, check regular cache, and possibly
  // generate and store.
  ImageCache::const_iterator image_iter = image_cache_.find(id);
  if (image_iter != image_cache_.end())
    return image_iter->second;

  scoped_ptr<SkBitmap> result;

  // Try to load the image from the extension.
  result.reset(LoadThemeBitmap(id));

  // If we still don't have an image, load it from resourcebundle.
  if (!result.get())
    result.reset(new SkBitmap(*rb_.GetBitmapNamed(id)));

  if (result.get()) {
    // If the requested image is part of the toolbar button set, and we have
    // a provided tint for that set, tint it appropriately.
    if (button_images_.count(id) && tints_.count(kTintButtons)) {
      SkBitmap* tinted =
          new SkBitmap(TintBitmap(*result.release(), TINT_BUTTONS));
      result.reset(tinted);
    }

    // We loaded successfully. Cache the bitmap.
    image_cache_[id] = result.get();
    return result.release();
  } else {
    NOTREACHED() << "Failed to load a requested image";
    return NULL;
  }
}

SkColor BrowserThemeProvider::GetColor(int id) const {
  DCHECK(CalledOnValidThread());

  // Special-case NTP header - if the color isn't provided, we fall back to
  // the section color.
  if (id == COLOR_NTP_HEADER) {
    ColorMap::const_iterator color_iter = colors_.find(kColorNTPHeader);
    if (color_iter != colors_.end())
      return color_iter->second;
    color_iter = colors_.find(kColorNTPSection);
    return (color_iter == colors_.end()) ?
        GetDefaultColor(id) : color_iter->second;
  }

  // Special case the underline colors to use semi transparent in case not
  // defined.
  if (id == COLOR_NTP_SECTION_LINK_UNDERLINE) {
    ColorMap::const_iterator color_iter =
        colors_.find(kColorNTPSectionLinkUnderline);
    if (color_iter != colors_.end())
      return color_iter->second;
    SkColor color_section_link = GetColor(COLOR_NTP_SECTION_LINK);
    return SkColorSetA(color_section_link, SkColorGetA(color_section_link) / 3);
  }

  if (id == COLOR_NTP_LINK_UNDERLINE) {
    ColorMap::const_iterator color_iter = colors_.find(kColorNTPLinkUnderline);
    if (color_iter != colors_.end())
      return color_iter->second;
    SkColor color_link = GetColor(COLOR_NTP_LINK);
    return SkColorSetA(color_link, SkColorGetA(color_link) / 3);
  }

  // TODO(glen): Figure out if we need to tint these. http://crbug.com/11578
  ColorMap::const_iterator color_iter = colors_.find(GetColorKey(id));
  return (color_iter == colors_.end()) ?
      GetDefaultColor(id) : color_iter->second;
}

bool BrowserThemeProvider::GetDisplayProperty(int id, int* result) const {
  switch (id) {
    case NTP_BACKGROUND_ALIGNMENT: {
      DisplayPropertyMap::const_iterator display_iter =
          display_properties_.find(kDisplayPropertyNTPAlignment);
      *result = (display_iter == display_properties_.end()) ?
          kDefaultDisplayPropertyNTPAlignment : display_iter->second;
      return true;
    }
    case NTP_BACKGROUND_TILING: {
      DisplayPropertyMap::const_iterator display_iter =
          display_properties_.find(kDisplayPropertyNTPTiling);
      *result = (display_iter == display_properties_.end()) ?
          kDefaultDisplayPropertyNTPTiling : display_iter->second;
      return true;
    }
    case NTP_LOGO_ALTERNATE: {
      DisplayPropertyMap::const_iterator display_iter =
          display_properties_.find(kDisplayPropertyNTPInverseLogo);
      *result = (display_iter == display_properties_.end()) ?
          kDefaultDisplayPropertyNTPInverseLogo : display_iter->second;
      return true;
    }
    default:
      NOTREACHED() << "Unknown property requested";
  }
  return false;
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
  if (!themeable_images.count(id))
    return false;

  // A custom image = base name is NOT equal to resource name.  See note in
  // SaveThemeBitmap describing the process by which an original image is
  // tagged.
  ImageMap::const_iterator images_iter = images_.find(id);
  ResourceNameMap::const_iterator names_iter = resource_names_.find(id);
  if ((images_iter == images_.end()) || (names_iter == resource_names_.end()))
    return false;
  return !EndsWith(UTF8ToWide(images_iter->second),
                   UTF8ToWide(names_iter->second), false);
}

bool BrowserThemeProvider::GetRawData(
    int id,
    std::vector<unsigned char>* raw_data) const {
  // Check to see whether we should substitute some images.
  int ntp_alternate;
  GetDisplayProperty(NTP_LOGO_ALTERNATE, &ntp_alternate);
  if (id == IDR_PRODUCT_LOGO && ntp_alternate != 0)
    id = IDR_PRODUCT_LOGO_WHITE;

  RawDataMap::const_iterator data_iter = raw_data_.find(id);
  if (data_iter != raw_data_.end()) {
    *raw_data = data_iter->second;
    return true;
  }

  if (!ReadThemeFileData(id, raw_data) &&
      !rb_.LoadImageResourceBytes(id, raw_data))
    return false;

  raw_data_[id] = *raw_data;
  return true;
}

void BrowserThemeProvider::SetTheme(Extension* extension) {
  // Clear our image cache.
  ClearCaches();

  DCHECK(extension);
  DCHECK(extension->IsTheme());
  SetImageData(extension->GetThemeImages(),
               extension->path());
  SetColorData(extension->GetThemeColors());
  SetTintData(extension->GetThemeTints());

  // Drop out to default theme if the theme data is empty.
  if (images_.empty() && colors_.empty() && tints_.empty()) {
    UseDefaultTheme();
    return;
  }

  SetDisplayPropertyData(extension->GetThemeDisplayProperties());
  raw_data_.clear();

  SaveImageData(extension->GetThemeImages());
  SaveColorData();
  SaveTintData();
  SaveDisplayPropertyData();
  SaveThemeID(extension->id());

  // Process all images when we first set theme.
  process_images_ = true;

  GenerateFrameColors();
  if (ShouldTintFrames()) {
    AutoLock lock(themed_image_cache_lock_);
    GenerateFrameImages();
    GenerateTabImages();
  }

  WriteImagesToDisk();
  NotifyThemeChanged();
  UserMetrics::RecordAction(L"Themes_Installed", profile_);
}

void BrowserThemeProvider::UseDefaultTheme() {
  ClearAllThemeData();
  NotifyThemeChanged();
  UserMetrics::RecordAction(L"Themes_Reset", profile_);
}

std::string BrowserThemeProvider::GetThemeID() const {
  std::wstring id = profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);
  return WideToUTF8(id);
}

bool BrowserThemeProvider::ReadThemeFileData(
    int id, std::vector<unsigned char>* raw_data) const {
  ImageMap::const_iterator images_iter = images_.find(id);
  if (images_iter != images_.end()) {
    // First check to see if we have a registered theme extension and whether
    // it can handle this resource.
#if defined(OS_WIN)
    FilePath path = FilePath(UTF8ToWide(images_iter->second));
#else
    FilePath path = FilePath(images_iter->second);
#endif
    if (!path.empty()) {
      net::FileStream file;
      int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
      if (file.Open(path, flags) == net::OK) {
        int64 avail = file.Available();
        if (avail > 0 && avail < INT_MAX) {
          size_t size = static_cast<size_t>(avail);
          raw_data->resize(size);
          char* data = reinterpret_cast<char*>(&(raw_data->front()));
          if (file.ReadUntilComplete(data, size) == avail)
            return true;
        }
      }
    }
  }

  return false;
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

void BrowserThemeProvider::SetColor(const char* key, const SkColor& color) {
  colors_[key] = color;
}

void BrowserThemeProvider::SetTint(const char* key,
                                   const color_utils::HSL& tint) {
  tints_[key] = tint;
}

color_utils::HSL BrowserThemeProvider::GetTint(int id) const {
  DCHECK(CalledOnValidThread());

  TintMap::const_iterator tint_iter = tints_.find(GetTintKey(id));
  return (tint_iter == tints_.end()) ? GetDefaultTint(id) : tint_iter->second;
}

void BrowserThemeProvider::GenerateFrameColors() {
  // Generate any secondary frame colors that weren't provided.
  SkColor frame = GetColor(COLOR_FRAME);

  if (!colors_.count(kColorFrame))
    colors_[kColorFrame] = HSLShift(frame, GetTint(TINT_FRAME));
  if (!colors_.count(kColorFrameInactive)) {
    colors_[kColorFrameInactive] =
        HSLShift(frame, GetTint(TINT_FRAME_INACTIVE));
  }
  if (!colors_.count(kColorFrameIncognito)) {
    colors_[kColorFrameIncognito] =
        HSLShift(frame, GetTint(TINT_FRAME_INCOGNITO));
  }
  if (!colors_.count(kColorFrameIncognitoInactive)) {
    colors_[kColorFrameIncognitoInactive] =
        HSLShift(frame, GetTint(TINT_FRAME_INCOGNITO_INACTIVE));
  }
}

void BrowserThemeProvider::GenerateFrameImages() const {
  for (FrameTintMap::const_iterator iter(frame_tints.begin());
       iter != frame_tints.end(); ++iter) {
    int id = iter->first;
    scoped_ptr<SkBitmap> frame;
    // If there's no frame image provided for the specified id, then load
    // the default provided frame. If that's not provided, skip this whole
    // thing and just use the default images.
    int base_id;
    std::string resource_name;

    // If we've already processed the images for this theme, they're all
    // waiting on disk -- just load them in.
    if (!process_images_) {
      frame.reset(LoadThemeBitmap(id));
      if (frame.get())
        themed_image_cache_[id] = new SkBitmap(*frame.get());
    } else {
      resource_name = resource_names_.find(id)->second;
      if (id == IDR_THEME_FRAME_INCOGNITO_INACTIVE) {
        base_id = HasCustomImage(IDR_THEME_FRAME_INCOGNITO) ?
            IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME;
      } else if (id == IDR_THEME_FRAME_OVERLAY_INACTIVE) {
        base_id = IDR_THEME_FRAME_OVERLAY;
      } else if (id == IDR_THEME_FRAME_INACTIVE) {
        base_id = IDR_THEME_FRAME;
      } else if (id == IDR_THEME_FRAME_INCOGNITO &&
                 !HasCustomImage(IDR_THEME_FRAME_INCOGNITO)) {
        base_id = IDR_THEME_FRAME;
      } else {
        base_id = id;
      }

      if (HasCustomImage(id)) {
        frame.reset(LoadThemeBitmap(id));
      } else if (base_id != id && HasCustomImage(base_id)) {
        frame.reset(LoadThemeBitmap(base_id));
      } else if (base_id == IDR_THEME_FRAME_OVERLAY &&
                 HasCustomImage(IDR_THEME_FRAME)) {
        // If there is no theme overlay, don't tint the default frame,
        // because it will overwrite the custom frame image when we cache and
        // reload from disk.
        frame.reset(NULL);
      } else {
        // If the theme doesn't specify an image, then apply the tint to
        // the default frame.
        frame.reset(new SkBitmap(*rb_.GetBitmapNamed(IDR_THEME_FRAME)));
      }

      if (frame.get()) {
        SkBitmap* tinted = new SkBitmap(TintBitmap(*frame, iter->second));
        themed_image_cache_[id] = tinted;
        SaveThemeBitmap(resource_name, id);
      }
    }
  }
}

void BrowserThemeProvider::GenerateTabImages() const {
  GenerateTabBackgroundBitmap(IDR_THEME_TAB_BACKGROUND);
  GenerateTabBackgroundBitmap(IDR_THEME_TAB_BACKGROUND_INCOGNITO);
}

void BrowserThemeProvider::ClearAllThemeData() {
  // Clear our image cache.
  ClearCaches();

  images_.clear();
  colors_.clear();
  tints_.clear();
  display_properties_.clear();
  raw_data_.clear();

  SaveImageData(NULL);
  SaveColorData();
  SaveTintData();
  SaveDisplayPropertyData();
  SaveThemeID(kDefaultThemeID);
}

void BrowserThemeProvider::LoadThemePrefs() {
  process_images_ = false;
  PrefService* prefs = profile_->GetPrefs();

  // TODO(glen): Figure out if any custom prefs were loaded, and if so UMA-log
  // the fact that a theme was loaded.
  if (!prefs->HasPrefPath(prefs::kCurrentThemeImages) &&
      !prefs->HasPrefPath(prefs::kCurrentThemeColors) &&
      !prefs->HasPrefPath(prefs::kCurrentThemeTints))
    return;

  // Our prefs already have the extension path baked in, so we don't need to
  // provide it.
  SetImageData(prefs->GetMutableDictionary(prefs::kCurrentThemeImages),
               FilePath());
  SetColorData(prefs->GetMutableDictionary(prefs::kCurrentThemeColors));
  SetTintData(prefs->GetMutableDictionary(prefs::kCurrentThemeTints));
  SetDisplayPropertyData(
      prefs->GetMutableDictionary(prefs::kCurrentThemeDisplayProperties));

  // If we're not loading the frame from the cached image dir, we are using an
  // old preferences file, or the processed images were not saved correctly.
  // Force image reprocessing and caching.
  ImageMap::const_iterator images_iter = images_.find(IDR_THEME_FRAME);
  if (images_iter != images_.end()) {
#if defined(OS_WIN)
    FilePath cache_path = FilePath(UTF8ToWide(images_iter->second));
#else
    FilePath cache_path = FilePath(images_iter->second);
#endif
    process_images_ = !file_util::ContainsPath(image_dir_, cache_path);
  }

  GenerateFrameColors();
  // Scope for AutoLock on themed_image_cache.
  {
    AutoLock lock(themed_image_cache_lock_);
    GenerateFrameImages();
    GenerateTabImages();
  }

  if (process_images_) {
    WriteImagesToDisk();
    UserMetrics::RecordAction(L"Migrated noncached to cached theme.", profile_);
  }
  UserMetrics::RecordAction(L"Themes_loaded", profile_);
}

void BrowserThemeProvider::NotifyThemeChanged() {
  // Redraw!
  NotificationService* service = NotificationService::current();
  service->Notify(NotificationType::BROWSER_THEME_CHANGED,
                  Source<BrowserThemeProvider>(this),
                  NotificationService::NoDetails());
}

SkBitmap* BrowserThemeProvider::LoadThemeBitmap(int id) const {
  DCHECK(CalledOnValidThread());

  if (!themeable_images.count(id))
    return NULL;

  // Attempt to find the image in our theme bundle.
  std::vector<unsigned char> raw_data, png_data;
  if (ReadThemeFileData(id, &raw_data)) {
    // Decode the PNG.
    int image_width = 0;
    int image_height = 0;

    if (!gfx::PNGCodec::Decode(&raw_data.front(), raw_data.size(),
                               gfx::PNGCodec::FORMAT_BGRA, &png_data,
                               &image_width, &image_height)) {
      NOTREACHED() << "Unable to decode theme image resource " << id;
      return NULL;
    }

    return gfx::PNGCodec::CreateSkBitmapFromBGRAFormat(png_data,
                                                       image_width,
                                                       image_height);
  } else {
    // TODO(glen): File no-longer exists, we're out of date. We should
    // clear the theme (or maybe just the pref that points to this
    // image).
    return NULL;
  }
}

void BrowserThemeProvider::SaveThemeBitmap(std::string resource_name,
                                           int id) const {
  DCHECK(CalledOnValidThread());
  if (!themed_image_cache_.count(id)) {
    NOTREACHED();
    return;
  }

  // The images_ directory, at this point, contains only the custom images
  // provided by the extension. We tag these images "_original" in the prefs
  // file so we can distinguish them from images which have been generated and
  // saved to disk by the theme caching process (WriteImagesToDisk).  This way,
  // when we call HasCustomImage, we can check for the "_original" tag to see
  // whether an image was originally provided by the extension, or saved
  // in the caching process.
  if (images_.count(id))
    resource_name.append("_original");

#if defined(OS_WIN)
  FilePath image_path = image_dir_.Append(UTF8ToWide(resource_name));
#elif defined(OS_POSIX)
  FilePath image_path = image_dir_.Append(resource_name);
#endif

  images_disk_cache_[image_path] = id;
}

#if defined(OS_WIN)
void BrowserThemeProvider::FreePlatformCaches() {
  // Views (Skia) has no platform image cache to clear.
}
#endif

SkBitmap* BrowserThemeProvider::GenerateTabBackgroundBitmapImpl(int id) const {
  int base_id = (id == IDR_THEME_TAB_BACKGROUND) ?
      IDR_THEME_FRAME : IDR_THEME_FRAME_INCOGNITO;
  // According to Miranda, it is safe to read from the themed_image_cache_ here
  // because we only lock to write on the UI thread, and we only lock to read
  // on the cache writing thread.
  ImageCache::const_iterator themed_iter = themed_image_cache_.find(base_id);
  if (themed_iter == themed_image_cache_.end())
    return NULL;

  SkBitmap bg_tint = TintBitmap(*(themed_iter->second), TINT_BACKGROUND_TAB);
  int vertical_offset = HasCustomImage(id) ? kRestoredTabVerticalOffset : 0;
  SkBitmap* bg_tab = new SkBitmap(SkBitmapOperations::CreateTiledBitmap(
      bg_tint, 0, vertical_offset, bg_tint.width(), bg_tint.height()));

  // If they've provided a custom image, overlay it.
  if (HasCustomImage(id)) {
    SkBitmap* overlay = LoadThemeBitmap(id);
    if (overlay) {
      SkCanvas canvas(*bg_tab);
      for (int x = 0; x < bg_tab->width(); x += overlay->width())
        canvas.drawBitmap(*overlay, static_cast<SkScalar>(x), 0, NULL);
    }
  }

  return bg_tab;
}

const std::string BrowserThemeProvider::GetTintKey(int id) const {
  switch (id) {
    case TINT_FRAME:
      return kTintFrame;
    case TINT_FRAME_INACTIVE:
      return kTintFrameInactive;
    case TINT_FRAME_INCOGNITO:
      return kTintFrameIncognito;
    case TINT_FRAME_INCOGNITO_INACTIVE:
      return kTintFrameIncognitoInactive;
    case TINT_BUTTONS:
      return kTintButtons;
    case TINT_BACKGROUND_TAB:
      return kTintBackgroundTab;
    default:
      NOTREACHED() << "Unknown tint requested";
      return "";
  }
}

color_utils::HSL BrowserThemeProvider::GetDefaultTint(int id) const {
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

const std::string BrowserThemeProvider::GetColorKey(int id) const {
  switch (id) {
    case COLOR_FRAME:
      return kColorFrame;
    case COLOR_FRAME_INACTIVE:
      return kColorFrameInactive;
    case COLOR_FRAME_INCOGNITO:
      return kColorFrameIncognito;
    case COLOR_FRAME_INCOGNITO_INACTIVE:
      return kColorFrameIncognitoInactive;
    case COLOR_TOOLBAR:
      return kColorToolbar;
    case COLOR_TAB_TEXT:
      return kColorTabText;
    case COLOR_BACKGROUND_TAB_TEXT:
      return kColorBackgroundTabText;
    case COLOR_BOOKMARK_TEXT:
      return kColorBookmarkText;
    case COLOR_NTP_BACKGROUND:
      return kColorNTPBackground;
    case COLOR_NTP_TEXT:
      return kColorNTPText;
    case COLOR_NTP_LINK:
      return kColorNTPLink;
    case COLOR_NTP_LINK_UNDERLINE:
      return kColorNTPLinkUnderline;
    case COLOR_NTP_HEADER:
      return kColorNTPHeader;
    case COLOR_NTP_SECTION:
      return kColorNTPSection;
    case COLOR_NTP_SECTION_TEXT:
      return kColorNTPSectionText;
    case COLOR_NTP_SECTION_LINK:
      return kColorNTPSectionLink;
    case COLOR_NTP_SECTION_LINK_UNDERLINE:
      return kColorNTPSectionLinkUnderline;
    case COLOR_CONTROL_BACKGROUND:
      return kColorControlBackground;
    case COLOR_BUTTON_BACKGROUND:
      return kColorButtonBackground;
    default:
      NOTREACHED() << "Unknown color requested";
      return "";
  }
}

SkColor BrowserThemeProvider::GetDefaultColor(int id) const {
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
    case COLOR_NTP_HEADER:
      return kDefaultColorNTPHeader;
    case COLOR_NTP_SECTION:
      return kDefaultColorNTPSection;
    case COLOR_NTP_SECTION_TEXT:
      return kDefaultColorNTPSectionText;
    case COLOR_NTP_SECTION_LINK:
      return kDefaultColorNTPSectionLink;
    case COLOR_CONTROL_BACKGROUND:
      return kDefaultColorControlBackground;
    case COLOR_BUTTON_BACKGROUND:
      return kDefaultColorButtonBackground;
    default:
      // Return a debugging red color.
      return 0xffff0000;
  }
}

SkBitmap BrowserThemeProvider::TintBitmap(const SkBitmap& bitmap,
                                          int hsl_id) const {
  return SkBitmapOperations::CreateHSLShiftedBitmap(bitmap, GetTint(hsl_id));
}

void BrowserThemeProvider::SetImageData(DictionaryValue* images_value,
                                        FilePath images_path) {
  images_.clear();

  if (!images_value)
    return;

  for (DictionaryValue::key_iterator iter(images_value->begin_keys());
       iter != images_value->end_keys(); ++iter) {
    std::string val;
    if (images_value->GetString(*iter, &val)) {
      int id = ThemeResourcesUtil::GetId(WideToUTF8(*iter));
      if (id != -1) {
        if (!images_path.empty()) {
          images_[id] =
              WideToUTF8(images_path.AppendASCII(val).ToWStringHack());
          resource_names_[id] = WideToASCII(*iter);
        } else {
          images_[id] = val;
        }
      }
    }
  }
}

void BrowserThemeProvider::SetColorData(DictionaryValue* colors_value) {
  colors_.clear();

  if (!colors_value)
    return;

  for (DictionaryValue::key_iterator iter(colors_value->begin_keys());
       iter != colors_value->end_keys(); ++iter) {
    ListValue* color_list;
    if (colors_value->GetList(*iter, &color_list) &&
        ((color_list->GetSize() == 3) || (color_list->GetSize() == 4))) {
      int r, g, b;
      color_list->GetInteger(0, &r);
      color_list->GetInteger(1, &g);
      color_list->GetInteger(2, &b);
      if (color_list->GetSize() == 4) {
        double alpha;
        int alpha_int;
        if (color_list->GetReal(3, &alpha)) {
          colors_[WideToUTF8(*iter)] =
              SkColorSetARGB(static_cast<int>(alpha * 255), r, g, b);
        } else if (color_list->GetInteger(3, &alpha_int)) {
          colors_[WideToUTF8(*iter)] =
              SkColorSetARGB(alpha_int * 255, r, g, b);
        }
      } else {
        colors_[WideToUTF8(*iter)] = SkColorSetRGB(r, g, b);
      }
    }
  }
}

void BrowserThemeProvider::SetTintData(DictionaryValue* tints_value) {
  tints_.clear();

  if (!tints_value)
    return;

  for (DictionaryValue::key_iterator iter(tints_value->begin_keys());
       iter != tints_value->end_keys(); ++iter) {
    ListValue* tint_list;
    if (tints_value->GetList(*iter, &tint_list) &&
        (tint_list->GetSize() == 3)) {
      color_utils::HSL hsl = { -1, -1, -1 };
      int value = 0;
      if (!tint_list->GetReal(0, &hsl.h) && tint_list->GetInteger(0, &value))
        hsl.h = value;
      if (!tint_list->GetReal(1, &hsl.s) &&  tint_list->GetInteger(1, &value))
        hsl.s = value;
      if (!tint_list->GetReal(2, &hsl.l) && tint_list->GetInteger(2, &value))
        hsl.l = value;

      tints_[WideToUTF8(*iter)] = hsl;
    }
  }
}

void BrowserThemeProvider::SetDisplayPropertyData(
    DictionaryValue* display_properties_value) {
  display_properties_.clear();

  if (!display_properties_value)
    return;

  for (DictionaryValue::key_iterator iter(
       display_properties_value->begin_keys());
       iter != display_properties_value->end_keys(); ++iter) {
    // New tab page alignment and background tiling.
    if (base::strcasecmp(WideToUTF8(*iter).c_str(),
                         kDisplayPropertyNTPAlignment) == 0) {
      std::string val;
      if (display_properties_value->GetString(*iter, &val)) {
        display_properties_[kDisplayPropertyNTPAlignment] =
            StringToAlignment(val);
      }
    } else if (base::strcasecmp(WideToUTF8(*iter).c_str(),
                                kDisplayPropertyNTPTiling) == 0) {
      std::string val;
      if (display_properties_value->GetString(*iter, &val)) {
        display_properties_[kDisplayPropertyNTPTiling] =
            StringToTiling(val);
      }
    }
    if (base::strcasecmp(WideToUTF8(*iter).c_str(),
                         kDisplayPropertyNTPInverseLogo) == 0) {
      int val = 0;
      if (display_properties_value->GetInteger(*iter, &val))
        display_properties_[kDisplayPropertyNTPInverseLogo] = val;
    }
  }
}

SkBitmap* BrowserThemeProvider::GenerateTabBackgroundBitmap(int id) const {
  if (id == IDR_THEME_TAB_BACKGROUND ||
      id == IDR_THEME_TAB_BACKGROUND_INCOGNITO) {
    // The requested image is a background tab. Get a frame to create the
    // tab against. As themes don't use the glass frame, we don't have to
    // worry about compositing them together, as our default theme provides
    // the necessary bitmaps.
    if (!process_images_) {
      scoped_ptr<SkBitmap> frame;
      frame.reset(LoadThemeBitmap(id));
      if (frame.get())
        themed_image_cache_[id] = new SkBitmap(*frame.get());
    } else {
      SkBitmap* bg_tab = GenerateTabBackgroundBitmapImpl(id);

      if (bg_tab) {
        std::string resource_name((id == IDR_THEME_TAB_BACKGROUND) ?
            "theme_tab_background" : "theme_tab_background_incognito");
        themed_image_cache_[id] = bg_tab;
        SaveThemeBitmap(resource_name, id);
        return bg_tab;
      }
    }
  }
  return NULL;
}

void BrowserThemeProvider::SaveImageData(DictionaryValue* images_value) const {
  // Save our images data.
  DictionaryValue* pref_images =
      profile_->GetPrefs()->GetMutableDictionary(prefs::kCurrentThemeImages);
  pref_images->Clear();

  if (!images_value)
    return;

  for (DictionaryValue::key_iterator iter(images_value->begin_keys());
       iter != images_value->end_keys(); ++iter) {
    std::string val;
    if (images_value->GetString(*iter, &val)) {
      int id = ThemeResourcesUtil::GetId(WideToUTF8(*iter));
      if (id != -1)
        pref_images->SetString(*iter, images_.find(id)->second);
    }
  }
}

void BrowserThemeProvider::SaveColorData() const {
  // Save our color data.
  DictionaryValue* pref_colors =
      profile_->GetPrefs()->GetMutableDictionary(prefs::kCurrentThemeColors);
  pref_colors->Clear();

  if (colors_.empty())
    return;

  for (ColorMap::const_iterator iter(colors_.begin()); iter != colors_.end();
       ++iter) {
    SkColor rgba = iter->second;
    ListValue* rgb_list = new ListValue();
    rgb_list->Set(0, Value::CreateIntegerValue(SkColorGetR(rgba)));
    rgb_list->Set(1, Value::CreateIntegerValue(SkColorGetG(rgba)));
    rgb_list->Set(2, Value::CreateIntegerValue(SkColorGetB(rgba)));
    if (SkColorGetA(rgba) != 255)
      rgb_list->Set(3, Value::CreateRealValue(SkColorGetA(rgba) / 255.0));
    pref_colors->Set(UTF8ToWide(iter->first), rgb_list);
  }
}

void BrowserThemeProvider::SaveTintData() const {
  // Save our tint data.
  DictionaryValue* pref_tints =
      profile_->GetPrefs()->GetMutableDictionary(prefs::kCurrentThemeTints);
  pref_tints->Clear();

  if (tints_.empty())
    return;

  for (TintMap::const_iterator iter(tints_.begin()); iter != tints_.end();
       ++iter) {
    color_utils::HSL hsl = iter->second;
    ListValue* hsl_list = new ListValue();
    hsl_list->Set(0, Value::CreateRealValue(hsl.h));
    hsl_list->Set(1, Value::CreateRealValue(hsl.s));
    hsl_list->Set(2, Value::CreateRealValue(hsl.l));
    pref_tints->Set(UTF8ToWide(iter->first), hsl_list);
  }
}

void BrowserThemeProvider::SaveDisplayPropertyData() const {
  // Save our display property data.
  DictionaryValue* pref_display_properties =
      profile_->GetPrefs()->
          GetMutableDictionary(prefs::kCurrentThemeDisplayProperties);
  pref_display_properties->Clear();

  if (display_properties_.empty())
    return;

  for (DisplayPropertyMap::const_iterator iter(display_properties_.begin());
       iter != display_properties_.end(); ++iter) {
    if (base::strcasecmp(iter->first.c_str(),
                         kDisplayPropertyNTPAlignment) == 0) {
      pref_display_properties->SetString(UTF8ToWide(iter->first),
                                         AlignmentToString(iter->second));
    } else if (base::strcasecmp(iter->first.c_str(),
                                kDisplayPropertyNTPTiling) == 0) {
      pref_display_properties->SetString(UTF8ToWide(iter->first),
                                         TilingToString(iter->second));
    }
    if (base::strcasecmp(iter->first.c_str(),
                         kDisplayPropertyNTPInverseLogo) == 0) {
      pref_display_properties->SetInteger(UTF8ToWide(iter->first),
                                          iter->second);
    }
  }
}

void BrowserThemeProvider::SaveCachedImageData() const {
  DictionaryValue* pref_images =
      profile_->GetPrefs()->GetMutableDictionary(prefs::kCurrentThemeImages);

  for (ImagesDiskCache::const_iterator it(images_disk_cache_.begin());
       it != images_disk_cache_.end(); ++it) {
    std::wstring disk_path = it->first.ToWStringHack();
    std::string pref_name = resource_names_.find(it->second)->second;
    pref_images->SetString(UTF8ToWide(pref_name), WideToUTF8(disk_path));
  }
  profile_->GetPrefs()->SavePersistentPrefs();
}

void BrowserThemeProvider::SaveThemeID(const std::string& id) {
  profile_->GetPrefs()->SetString(prefs::kCurrentThemeID, UTF8ToWide(id));
}

void BrowserThemeProvider::ClearCaches() {
  FreePlatformCaches();
  STLDeleteValues(&image_cache_);

  // Scope for AutoLock on themed_image_cache.
  {
    AutoLock lock(themed_image_cache_lock_);
    STLDeleteValues(&themed_image_cache_);
  }

  images_disk_cache_.clear();
}

void BrowserThemeProvider::WriteImagesToDisk() const {
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      new WriteImagesToDiskTask(images_disk_cache_, themed_image_cache_));
  SaveCachedImageData();
}

bool BrowserThemeProvider::ShouldTintFrames() const {
  return (HasCustomImage(IDR_THEME_FRAME) ||
      tints_.count(GetTintKey(TINT_BACKGROUND_TAB)) ||
      tints_.count(GetTintKey(TINT_FRAME)) ||
      tints_.count(GetTintKey(TINT_FRAME_INACTIVE)) ||
      tints_.count(GetTintKey(TINT_FRAME_INCOGNITO)) ||
      tints_.count(GetTintKey(TINT_FRAME_INCOGNITO_INACTIVE)));
}
