// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/browser_theme_pack.h"

#include <limits>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/common/extensions/api/themes/theme_handler.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/id_util.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;
using extensions::Extension;

namespace {

// Version number of the current theme pack. We just throw out and rebuild
// theme packs that aren't int-equal to this. Increment this number if you
// change default theme assets.
const int kThemePackVersion = 30;

// IDs that are in the DataPack won't clash with the positive integer
// uint16. kHeaderID should always have the maximum value because we want the
// "header" to be written last. That way we can detect whether the pack was
// successfully written and ignore and regenerate if it was only partially
// written (i.e. chrome crashed on a different thread while writing the pack).
const int kMaxID = 0x0000FFFF;  // Max unsigned 16-bit int.
const int kHeaderID = kMaxID - 1;
const int kTintsID = kMaxID - 2;
const int kColorsID = kMaxID - 3;
const int kDisplayPropertiesID = kMaxID - 4;
const int kSourceImagesID = kMaxID - 5;
const int kScaleFactorsID = kMaxID - 6;

// The sum of kFrameBorderThickness and kNonClientRestoredExtraThickness from
// OpaqueBrowserFrameView.
const int kRestoredTabVerticalOffset = 15;

// Persistent constants for the main images that we need. These have the same
// names as their IDR_* counterparts but these values will always stay the
// same.
const int PRS_THEME_FRAME = 1;
const int PRS_THEME_FRAME_INACTIVE = 2;
const int PRS_THEME_FRAME_INCOGNITO = 3;
const int PRS_THEME_FRAME_INCOGNITO_INACTIVE = 4;
const int PRS_THEME_TOOLBAR = 5;
const int PRS_THEME_TAB_BACKGROUND = 6;
const int PRS_THEME_TAB_BACKGROUND_INCOGNITO = 7;
const int PRS_THEME_TAB_BACKGROUND_V = 8;
const int PRS_THEME_NTP_BACKGROUND = 9;
const int PRS_THEME_FRAME_OVERLAY = 10;
const int PRS_THEME_FRAME_OVERLAY_INACTIVE = 11;
const int PRS_THEME_BUTTON_BACKGROUND = 12;
const int PRS_THEME_NTP_ATTRIBUTION = 13;
const int PRS_THEME_WINDOW_CONTROL_BACKGROUND = 14;

struct PersistingImagesTable {
  // A non-changing integer ID meant to be saved in theme packs. This ID must
  // not change between versions of chrome.
  int persistent_id;

  // The IDR that depends on the whims of GRIT and therefore changes whenever
  // someone adds a new resource.
  int idr_id;

  // String to check for when parsing theme manifests or NULL if this isn't
  // supposed to be changeable by the user.
  const char* key;
};

// IDR_* resource names change whenever new resources are added; use persistent
// IDs when storing to a cached pack.
PersistingImagesTable kPersistingImages[] = {
  { PRS_THEME_FRAME, IDR_THEME_FRAME,
    "theme_frame" },
  { PRS_THEME_FRAME_INACTIVE, IDR_THEME_FRAME_INACTIVE,
    "theme_frame_inactive" },
  { PRS_THEME_FRAME_INCOGNITO, IDR_THEME_FRAME_INCOGNITO,
    "theme_frame_incognito" },
  { PRS_THEME_FRAME_INCOGNITO_INACTIVE, IDR_THEME_FRAME_INCOGNITO_INACTIVE,
    "theme_frame_incognito_inactive" },
  { PRS_THEME_TOOLBAR, IDR_THEME_TOOLBAR,
    "theme_toolbar" },
  { PRS_THEME_TAB_BACKGROUND, IDR_THEME_TAB_BACKGROUND,
    "theme_tab_background" },
  { PRS_THEME_TAB_BACKGROUND_INCOGNITO, IDR_THEME_TAB_BACKGROUND_INCOGNITO,
    "theme_tab_background_incognito" },
  { PRS_THEME_TAB_BACKGROUND_V, IDR_THEME_TAB_BACKGROUND_V,
    "theme_tab_background_v"},
  { PRS_THEME_NTP_BACKGROUND, IDR_THEME_NTP_BACKGROUND,
    "theme_ntp_background" },
  { PRS_THEME_FRAME_OVERLAY, IDR_THEME_FRAME_OVERLAY,
    "theme_frame_overlay" },
  { PRS_THEME_FRAME_OVERLAY_INACTIVE, IDR_THEME_FRAME_OVERLAY_INACTIVE,
    "theme_frame_overlay_inactive" },
  { PRS_THEME_BUTTON_BACKGROUND, IDR_THEME_BUTTON_BACKGROUND,
    "theme_button_background" },
  { PRS_THEME_NTP_ATTRIBUTION, IDR_THEME_NTP_ATTRIBUTION,
    "theme_ntp_attribution" },
  { PRS_THEME_WINDOW_CONTROL_BACKGROUND, IDR_THEME_WINDOW_CONTROL_BACKGROUND,
    "theme_window_control_background"},

  // The rest of these entries have no key because they can't be overridden
  // from the json manifest.
  { 15, IDR_BACK, NULL },
  { 16, IDR_BACK_D, NULL },
  { 17, IDR_BACK_H, NULL },
  { 18, IDR_BACK_P, NULL },
  { 19, IDR_FORWARD, NULL },
  { 20, IDR_FORWARD_D, NULL },
  { 21, IDR_FORWARD_H, NULL },
  { 22, IDR_FORWARD_P, NULL },
  { 23, IDR_HOME, NULL },
  { 24, IDR_HOME_H, NULL },
  { 25, IDR_HOME_P, NULL },
  { 26, IDR_RELOAD, NULL },
  { 27, IDR_RELOAD_H, NULL },
  { 28, IDR_RELOAD_P, NULL },
  { 29, IDR_STOP, NULL },
  { 30, IDR_STOP_D, NULL },
  { 31, IDR_STOP_H, NULL },
  { 32, IDR_STOP_P, NULL },
  { 33, IDR_LOCATIONBG_C, NULL },
  { 34, IDR_LOCATIONBG_L, NULL },
  { 35, IDR_LOCATIONBG_R, NULL },
  { 36, IDR_BROWSER_ACTIONS_OVERFLOW, NULL },
  { 37, IDR_BROWSER_ACTIONS_OVERFLOW_H, NULL },
  { 38, IDR_BROWSER_ACTIONS_OVERFLOW_P, NULL },
  { 39, IDR_TOOLS, NULL },
  { 40, IDR_TOOLS_H, NULL },
  { 41, IDR_TOOLS_P, NULL },
  { 42, IDR_MENU_DROPARROW, NULL },
  { 43, IDR_THROBBER, NULL },
  { 44, IDR_THROBBER_WAITING, NULL },
  { 45, IDR_THROBBER_LIGHT, NULL },
  { 46, IDR_TOOLBAR_BEZEL_HOVER, NULL },
  { 47, IDR_TOOLBAR_BEZEL_PRESSED, NULL },
  { 48, IDR_TOOLS_BAR, NULL },
};
const size_t kPersistingImagesLength = arraysize(kPersistingImages);

#if defined(OS_WIN) && defined(USE_AURA)
// Persistent theme ids for Windows AURA.
const int PRS_THEME_FRAME_WIN = 100;
const int PRS_THEME_FRAME_INACTIVE_WIN = 101;
const int PRS_THEME_FRAME_INCOGNITO_WIN = 102;
const int PRS_THEME_FRAME_INCOGNITO_INACTIVE_WIN = 103;
const int PRS_THEME_TOOLBAR_WIN = 104;
const int PRS_THEME_TAB_BACKGROUND_WIN = 105;
const int PRS_THEME_TAB_BACKGROUND_INCOGNITO_WIN = 106;

// Persistent theme to resource id mapping for Windows AURA.
PersistingImagesTable kPersistingImagesWinDesktopAura[] = {
  { PRS_THEME_FRAME_WIN, IDR_THEME_FRAME_WIN,
    "theme_frame" },
  { PRS_THEME_FRAME_INACTIVE_WIN, IDR_THEME_FRAME_INACTIVE_WIN,
    "theme_frame_inactive" },
  { PRS_THEME_FRAME_INCOGNITO_WIN, IDR_THEME_FRAME_INCOGNITO_WIN,
    "theme_frame_incognito" },
  { PRS_THEME_FRAME_INCOGNITO_INACTIVE_WIN,
    IDR_THEME_FRAME_INCOGNITO_INACTIVE_WIN,
    "theme_frame_incognito_inactive" },
  { PRS_THEME_TOOLBAR_WIN, IDR_THEME_TOOLBAR_WIN,
    "theme_toolbar" },
  { PRS_THEME_TAB_BACKGROUND_WIN, IDR_THEME_TAB_BACKGROUND_WIN,
    "theme_tab_background" },
  { PRS_THEME_TAB_BACKGROUND_INCOGNITO_WIN,
    IDR_THEME_TAB_BACKGROUND_INCOGNITO_WIN,
    "theme_tab_background_incognito" },
};
const size_t kPersistingImagesWinDesktopAuraLength =
    arraysize(kPersistingImagesWinDesktopAura);
#endif

int GetPersistentIDByNameHelper(const std::string& key,
                                const PersistingImagesTable* image_table,
                                size_t image_table_size) {
  for (size_t i = 0; i < image_table_size; ++i) {
    if (image_table[i].key != NULL &&
        base::strcasecmp(key.c_str(), image_table[i].key) == 0) {
      return image_table[i].persistent_id;
    }
  }
  return -1;
}

int GetPersistentIDByName(const std::string& key) {
  return GetPersistentIDByNameHelper(key,
                                     kPersistingImages,
                                     kPersistingImagesLength);
}

int GetPersistentIDByIDR(int idr) {
  static std::map<int,int>* lookup_table = new std::map<int,int>();
  if (lookup_table->empty()) {
    for (size_t i = 0; i < kPersistingImagesLength; ++i) {
      int idr = kPersistingImages[i].idr_id;
      int prs_id = kPersistingImages[i].persistent_id;
      (*lookup_table)[idr] = prs_id;
    }
#if defined(OS_WIN) && defined(USE_AURA)
    for (size_t i = 0; i < kPersistingImagesWinDesktopAuraLength; ++i) {
      int idr = kPersistingImagesWinDesktopAura[i].idr_id;
      int prs_id = kPersistingImagesWinDesktopAura[i].persistent_id;
      (*lookup_table)[idr] = prs_id;
    }
#endif
  }
  std::map<int,int>::iterator it = lookup_table->find(idr);
  return (it == lookup_table->end()) ? -1 : it->second;
}

// Returns true if the scales in |input| match those in |expected|.
// The order must match as the index is used in determining the raw id.
bool InputScalesValid(const base::StringPiece& input,
                      const std::vector<ui::ScaleFactor>& expected) {
  size_t scales_size = static_cast<size_t>(input.size() / sizeof(float));
  if (scales_size != expected.size())
    return false;
  scoped_array<float> scales(new float[scales_size]);
  // Do a memcpy to avoid misaligned memory access.
  memcpy(scales.get(), input.data(), input.size());
  for (size_t index = 0; index < scales_size; ++index) {
    if (scales[index] != ui::GetScaleFactorScale(expected[index]))
      return false;
  }
  return true;
}

// Returns |scale_factors| as a string to be written to disk.
std::string GetScaleFactorsAsString(
    const std::vector<ui::ScaleFactor>& scale_factors) {
  scoped_array<float> scales(new float[scale_factors.size()]);
  for (size_t i = 0; i < scale_factors.size(); ++i)
    scales[i] = ui::GetScaleFactorScale(scale_factors[i]);
  std::string out_string = std::string(
      reinterpret_cast<const char*>(scales.get()),
      scale_factors.size() * sizeof(float));
  return out_string;
}

struct StringToIntTable {
  const char* key;
  ThemeProperties::OverwritableByUserThemeProperty id;
};

// Strings used by themes to identify tints in the JSON.
StringToIntTable kTintTable[] = {
  { "buttons", ThemeProperties::TINT_BUTTONS },
  { "frame", ThemeProperties::TINT_FRAME },
  { "frame_inactive", ThemeProperties::TINT_FRAME_INACTIVE },
  { "frame_incognito", ThemeProperties::TINT_FRAME_INCOGNITO },
  { "frame_incognito_inactive",
    ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE },
  { "background_tab", ThemeProperties::TINT_BACKGROUND_TAB },
};
const size_t kTintTableLength = arraysize(kTintTable);

// Strings used by themes to identify colors in the JSON.
StringToIntTable kColorTable[] = {
  { "frame", ThemeProperties::COLOR_FRAME },
  { "frame_inactive", ThemeProperties::COLOR_FRAME_INACTIVE },
  { "frame_incognito", ThemeProperties::COLOR_FRAME_INCOGNITO },
  { "frame_incognito_inactive",
    ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE },
  { "toolbar", ThemeProperties::COLOR_TOOLBAR },
  { "tab_text", ThemeProperties::COLOR_TAB_TEXT },
  { "tab_background_text", ThemeProperties::COLOR_BACKGROUND_TAB_TEXT },
  { "bookmark_text", ThemeProperties::COLOR_BOOKMARK_TEXT },
  { "ntp_background", ThemeProperties::COLOR_NTP_BACKGROUND },
  { "ntp_text", ThemeProperties::COLOR_NTP_TEXT },
  { "ntp_link", ThemeProperties::COLOR_NTP_LINK },
  { "ntp_link_underline", ThemeProperties::COLOR_NTP_LINK_UNDERLINE },
  { "ntp_header", ThemeProperties::COLOR_NTP_HEADER },
  { "ntp_section", ThemeProperties::COLOR_NTP_SECTION },
  { "ntp_section_text", ThemeProperties::COLOR_NTP_SECTION_TEXT },
  { "ntp_section_link", ThemeProperties::COLOR_NTP_SECTION_LINK },
  { "ntp_section_link_underline",
    ThemeProperties::COLOR_NTP_SECTION_LINK_UNDERLINE },
  { "button_background", ThemeProperties::COLOR_BUTTON_BACKGROUND },
};
const size_t kColorTableLength = arraysize(kColorTable);

// Strings used by themes to identify display properties keys in JSON.
StringToIntTable kDisplayProperties[] = {
  { "ntp_background_alignment",
    ThemeProperties::NTP_BACKGROUND_ALIGNMENT },
  { "ntp_background_repeat", ThemeProperties::NTP_BACKGROUND_TILING },
  { "ntp_logo_alternate", ThemeProperties::NTP_LOGO_ALTERNATE },
};
const size_t kDisplayPropertiesSize = arraysize(kDisplayProperties);

int GetIntForString(const std::string& key,
                    StringToIntTable* table,
                    size_t table_length) {
  for (size_t i = 0; i < table_length; ++i) {
    if (base::strcasecmp(key.c_str(), table[i].key) == 0) {
      return table[i].id;
    }
  }

  return -1;
}

struct IntToIntTable {
  int key;
  int value;
};

// Mapping used in GenerateFrameImages() to associate frame images with the
// tint ID that should maybe be applied to it.
IntToIntTable kFrameTintMap[] = {
  { PRS_THEME_FRAME, ThemeProperties::TINT_FRAME },
  { PRS_THEME_FRAME_INACTIVE, ThemeProperties::TINT_FRAME_INACTIVE },
  { PRS_THEME_FRAME_OVERLAY, ThemeProperties::TINT_FRAME },
  { PRS_THEME_FRAME_OVERLAY_INACTIVE,
    ThemeProperties::TINT_FRAME_INACTIVE },
  { PRS_THEME_FRAME_INCOGNITO, ThemeProperties::TINT_FRAME_INCOGNITO },
  { PRS_THEME_FRAME_INCOGNITO_INACTIVE,
    ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE },
#if defined(OS_WIN) && defined(USE_AURA)
  { PRS_THEME_FRAME_WIN, ThemeProperties::TINT_FRAME },
  { PRS_THEME_FRAME_INACTIVE_WIN, ThemeProperties::TINT_FRAME_INACTIVE },
  { PRS_THEME_FRAME_INCOGNITO_WIN, ThemeProperties::TINT_FRAME_INCOGNITO },
  { PRS_THEME_FRAME_INCOGNITO_INACTIVE_WIN,
    ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE },
#endif
};

// Mapping used in GenerateTabBackgroundImages() to associate what frame image
// goes with which tab background.
IntToIntTable kTabBackgroundMap[] = {
  { PRS_THEME_TAB_BACKGROUND, PRS_THEME_FRAME },
  { PRS_THEME_TAB_BACKGROUND_INCOGNITO, PRS_THEME_FRAME_INCOGNITO },
#if defined(OS_WIN) && defined(USE_AURA)
  { PRS_THEME_TAB_BACKGROUND_WIN, PRS_THEME_FRAME_WIN },
  { PRS_THEME_TAB_BACKGROUND_INCOGNITO_WIN, PRS_THEME_FRAME_INCOGNITO_WIN },
#endif
};


// A list of images that don't need tinting or any other modification and can
// be byte-copied directly into the finished DataPack. This should contain the
// persistent IDs for all themeable image IDs that aren't in kFrameTintMap or
// kTabBackgroundMap.
const int kPreloadIDs[] = {
  PRS_THEME_TOOLBAR,
  PRS_THEME_NTP_BACKGROUND,
  PRS_THEME_BUTTON_BACKGROUND,
  PRS_THEME_NTP_ATTRIBUTION,
  PRS_THEME_WINDOW_CONTROL_BACKGROUND,
#if defined(OS_WIN) && defined(USE_AURA)
  PRS_THEME_TOOLBAR_WIN,
#endif
};

// Returns a piece of memory with the contents of the file |path|.
base::RefCountedMemory* ReadFileData(const base::FilePath& path) {
  if (!path.empty()) {
    net::FileStream file(NULL);
    int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
    if (file.OpenSync(path, flags) == net::OK) {
      int64 avail = file.Available();
      if (avail > 0 && avail < INT_MAX) {
        size_t size = static_cast<size_t>(avail);
        std::vector<unsigned char> raw_data;
        raw_data.resize(size);
        char* data = reinterpret_cast<char*>(&(raw_data.front()));
        if (file.ReadUntilComplete(data, size) == avail)
          return base::RefCountedBytes::TakeVector(&raw_data);
      }
    }
  }

  return NULL;
}

// Shifts an image's HSL values. The caller is responsible for deleting
// the returned image.
gfx::Image* CreateHSLShiftedImage(const gfx::Image& image,
                                  const color_utils::HSL& hsl_shift) {
  const gfx::ImageSkia* src_image = image.ToImageSkia();
  return new gfx::Image(gfx::ImageSkiaOperations::CreateHSLShiftedImage(
      *src_image, hsl_shift));
}

// A ImageSkiaSource that scales 100P image to the target scale factor
// if the ImageSkiaRep for the target scale factor isn't available.
class ThemeImageSource: public gfx::ImageSkiaSource {
 public:
  explicit ThemeImageSource(const gfx::ImageSkia& source) : source_(source) {
  }
  virtual ~ThemeImageSource() {}

  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE {
    if (source_.HasRepresentation(scale_factor))
      return source_.GetRepresentation(scale_factor);
    const gfx::ImageSkiaRep& rep_100p =
        source_.GetRepresentation(ui::SCALE_FACTOR_100P);
    float scale = ui::GetScaleFactorScale(scale_factor);
    gfx::Size size(rep_100p.GetWidth() * scale, rep_100p.GetHeight() * scale);
    SkBitmap resized_bitmap;
    resized_bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(),
                             size.height());
    if (!resized_bitmap.allocPixels())
      SK_CRASH();
    resized_bitmap.eraseARGB(0, 0, 0, 0);
    SkCanvas canvas(resized_bitmap);
    SkRect resized_bounds = RectToSkRect(gfx::Rect(size));
    // Note(oshima): The following scaling code doesn't work with
    // a mask image.
    canvas.drawBitmapRect(rep_100p.sk_bitmap(), NULL, resized_bounds);
    return gfx::ImageSkiaRep(resized_bitmap, scale_factor);
  }

 private:
  const gfx::ImageSkia source_;

  DISALLOW_COPY_AND_ASSIGN(ThemeImageSource);
};

class TabBackgroundImageSource: public gfx::CanvasImageSource {
 public:
  TabBackgroundImageSource(const gfx::ImageSkia& image_to_tint,
                           const gfx::ImageSkia& overlay,
                           const color_utils::HSL& hsl_shift,
                           int vertical_offset)
      : gfx::CanvasImageSource(image_to_tint.size(), false),
        image_to_tint_(image_to_tint),
        overlay_(overlay),
        hsl_shift_(hsl_shift),
        vertical_offset_(vertical_offset) {
  }

  virtual ~TabBackgroundImageSource() {
  }

  // Overridden from CanvasImageSource:
  virtual void Draw(gfx::Canvas* canvas) OVERRIDE {
    gfx::ImageSkia bg_tint =
        gfx::ImageSkiaOperations::CreateHSLShiftedImage(image_to_tint_,
            hsl_shift_);
    canvas->TileImageInt(bg_tint, 0, vertical_offset_, 0, 0,
        size().width(), size().height());

    // If they've provided a custom image, overlay it.
    if (!overlay_.isNull()) {
      canvas->TileImageInt(overlay_, 0, 0, size().width(),
                           overlay_.height());
    }
  }

 private:
  const gfx::ImageSkia image_to_tint_;
  const gfx::ImageSkia overlay_;
  const color_utils::HSL hsl_shift_;
  const int vertical_offset_;

  DISALLOW_COPY_AND_ASSIGN(TabBackgroundImageSource);
};

}  // namespace

BrowserThemePack::~BrowserThemePack() {
  if (!data_pack_.get()) {
    delete header_;
    delete [] tints_;
    delete [] colors_;
    delete [] display_properties_;
    delete [] source_images_;
  }

  STLDeleteValues(&images_on_ui_thread_);
  STLDeleteValues(&images_on_file_thread_);
}

// static
scoped_refptr<BrowserThemePack> BrowserThemePack::BuildFromExtension(
    const Extension* extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension);
  DCHECK(extension->is_theme());

  scoped_refptr<BrowserThemePack> pack(new BrowserThemePack);
  pack->BuildHeader(extension);
  pack->BuildTintsFromJSON(extensions::ThemeInfo::GetThemeTints(extension));
  pack->BuildColorsFromJSON(extensions::ThemeInfo::GetThemeColors(extension));
  pack->BuildDisplayPropertiesFromJSON(
      extensions::ThemeInfo::GetThemeDisplayProperties(extension));

  // Builds the images. (Image building is dependent on tints).
  FilePathMap file_paths;
  pack->ParseImageNamesFromJSON(
      extensions::ThemeInfo::GetThemeImages(extension),
      extension->path(),
      &file_paths);
  pack->BuildSourceImagesArray(file_paths);

  if (!pack->LoadRawBitmapsTo(file_paths, &pack->images_on_ui_thread_))
    return NULL;

  pack->CopyImagesTo(pack->images_on_ui_thread_, &pack->images_on_file_thread_);

  pack->CreateImages(&pack->images_on_ui_thread_);
  pack->CreateImages(&pack->images_on_file_thread_);

  // Make sure the |images_on_file_thread_| has bitmaps for supported
  // scale factors before passing to FILE thread.
  for (ImageCache::iterator it = pack->images_on_file_thread_.begin();
       it != pack->images_on_file_thread_.end(); ++it) {
    gfx::ImageSkia* image_skia =
        const_cast<gfx::ImageSkia*>(it->second->ToImageSkia());
    image_skia->MakeThreadSafe();
  }

  // The BrowserThemePack is now in a consistent state.
  return pack;
}

// static
scoped_refptr<BrowserThemePack> BrowserThemePack::BuildFromDataPack(
    const base::FilePath& path, const std::string& expected_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Allow IO on UI thread due to deep-seated theme design issues.
  // (see http://crbug.com/80206)
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  scoped_refptr<BrowserThemePack> pack(new BrowserThemePack);
  // Scale factor parameter is moot as data pack has image resources for all
  // supported scale factors.
  pack->data_pack_.reset(
      new ui::DataPack(ui::SCALE_FACTOR_NONE));

  if (!pack->data_pack_->LoadFromPath(path)) {
    LOG(ERROR) << "Failed to load theme data pack.";
    return NULL;
  }

  base::StringPiece pointer;
  if (!pack->data_pack_->GetStringPiece(kHeaderID, &pointer))
    return NULL;
  pack->header_ = reinterpret_cast<BrowserThemePackHeader*>(const_cast<char*>(
      pointer.data()));

  if (pack->header_->version != kThemePackVersion) {
    DLOG(ERROR) << "BuildFromDataPack failure! Version mismatch!";
    return NULL;
  }
  // TODO(erg): Check endianess once DataPack works on the other endian.
  std::string theme_id(reinterpret_cast<char*>(pack->header_->theme_id),
                       extensions::id_util::kIdSize);
  std::string truncated_id =
      expected_id.substr(0, extensions::id_util::kIdSize);
  if (theme_id != truncated_id) {
    DLOG(ERROR) << "Wrong id: " << theme_id << " vs " << expected_id;
    return NULL;
  }

  if (!pack->data_pack_->GetStringPiece(kTintsID, &pointer))
    return NULL;
  pack->tints_ = reinterpret_cast<TintEntry*>(const_cast<char*>(
      pointer.data()));

  if (!pack->data_pack_->GetStringPiece(kColorsID, &pointer))
    return NULL;
  pack->colors_ =
      reinterpret_cast<ColorPair*>(const_cast<char*>(pointer.data()));

  if (!pack->data_pack_->GetStringPiece(kDisplayPropertiesID, &pointer))
    return NULL;
  pack->display_properties_ = reinterpret_cast<DisplayPropertyPair*>(
      const_cast<char*>(pointer.data()));

  if (!pack->data_pack_->GetStringPiece(kSourceImagesID, &pointer))
    return NULL;
  pack->source_images_ = reinterpret_cast<int*>(
      const_cast<char*>(pointer.data()));

  if (!pack->data_pack_->GetStringPiece(kScaleFactorsID, &pointer))
    return NULL;

  if (!InputScalesValid(pointer, pack->scale_factors_)) {
    DLOG(ERROR) << "BuildFromDataPack failure! The pack scale factors differ "
                << "from those supported by platform.";
  }
  return pack;
}

bool BrowserThemePack::WriteToDisk(const base::FilePath& path) const {
  // Add resources for each of the property arrays.
  RawDataForWriting resources;
  resources[kHeaderID] = base::StringPiece(
      reinterpret_cast<const char*>(header_), sizeof(BrowserThemePackHeader));
  resources[kTintsID] = base::StringPiece(
      reinterpret_cast<const char*>(tints_),
      sizeof(TintEntry[kTintTableLength]));
  resources[kColorsID] = base::StringPiece(
      reinterpret_cast<const char*>(colors_),
      sizeof(ColorPair[kColorTableLength]));
  resources[kDisplayPropertiesID] = base::StringPiece(
      reinterpret_cast<const char*>(display_properties_),
      sizeof(DisplayPropertyPair[kDisplayPropertiesSize]));

  int source_count = 1;
  int* end = source_images_;
  for (; *end != -1 ; end++)
    source_count++;
  resources[kSourceImagesID] = base::StringPiece(
      reinterpret_cast<const char*>(source_images_),
      source_count * sizeof(*source_images_));

  // Store results of GetScaleFactorsAsString() in std::string as
  // base::StringPiece does not copy data in constructor.
  std::string scale_factors_string = GetScaleFactorsAsString(scale_factors_);
  resources[kScaleFactorsID] = scale_factors_string;

  AddRawImagesTo(image_memory_, &resources);

  RawImages reencoded_images;
  RepackImages(images_on_file_thread_, &reencoded_images);
  AddRawImagesTo(reencoded_images, &resources);

  return ui::DataPack::WritePack(path, resources, ui::DataPack::BINARY);
}

bool BrowserThemePack::GetTint(int id, color_utils::HSL* hsl) const {
  if (tints_) {
    for (size_t i = 0; i < kTintTableLength; ++i) {
      if (tints_[i].id == id) {
        hsl->h = tints_[i].h;
        hsl->s = tints_[i].s;
        hsl->l = tints_[i].l;
        return true;
      }
    }
  }

  return false;
}

bool BrowserThemePack::GetColor(int id, SkColor* color) const {
  if (colors_) {
    for (size_t i = 0; i < kColorTableLength; ++i) {
      if (colors_[i].id == id) {
        *color = colors_[i].color;
        return true;
      }
    }
  }

  return false;
}

bool BrowserThemePack::GetDisplayProperty(int id, int* result) const {
  if (display_properties_) {
    for (size_t i = 0; i < kDisplayPropertiesSize; ++i) {
      if (display_properties_[i].id == id) {
        *result = display_properties_[i].property;
        return true;
      }
    }
  }

  return false;
}

const gfx::Image* BrowserThemePack::GetImageNamed(int idr_id) const {
  int prs_id = GetPersistentIDByIDR(idr_id);
  if (prs_id == -1)
    return NULL;

  // Check if the image is cached.
  ImageCache::const_iterator image_iter = images_on_ui_thread_.find(prs_id);
  if (image_iter != images_on_ui_thread_.end())
    return image_iter->second;

  // TODO(pkotwicz): Do something better than loading the bitmaps
  // for all the scale factors associated with |idr_id|.
  gfx::ImageSkia source_image_skia;
  for (size_t i = 0; i < scale_factors_.size(); ++i) {
    scoped_refptr<base::RefCountedMemory> memory =
        GetRawData(idr_id, scale_factors_[i]);
    if (memory.get()) {
      // Decode the PNG.
      SkBitmap bitmap;
      if (!gfx::PNGCodec::Decode(memory->front(), memory->size(),
                                 &bitmap)) {
        NOTREACHED() << "Unable to decode theme image resource " << idr_id
                     << " from saved DataPack.";
        continue;
      }
      source_image_skia.AddRepresentation(
          gfx::ImageSkiaRep(bitmap, scale_factors_[i]));
    }
  }

  if (!source_image_skia.isNull()) {
    ThemeImageSource* source = new ThemeImageSource(source_image_skia);
    gfx::ImageSkia image_skia(source, source_image_skia.size());
    gfx::Image* ret = new gfx::Image(image_skia);
    images_on_ui_thread_[prs_id] = ret;
    return ret;
  }
  return NULL;
}

base::RefCountedMemory* BrowserThemePack::GetRawData(
    int idr_id,
    ui::ScaleFactor scale_factor) const {
  base::RefCountedMemory* memory = NULL;
  int prs_id = GetPersistentIDByIDR(idr_id);
  int raw_id = GetRawIDByPersistentID(prs_id, scale_factor);

  if (raw_id != -1) {
    if (data_pack_.get()) {
      memory = data_pack_->GetStaticMemory(raw_id);
    } else {
      RawImages::const_iterator it = image_memory_.find(raw_id);
      if (it != image_memory_.end()) {
        memory = it->second;
      }
    }
  }

  return memory;
}

// static
void BrowserThemePack::GetThemeableImageIDRs(std::set<int>* result) {
  if (!result)
    return;

  result->clear();
  for (size_t i = 0; i < kPersistingImagesLength; ++i)
    result->insert(kPersistingImages[i].idr_id);

#if defined(OS_WIN) && defined(USE_AURA)
  for (size_t i = 0; i < kPersistingImagesWinDesktopAuraLength; ++i)
    result->insert(kPersistingImagesWinDesktopAura[i].idr_id);
#endif
}

bool BrowserThemePack::HasCustomImage(int idr_id) const {
  int prs_id = GetPersistentIDByIDR(idr_id);
  if (prs_id == -1)
    return false;

  int* img = source_images_;
  for (; *img != -1; ++img) {
    if (*img == prs_id)
      return true;
  }

  return false;
}

// private:

BrowserThemePack::BrowserThemePack()
    : header_(NULL),
      tints_(NULL),
      colors_(NULL),
      display_properties_(NULL),
      source_images_(NULL) {
  scale_factors_ = ui::GetSupportedScaleFactors();
}

void BrowserThemePack::BuildHeader(const Extension* extension) {
  header_ = new BrowserThemePackHeader;
  header_->version = kThemePackVersion;

  // TODO(erg): Need to make this endian safe on other computers. Prerequisite
  // is that ui::DataPack removes this same check.
#if defined(__BYTE_ORDER)
  // Linux check
  COMPILE_ASSERT(__BYTE_ORDER == __LITTLE_ENDIAN,
                 datapack_assumes_little_endian);
#elif defined(__BIG_ENDIAN__)
  // Mac check
  #error DataPack assumes little endian
#endif
  header_->little_endian = 1;

  const std::string& id = extension->id();
  memcpy(header_->theme_id, id.c_str(), extensions::id_util::kIdSize);
}

void BrowserThemePack::BuildTintsFromJSON(DictionaryValue* tints_value) {
  tints_ = new TintEntry[kTintTableLength];
  for (size_t i = 0; i < kTintTableLength; ++i) {
    tints_[i].id = -1;
    tints_[i].h = -1;
    tints_[i].s = -1;
    tints_[i].l = -1;
  }

  if (!tints_value)
    return;

  // Parse the incoming data from |tints_value| into an intermediary structure.
  std::map<int, color_utils::HSL> temp_tints;
  for (DictionaryValue::Iterator iter(*tints_value); !iter.IsAtEnd();
       iter.Advance()) {
    const ListValue* tint_list;
    if (iter.value().GetAsList(&tint_list) &&
        (tint_list->GetSize() == 3)) {
      color_utils::HSL hsl = { -1, -1, -1 };

      if (tint_list->GetDouble(0, &hsl.h) &&
          tint_list->GetDouble(1, &hsl.s) &&
          tint_list->GetDouble(2, &hsl.l)) {
        int id = GetIntForString(iter.key(), kTintTable, kTintTableLength);
        if (id != -1) {
          temp_tints[id] = hsl;
        }
      }
    }
  }

  // Copy data from the intermediary data structure to the array.
  size_t count = 0;
  for (std::map<int, color_utils::HSL>::const_iterator it =
           temp_tints.begin();
       it != temp_tints.end() && count < kTintTableLength;
       ++it, ++count) {
    tints_[count].id = it->first;
    tints_[count].h = it->second.h;
    tints_[count].s = it->second.s;
    tints_[count].l = it->second.l;
  }
}

void BrowserThemePack::BuildColorsFromJSON(DictionaryValue* colors_value) {
  colors_ = new ColorPair[kColorTableLength];
  for (size_t i = 0; i < kColorTableLength; ++i) {
    colors_[i].id = -1;
    colors_[i].color = SkColorSetRGB(0, 0, 0);
  }

  std::map<int, SkColor> temp_colors;
  if (colors_value)
    ReadColorsFromJSON(colors_value, &temp_colors);
  GenerateMissingColors(&temp_colors);

  // Copy data from the intermediary data structure to the array.
  size_t count = 0;
  for (std::map<int, SkColor>::const_iterator it = temp_colors.begin();
       it != temp_colors.end() && count < kColorTableLength; ++it, ++count) {
    colors_[count].id = it->first;
    colors_[count].color = it->second;
  }
}

void BrowserThemePack::ReadColorsFromJSON(
    DictionaryValue* colors_value,
    std::map<int, SkColor>* temp_colors) {
  // Parse the incoming data from |colors_value| into an intermediary structure.
  for (DictionaryValue::Iterator iter(*colors_value); !iter.IsAtEnd();
       iter.Advance()) {
    const ListValue* color_list;
    if (iter.value().GetAsList(&color_list) &&
        ((color_list->GetSize() == 3) || (color_list->GetSize() == 4))) {
      SkColor color = SK_ColorWHITE;
      int r, g, b;
      if (color_list->GetInteger(0, &r) &&
          color_list->GetInteger(1, &g) &&
          color_list->GetInteger(2, &b)) {
        if (color_list->GetSize() == 4) {
          double alpha;
          int alpha_int;
          if (color_list->GetDouble(3, &alpha)) {
            color = SkColorSetARGB(static_cast<int>(alpha * 255), r, g, b);
          } else if (color_list->GetInteger(3, &alpha_int) &&
                     (alpha_int == 0 || alpha_int == 1)) {
            color = SkColorSetARGB(alpha_int ? 255 : 0, r, g, b);
          } else {
            // Invalid entry for part 4.
            continue;
          }
        } else {
          color = SkColorSetRGB(r, g, b);
        }

        int id = GetIntForString(iter.key(), kColorTable, kColorTableLength);
        if (id != -1) {
          (*temp_colors)[id] = color;
        }
      }
    }
  }
}

void BrowserThemePack::GenerateMissingColors(
    std::map<int, SkColor>* colors) {
  // Generate link colors, if missing. (See GetColor()).
  if (!colors->count(ThemeProperties::COLOR_NTP_HEADER) &&
      colors->count(ThemeProperties::COLOR_NTP_SECTION)) {
    (*colors)[ThemeProperties::COLOR_NTP_HEADER] =
        (*colors)[ThemeProperties::COLOR_NTP_SECTION];
  }

  if (!colors->count(ThemeProperties::COLOR_NTP_SECTION_LINK_UNDERLINE) &&
      colors->count(ThemeProperties::COLOR_NTP_SECTION_LINK)) {
    SkColor color_section_link =
        (*colors)[ThemeProperties::COLOR_NTP_SECTION_LINK];
    (*colors)[ThemeProperties::COLOR_NTP_SECTION_LINK_UNDERLINE] =
        SkColorSetA(color_section_link, SkColorGetA(color_section_link) / 3);
  }

  if (!colors->count(ThemeProperties::COLOR_NTP_LINK_UNDERLINE) &&
      colors->count(ThemeProperties::COLOR_NTP_LINK)) {
    SkColor color_link = (*colors)[ThemeProperties::COLOR_NTP_LINK];
    (*colors)[ThemeProperties::COLOR_NTP_LINK_UNDERLINE] =
        SkColorSetA(color_link, SkColorGetA(color_link) / 3);
  }

  // Generate frame colors, if missing. (See GenerateFrameColors()).
  SkColor frame;
  std::map<int, SkColor>::const_iterator it =
      colors->find(ThemeProperties::COLOR_FRAME);
  if (it != colors->end()) {
    frame = it->second;
  } else {
    frame = ThemeProperties::GetDefaultColor(
        ThemeProperties::COLOR_FRAME);
  }

  if (!colors->count(ThemeProperties::COLOR_FRAME)) {
    (*colors)[ThemeProperties::COLOR_FRAME] =
        HSLShift(frame, GetTintInternal(ThemeProperties::TINT_FRAME));
  }
  if (!colors->count(ThemeProperties::COLOR_FRAME_INACTIVE)) {
    (*colors)[ThemeProperties::COLOR_FRAME_INACTIVE] =
        HSLShift(frame, GetTintInternal(
            ThemeProperties::TINT_FRAME_INACTIVE));
  }
  if (!colors->count(ThemeProperties::COLOR_FRAME_INCOGNITO)) {
    (*colors)[ThemeProperties::COLOR_FRAME_INCOGNITO] =
        HSLShift(frame, GetTintInternal(
            ThemeProperties::TINT_FRAME_INCOGNITO));
  }
  if (!colors->count(ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE)) {
    (*colors)[ThemeProperties::COLOR_FRAME_INCOGNITO_INACTIVE] =
        HSLShift(frame, GetTintInternal(
            ThemeProperties::TINT_FRAME_INCOGNITO_INACTIVE));
  }
}

void BrowserThemePack::BuildDisplayPropertiesFromJSON(
    DictionaryValue* display_properties_value) {
  display_properties_ = new DisplayPropertyPair[kDisplayPropertiesSize];
  for (size_t i = 0; i < kDisplayPropertiesSize; ++i) {
    display_properties_[i].id = -1;
    display_properties_[i].property = 0;
  }

  if (!display_properties_value)
    return;

  std::map<int, int> temp_properties;
  for (DictionaryValue::Iterator iter(*display_properties_value);
       !iter.IsAtEnd(); iter.Advance()) {
    int property_id = GetIntForString(iter.key(), kDisplayProperties,
                                      kDisplayPropertiesSize);
    switch (property_id) {
      case ThemeProperties::NTP_BACKGROUND_ALIGNMENT: {
        std::string val;
        if (iter.value().GetAsString(&val)) {
          temp_properties[ThemeProperties::NTP_BACKGROUND_ALIGNMENT] =
              ThemeProperties::StringToAlignment(val);
        }
        break;
      }
      case ThemeProperties::NTP_BACKGROUND_TILING: {
        std::string val;
        if (iter.value().GetAsString(&val)) {
          temp_properties[ThemeProperties::NTP_BACKGROUND_TILING] =
              ThemeProperties::StringToTiling(val);
        }
        break;
      }
      case ThemeProperties::NTP_LOGO_ALTERNATE: {
        int val = 0;
        if (iter.value().GetAsInteger(&val))
          temp_properties[ThemeProperties::NTP_LOGO_ALTERNATE] = val;
        break;
      }
    }
  }

  // Copy data from the intermediary data structure to the array.
  size_t count = 0;
  for (std::map<int, int>::const_iterator it = temp_properties.begin();
       it != temp_properties.end() && count < kDisplayPropertiesSize;
       ++it, ++count) {
    display_properties_[count].id = it->first;
    display_properties_[count].property = it->second;
  }
}

void BrowserThemePack::ParseImageNamesFromJSON(
    DictionaryValue* images_value,
    const base::FilePath& images_path,
    FilePathMap* file_paths) const {
  if (!images_value)
    return;

  for (DictionaryValue::Iterator iter(*images_value); !iter.IsAtEnd();
       iter.Advance()) {
    std::string val;
    if (iter.value().GetAsString(&val)) {
      int id = GetPersistentIDByName(iter.key());
      if (id != -1)
        (*file_paths)[id] = images_path.AppendASCII(val);
#if defined(OS_WIN) && defined(USE_AURA)
      id = GetPersistentIDByNameHelper(iter.key(),
                                       kPersistingImagesWinDesktopAura,
                                       kPersistingImagesWinDesktopAuraLength);
      if (id != -1)
        (*file_paths)[id] = images_path.AppendASCII(val);
#endif
    }
  }
}

void BrowserThemePack::BuildSourceImagesArray(const FilePathMap& file_paths) {
  std::vector<int> ids;
  for (FilePathMap::const_iterator it = file_paths.begin();
       it != file_paths.end(); ++it) {
    ids.push_back(it->first);
  }

  source_images_ = new int[ids.size() + 1];
  std::copy(ids.begin(), ids.end(), source_images_);
  source_images_[ids.size()] = -1;
}

bool BrowserThemePack::LoadRawBitmapsTo(
    const FilePathMap& file_paths,
    ImageCache* image_cache) {
  // Themes should be loaded on the file thread, not the UI thread.
  // http://crbug.com/61838
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  for (FilePathMap::const_iterator it = file_paths.begin();
       it != file_paths.end(); ++it) {
    scoped_refptr<base::RefCountedMemory> raw_data(ReadFileData(it->second));
    if (!raw_data.get()) {
      LOG(ERROR) << "Could not load theme image";
      return false;
    }

    int prs_id = it->first;

    // Some images need to go directly into |image_memory_|. No modification is
    // necessary or desirable.
    bool is_copyable = false;
    for (size_t i = 0; i < arraysize(kPreloadIDs); ++i) {
      if (kPreloadIDs[i] == prs_id) {
        is_copyable = true;
        break;
      }
    }

    if (is_copyable) {
      int raw_id = GetRawIDByPersistentID(prs_id, ui::SCALE_FACTOR_100P);
      image_memory_[raw_id] = raw_data;
    } else if (raw_data.get() && raw_data->size()) {
      // Decode the PNG.
      SkBitmap bitmap;
      if (gfx::PNGCodec::Decode(raw_data->front(), raw_data->size(),
                                &bitmap)) {
        (*image_cache)[prs_id] =
            new gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
      } else {
        NOTREACHED() << "Unable to decode theme image resource " << it->first;
      }
    }
  }

  return true;
}

void BrowserThemePack::CreateImages(ImageCache* images) const {
  CreateFrameImages(images);
  CreateTintedButtons(GetTintInternal(ThemeProperties::TINT_BUTTONS), images);
  CreateTabBackgroundImages(images);
}

void BrowserThemePack::CreateFrameImages(ImageCache* images) const {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Create all the output images in a separate cache and move them back into
  // the input images because there can be name collisions.
  ImageCache temp_output;

  for (size_t i = 0; i < arraysize(kFrameTintMap); ++i) {
    int prs_id = kFrameTintMap[i].key;
    const gfx::Image* frame = NULL;
    // If there's no frame image provided for the specified id, then load
    // the default provided frame. If that's not provided, skip this whole
    // thing and just use the default images.
    int prs_base_id = 0;

#if defined(OS_WIN) && defined(USE_AURA)
    if (prs_id == PRS_THEME_FRAME_INCOGNITO_INACTIVE_WIN) {
      prs_base_id = images->count(PRS_THEME_FRAME_INCOGNITO_WIN) ?
                    PRS_THEME_FRAME_INCOGNITO_WIN : PRS_THEME_FRAME_WIN;
    } else if (prs_id == PRS_THEME_FRAME_INACTIVE_WIN) {
      prs_base_id = PRS_THEME_FRAME_WIN;
    } else if (prs_id == PRS_THEME_FRAME_INCOGNITO_WIN &&
                !images->count(PRS_THEME_FRAME_INCOGNITO_WIN)) {
      prs_base_id = PRS_THEME_FRAME_WIN;
    }
#endif
    if (!prs_base_id) {
      if (prs_id == PRS_THEME_FRAME_INCOGNITO_INACTIVE) {
        prs_base_id = images->count(PRS_THEME_FRAME_INCOGNITO) ?
                      PRS_THEME_FRAME_INCOGNITO : PRS_THEME_FRAME;
      } else if (prs_id == PRS_THEME_FRAME_OVERLAY_INACTIVE) {
        prs_base_id = PRS_THEME_FRAME_OVERLAY;
      } else if (prs_id == PRS_THEME_FRAME_INACTIVE) {
        prs_base_id = PRS_THEME_FRAME;
      } else if (prs_id == PRS_THEME_FRAME_INCOGNITO &&
                 !images->count(PRS_THEME_FRAME_INCOGNITO)) {
        prs_base_id = PRS_THEME_FRAME;
      } else {
        prs_base_id = prs_id;
      }
    }
    if (images->count(prs_id)) {
      frame = (*images)[prs_id];
    } else if (prs_base_id != prs_id && images->count(prs_base_id)) {
      frame = (*images)[prs_base_id];
    } else if (prs_base_id == PRS_THEME_FRAME_OVERLAY) {
#if defined(OS_WIN) && defined(USE_AURA)
      if (images->count(PRS_THEME_FRAME_WIN)) {
#else
      if (images->count(PRS_THEME_FRAME)) {
#endif
        // If there is no theme overlay, don't tint the default frame,
        // because it will overwrite the custom frame image when we cache and
        // reload from disk.
        frame = NULL;
      }
    } else {
      // If the theme doesn't specify an image, then apply the tint to
      // the default frame.
      frame = &rb.GetImageNamed(IDR_THEME_FRAME);
#if defined(OS_WIN) && defined(USE_AURA)
      if (prs_id >= PRS_THEME_FRAME_WIN &&
          prs_id <= PRS_THEME_FRAME_INCOGNITO_INACTIVE_WIN) {
        frame = &rb.GetImageNamed(IDR_THEME_FRAME_WIN);
      }
#endif
    }
    if (frame) {
      temp_output[prs_id] = CreateHSLShiftedImage(
          *frame, GetTintInternal(kFrameTintMap[i].value));
    }
  }
  MergeImageCaches(temp_output, images);
}

void BrowserThemePack::CreateTintedButtons(
    const color_utils::HSL& button_tint,
    ImageCache* processed_images) const {
  if (button_tint.h != -1 || button_tint.s != -1 || button_tint.l != -1) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const std::set<int>& idr_ids =
        ThemeProperties::GetTintableToolbarButtons();
    for (std::set<int>::const_iterator it = idr_ids.begin();
         it != idr_ids.end(); ++it) {
      int prs_id = GetPersistentIDByIDR(*it);
      DCHECK(prs_id > 0);

      // Fetch the image by IDR...
      gfx::Image& button = rb.GetImageNamed(*it);

      // but save a version with the persistent ID.
      (*processed_images)[prs_id] =
          CreateHSLShiftedImage(button, button_tint);
    }
  }
}

void BrowserThemePack::CreateTabBackgroundImages(ImageCache* images) const {
  ImageCache temp_output;
  for (size_t i = 0; i < arraysize(kTabBackgroundMap); ++i) {
    int prs_id = kTabBackgroundMap[i].key;
    int prs_base_id = kTabBackgroundMap[i].value;

    // We only need to generate the background tab images if we were provided
    // with a PRS_THEME_FRAME.
    ImageCache::const_iterator it = images->find(prs_base_id);
    if (it != images->end()) {
      const gfx::ImageSkia* image_to_tint = (it->second)->ToImageSkia();
      color_utils::HSL hsl_shift = GetTintInternal(
          ThemeProperties::TINT_BACKGROUND_TAB);
      int vertical_offset = images->count(prs_id)
                            ? kRestoredTabVerticalOffset : 0;

      gfx::ImageSkia overlay;
      ImageCache::const_iterator overlay_it = images->find(prs_id);
      if (overlay_it != images->end())
        overlay = *overlay_it->second->ToImageSkia();

      gfx::ImageSkiaSource* source = new TabBackgroundImageSource(
          *image_to_tint, overlay, hsl_shift, vertical_offset);
      // ImageSkia takes ownership of |source|.
      temp_output[prs_id] = new gfx::Image(gfx::ImageSkia(source,
          image_to_tint->size()));
    }
  }
  MergeImageCaches(temp_output, images);
}

void BrowserThemePack::RepackImages(const ImageCache& images,
                                    RawImages* reencoded_images) const {
  typedef std::vector<ui::ScaleFactor> ScaleFactors;
  for (ImageCache::const_iterator it = images.begin();
       it != images.end(); ++it) {
    gfx::ImageSkia image_skia = *it->second->ToImageSkia();

    typedef std::vector<gfx::ImageSkiaRep> ImageSkiaReps;
    ImageSkiaReps image_reps = image_skia.image_reps();
    if (image_reps.empty()) {
      NOTREACHED() << "No image reps for resource " << it->first << ".";
    }
    for (ImageSkiaReps::iterator rep_it = image_reps.begin();
         rep_it != image_reps.end(); ++rep_it) {
      std::vector<unsigned char> bitmap_data;
      if (!gfx::PNGCodec::EncodeBGRASkBitmap(rep_it->sk_bitmap(), false,
                                             &bitmap_data)) {
        NOTREACHED() << "Image file for resource " << it->first
                     << " could not be encoded.";
      }
      int raw_id = GetRawIDByPersistentID(it->first, rep_it->scale_factor());
      (*reencoded_images)[raw_id] =
          base::RefCountedBytes::TakeVector(&bitmap_data);
    }
  }
}

void BrowserThemePack::MergeImageCaches(
    const ImageCache& source, ImageCache* destination) const {
  for (ImageCache::const_iterator it = source.begin(); it != source.end();
       ++it) {
    ImageCache::const_iterator image_it = destination->find(it->first);
    if (image_it != destination->end())
      delete image_it->second;

    (*destination)[it->first] = it->second;
  }
}

void BrowserThemePack::CopyImagesTo(const ImageCache& source,
                                    ImageCache* destination) const {
  for (ImageCache::const_iterator it = source.begin(); it != source.end();
       ++it) {
    (*destination)[it->first] = new gfx::Image(*it->second);
  }
}

void BrowserThemePack::AddRawImagesTo(const RawImages& images,
                                      RawDataForWriting* out) const {
  for (RawImages::const_iterator it = images.begin(); it != images.end();
       ++it) {
    (*out)[it->first] = base::StringPiece(
        reinterpret_cast<const char*>(it->second->front()), it->second->size());
  }
}

color_utils::HSL BrowserThemePack::GetTintInternal(int id) const {
  if (tints_) {
    for (size_t i = 0; i < kTintTableLength; ++i) {
      if (tints_[i].id == id) {
        color_utils::HSL hsl;
        hsl.h = tints_[i].h;
        hsl.s = tints_[i].s;
        hsl.l = tints_[i].l;
        return hsl;
      }
    }
  }

  return ThemeProperties::GetDefaultTint(id);
}

int BrowserThemePack::GetRawIDByPersistentID(
    int prs_id,
    ui::ScaleFactor scale_factor) const {
  if (prs_id < 0)
    return -1;

  for (size_t i = 0; i < scale_factors_.size(); ++i) {
    if (scale_factors_[i] == scale_factor)
      return static_cast<int>(kPersistingImagesLength * i) + prs_id;
  }
  return -1;
}
