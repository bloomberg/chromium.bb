// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/browser_theme_pack.h"

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/resource/data_pack.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {

// Version number of the current theme pack. We just throw out and rebuild
// theme packs that aren't int-equal to this.
const int kThemePackVersion = 15;

// IDs that are in the DataPack won't clash with the positive integer
// int32_t. kHeaderID should always have the maximum value because we want the
// "header" to be written last. That way we can detect whether the pack was
// successfully written and ignore and regenerate if it was only partially
// written (i.e. chrome crashed on a different thread while writing the pack).
const int kHeaderID = UINT_MAX - 1;
const int kTintsID = UINT_MAX - 2;
const int kColorsID = UINT_MAX - 3;
const int kDisplayPropertiesID = UINT_MAX - 4;
const int kSourceImagesID = UINT_MAX - 5;

// Static size of the tint/color/display property arrays that are mmapped.
const int kTintArraySize = 6;
const int kColorArraySize = 19;
const int kDisplayPropertySize = 3;

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
};

int GetPersistentIDByName(const std::string& key) {
  for (size_t i = 0; i < arraysize(kPersistingImages); ++i) {
    if (kPersistingImages[i].key != NULL &&
        base::strcasecmp(key.c_str(), kPersistingImages[i].key) == 0) {
      return kPersistingImages[i].persistent_id;
    }
  }

  return -1;
}

int GetPersistentIDByIDR(int idr) {
  for (size_t i = 0; i < arraysize(kPersistingImages); ++i) {
    if (kPersistingImages[i].idr_id == idr) {
      return kPersistingImages[i].persistent_id;
    }
  }

  return -1;
}

struct StringToIntTable {
  const char* key;
  int id;
};

// Strings used by themes to identify tints in the JSON.
StringToIntTable kTintTable[] = {
  { "buttons", BrowserThemeProvider::TINT_BUTTONS },
  { "frame", BrowserThemeProvider::TINT_FRAME },
  { "frame_inactive", BrowserThemeProvider::TINT_FRAME_INACTIVE },
  { "frame_incognito", BrowserThemeProvider::TINT_FRAME_INCOGNITO },
  { "frame_incognito_inactive",
    BrowserThemeProvider::TINT_FRAME_INCOGNITO_INACTIVE },
  { "background_tab", BrowserThemeProvider::TINT_BACKGROUND_TAB },
  { NULL, 0 }
};

// Strings used by themes to identify colors in the JSON.
StringToIntTable kColorTable[] = {
  { "frame", BrowserThemeProvider::COLOR_FRAME },
  { "frame_inactive", BrowserThemeProvider::COLOR_FRAME_INACTIVE },
  { "frame_incognito", BrowserThemeProvider::COLOR_FRAME_INCOGNITO },
  { "frame_incognito_inactive",
    BrowserThemeProvider::COLOR_FRAME_INCOGNITO_INACTIVE },
  { "toolbar", BrowserThemeProvider::COLOR_TOOLBAR },
  { "tab_text", BrowserThemeProvider::COLOR_TAB_TEXT },
  { "tab_background_text", BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT },
  { "bookmark_text", BrowserThemeProvider::COLOR_BOOKMARK_TEXT },
  { "ntp_background", BrowserThemeProvider::COLOR_NTP_BACKGROUND },
  { "ntp_text", BrowserThemeProvider::COLOR_NTP_TEXT },
  { "ntp_link", BrowserThemeProvider::COLOR_NTP_LINK },
  { "ntp_link_underline", BrowserThemeProvider::COLOR_NTP_LINK_UNDERLINE },
  { "ntp_header", BrowserThemeProvider::COLOR_NTP_HEADER },
  { "ntp_section", BrowserThemeProvider::COLOR_NTP_SECTION },
  { "ntp_section_text", BrowserThemeProvider::COLOR_NTP_SECTION_TEXT },
  { "ntp_section_link", BrowserThemeProvider::COLOR_NTP_SECTION_LINK },
  { "ntp_section_link_underline",
    BrowserThemeProvider::COLOR_NTP_SECTION_LINK_UNDERLINE },
  { "control_background", BrowserThemeProvider::COLOR_CONTROL_BACKGROUND },
  { "button_background", BrowserThemeProvider::COLOR_BUTTON_BACKGROUND },
  { NULL, 0 }
};

// Strings used by themes to identify display properties keys in JSON.
StringToIntTable kDisplayProperties[] = {
  { "ntp_background_alignment",
    BrowserThemeProvider::NTP_BACKGROUND_ALIGNMENT },
  { "ntp_background_repeat", BrowserThemeProvider::NTP_BACKGROUND_TILING },
  { "ntp_logo_alternate", BrowserThemeProvider::NTP_LOGO_ALTERNATE },
  { NULL, 0 }
};

// Strings used by the tiling values in JSON.
StringToIntTable kTilingStrings[] = {
  { "no-repeat", BrowserThemeProvider::NO_REPEAT },
  { "repeat-x", BrowserThemeProvider::REPEAT_X },
  { "repeat-y", BrowserThemeProvider::REPEAT_Y },
  { "repeat", BrowserThemeProvider::REPEAT },
  { NULL, 0 }
};

int GetIntForString(const std::string& key, StringToIntTable* table) {
  for (int i = 0; table[i].key != NULL; ++i) {
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
  { PRS_THEME_FRAME, BrowserThemeProvider::TINT_FRAME },
  { PRS_THEME_FRAME_INACTIVE, BrowserThemeProvider::TINT_FRAME_INACTIVE },
  { PRS_THEME_FRAME_OVERLAY, BrowserThemeProvider::TINT_FRAME },
  { PRS_THEME_FRAME_OVERLAY_INACTIVE,
    BrowserThemeProvider::TINT_FRAME_INACTIVE },
  { PRS_THEME_FRAME_INCOGNITO, BrowserThemeProvider::TINT_FRAME_INCOGNITO },
  { PRS_THEME_FRAME_INCOGNITO_INACTIVE,
    BrowserThemeProvider::TINT_FRAME_INCOGNITO_INACTIVE }
};

// Mapping used in GenerateTabBackgroundImages() to associate what frame image
// goes with which tab background.
IntToIntTable kTabBackgroundMap[] = {
  { PRS_THEME_TAB_BACKGROUND, PRS_THEME_FRAME },
  { PRS_THEME_TAB_BACKGROUND_INCOGNITO, PRS_THEME_FRAME_INCOGNITO }
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
  PRS_THEME_WINDOW_CONTROL_BACKGROUND
};

// Returns a piece of memory with the contents of the file |path|.
RefCountedMemory* ReadFileData(const FilePath& path) {
  if (!path.empty()) {
    net::FileStream file;
    int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
    if (file.Open(path, flags) == net::OK) {
      int64 avail = file.Available();
      if (avail > 0 && avail < INT_MAX) {
        size_t size = static_cast<size_t>(avail);
        std::vector<unsigned char> raw_data;
        raw_data.resize(size);
        char* data = reinterpret_cast<char*>(&(raw_data.front()));
        if (file.ReadUntilComplete(data, size) == avail)
          return RefCountedBytes::TakeVector(&raw_data);
      }
    }
  }

  return NULL;
}

// Does error checking for invalid incoming data while trying to read an
// floating point value.
bool ValidDoubleValue(ListValue* tint_list, int index, double* out) {
  if (tint_list->GetDouble(index, out))
    return true;

  int value = 0;
  if (tint_list->GetInteger(index, &value)) {
    *out = value;
    return true;
  }

  return false;
}

}  // namespace

BrowserThemePack::~BrowserThemePack() {
  if (!data_pack_.get()) {
    delete header_;
    delete [] tints_;
    delete [] colors_;
    delete [] display_properties_;
    delete [] source_images_;
  }

  STLDeleteValues(&prepared_images_);
  STLDeleteValues(&loaded_images_);
}

// static
BrowserThemePack* BrowserThemePack::BuildFromExtension(
    const Extension* extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension);
  DCHECK(extension->is_theme());

  BrowserThemePack* pack = new BrowserThemePack;
  pack->BuildHeader(extension);
  pack->BuildTintsFromJSON(extension->GetThemeTints());
  pack->BuildColorsFromJSON(extension->GetThemeColors());
  pack->BuildDisplayPropertiesFromJSON(extension->GetThemeDisplayProperties());

  // Builds the images. (Image building is dependent on tints).
  FilePathMap file_paths;
  pack->ParseImageNamesFromJSON(extension->GetThemeImages(),
                                extension->path(),
                                &file_paths);
  pack->BuildSourceImagesArray(file_paths);

  if (!pack->LoadRawBitmapsTo(file_paths, &pack->prepared_images_))
    return NULL;

  pack->GenerateFrameImages(&pack->prepared_images_);

#if !defined(OS_MACOSX)
  // OSX uses its own special buttons that are PDFs that do odd sorts of vector
  // graphics tricks. Other platforms use bitmaps and we must pre-tint them.
  pack->GenerateTintedButtons(
      pack->GetTintInternal(BrowserThemeProvider::TINT_BUTTONS),
      &pack->prepared_images_);
#endif

  pack->GenerateTabBackgroundImages(&pack->prepared_images_);

  // The BrowserThemePack is now in a consistent state.
  return pack;
}

// static
scoped_refptr<BrowserThemePack> BrowserThemePack::BuildFromDataPack(
    FilePath path, const std::string& expected_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<BrowserThemePack> pack(new BrowserThemePack);
  pack->data_pack_.reset(new ui::DataPack);

  if (!pack->data_pack_->Load(path)) {
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
                       Extension::kIdSize);
  std::string truncated_id = expected_id.substr(0, Extension::kIdSize);
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

  return pack;
}

bool BrowserThemePack::WriteToDisk(FilePath path) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Add resources for each of the property arrays.
  RawDataForWriting resources;
  resources[kHeaderID] = base::StringPiece(
      reinterpret_cast<const char*>(header_), sizeof(BrowserThemePackHeader));
  resources[kTintsID] = base::StringPiece(
      reinterpret_cast<const char*>(tints_), sizeof(TintEntry[kTintArraySize]));
  resources[kColorsID] = base::StringPiece(
      reinterpret_cast<const char*>(colors_),
      sizeof(ColorPair[kColorArraySize]));
  resources[kDisplayPropertiesID] = base::StringPiece(
      reinterpret_cast<const char*>(display_properties_),
      sizeof(DisplayPropertyPair[kDisplayPropertySize]));

  int source_count = 1;
  int* end = source_images_;
  for (; *end != -1 ; end++)
    source_count++;
  resources[kSourceImagesID] = base::StringPiece(
      reinterpret_cast<const char*>(source_images_),
      source_count * sizeof(*source_images_));

  AddRawImagesTo(image_memory_, &resources);

  RawImages reencoded_images;
  RepackImages(prepared_images_, &reencoded_images);
  AddRawImagesTo(reencoded_images, &resources);

  return ui::DataPack::WritePack(path, resources);
}

bool BrowserThemePack::GetTint(int id, color_utils::HSL* hsl) const {
  if (tints_) {
    for (int i = 0; i < kTintArraySize; ++i) {
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
    for (int i = 0; i < kColorArraySize; ++i) {
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
    for (int i = 0; i < kDisplayPropertySize; ++i) {
      if (display_properties_[i].id == id) {
        *result = display_properties_[i].property;
        return true;
      }
    }
  }

  return false;
}

SkBitmap* BrowserThemePack::GetBitmapNamed(int idr_id) const {
  int prs_id = GetPersistentIDByIDR(idr_id);
  if (prs_id == -1)
    return NULL;

  // Check our cache of prepared images, first.
  ImageCache::const_iterator image_iter = prepared_images_.find(prs_id);
  if (image_iter != prepared_images_.end())
    return image_iter->second;

  // Check if we've already loaded this image.
  image_iter = loaded_images_.find(prs_id);
  if (image_iter != loaded_images_.end())
    return image_iter->second;

  scoped_refptr<RefCountedMemory> memory;
  if (data_pack_.get()) {
    memory = data_pack_->GetStaticMemory(prs_id);
  } else {
    RawImages::const_iterator it = image_memory_.find(prs_id);
    if (it != image_memory_.end()) {
      memory = it->second;
    }
  }

  if (memory.get()) {
    // Decode the PNG.
    SkBitmap bitmap;
    if (!gfx::PNGCodec::Decode(memory->front(), memory->size(),
                               &bitmap)) {
      NOTREACHED() << "Unable to decode theme image resource " << idr_id
                   << " from saved DataPack.";
      return NULL;
    }

    SkBitmap* ret = new SkBitmap(bitmap);
    loaded_images_[prs_id] = ret;

    return ret;
  }

  return NULL;
}

RefCountedMemory* BrowserThemePack::GetRawData(int idr_id) const {
  RefCountedMemory* memory = NULL;
  int prs_id = GetPersistentIDByIDR(idr_id);

  if (prs_id != -1) {
    if (data_pack_.get()) {
      memory = data_pack_->GetStaticMemory(prs_id);
    } else {
      RawImages::const_iterator it = image_memory_.find(prs_id);
      if (it != image_memory_.end()) {
        memory = it->second;
      }
    }
  }

  return memory;
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
  memcpy(header_->theme_id, id.c_str(), Extension::kIdSize);
}

void BrowserThemePack::BuildTintsFromJSON(DictionaryValue* tints_value) {
  tints_ = new TintEntry[kTintArraySize];
  for (int i = 0; i < kTintArraySize; ++i) {
    tints_[i].id = -1;
    tints_[i].h = -1;
    tints_[i].s = -1;
    tints_[i].l = -1;
  }

  if (!tints_value)
    return;

  // Parse the incoming data from |tints_value| into an intermediary structure.
  std::map<int, color_utils::HSL> temp_tints;
  for (DictionaryValue::key_iterator iter(tints_value->begin_keys());
       iter != tints_value->end_keys(); ++iter) {
    ListValue* tint_list;
    if (tints_value->GetList(*iter, &tint_list) &&
        (tint_list->GetSize() == 3)) {
      color_utils::HSL hsl = { -1, -1, -1 };

      if (ValidDoubleValue(tint_list, 0, &hsl.h) &&
          ValidDoubleValue(tint_list, 1, &hsl.s) &&
          ValidDoubleValue(tint_list, 2, &hsl.l)) {
        int id = GetIntForString(*iter, kTintTable);
        if (id != -1) {
          temp_tints[id] = hsl;
        }
      }
    }
  }

  // Copy data from the intermediary data structure to the array.
  int count = 0;
  for (std::map<int, color_utils::HSL>::const_iterator it =
           temp_tints.begin(); it != temp_tints.end() && count < kTintArraySize;
       ++it, ++count) {
    tints_[count].id = it->first;
    tints_[count].h = it->second.h;
    tints_[count].s = it->second.s;
    tints_[count].l = it->second.l;
  }
}

void BrowserThemePack::BuildColorsFromJSON(DictionaryValue* colors_value) {
  colors_ = new ColorPair[kColorArraySize];
  for (int i = 0; i < kColorArraySize; ++i) {
    colors_[i].id = -1;
    colors_[i].color = SkColorSetRGB(0, 0, 0);
  }

  std::map<int, SkColor> temp_colors;
  if (colors_value)
    ReadColorsFromJSON(colors_value, &temp_colors);
  GenerateMissingColors(&temp_colors);

  // Copy data from the intermediary data structure to the array.
  int count = 0;
  for (std::map<int, SkColor>::const_iterator it = temp_colors.begin();
       it != temp_colors.end() && count < kColorArraySize; ++it, ++count) {
    colors_[count].id = it->first;
    colors_[count].color = it->second;
  }
}

void BrowserThemePack::ReadColorsFromJSON(
    DictionaryValue* colors_value,
    std::map<int, SkColor>* temp_colors) {
  // Parse the incoming data from |colors_value| into an intermediary structure.
  for (DictionaryValue::key_iterator iter(colors_value->begin_keys());
       iter != colors_value->end_keys(); ++iter) {
    ListValue* color_list;
    if (colors_value->GetList(*iter, &color_list) &&
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

        int id = GetIntForString(*iter, kColorTable);
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
  if (!colors->count(BrowserThemeProvider::COLOR_NTP_HEADER) &&
      colors->count(BrowserThemeProvider::COLOR_NTP_SECTION)) {
    (*colors)[BrowserThemeProvider::COLOR_NTP_HEADER] =
        (*colors)[BrowserThemeProvider::COLOR_NTP_SECTION];
  }

  if (!colors->count(BrowserThemeProvider::COLOR_NTP_SECTION_LINK_UNDERLINE) &&
      colors->count(BrowserThemeProvider::COLOR_NTP_SECTION_LINK)) {
    SkColor color_section_link =
        (*colors)[BrowserThemeProvider::COLOR_NTP_SECTION_LINK];
    (*colors)[BrowserThemeProvider::COLOR_NTP_SECTION_LINK_UNDERLINE] =
        SkColorSetA(color_section_link, SkColorGetA(color_section_link) / 3);
  }

  if (!colors->count(BrowserThemeProvider::COLOR_NTP_LINK_UNDERLINE) &&
      colors->count(BrowserThemeProvider::COLOR_NTP_LINK)) {
    SkColor color_link = (*colors)[BrowserThemeProvider::COLOR_NTP_LINK];
    (*colors)[BrowserThemeProvider::COLOR_NTP_LINK_UNDERLINE] =
        SkColorSetA(color_link, SkColorGetA(color_link) / 3);
  }

  // Generate frame colors, if missing. (See GenerateFrameColors()).
  SkColor frame;
  std::map<int, SkColor>::const_iterator it =
      colors->find(BrowserThemeProvider::COLOR_FRAME);
  if (it != colors->end()) {
    frame = it->second;
  } else {
    frame = BrowserThemeProvider::GetDefaultColor(
        BrowserThemeProvider::COLOR_FRAME);
  }

  if (!colors->count(BrowserThemeProvider::COLOR_FRAME)) {
    (*colors)[BrowserThemeProvider::COLOR_FRAME] =
        HSLShift(frame, GetTintInternal(BrowserThemeProvider::TINT_FRAME));
  }
  if (!colors->count(BrowserThemeProvider::COLOR_FRAME_INACTIVE)) {
    (*colors)[BrowserThemeProvider::COLOR_FRAME_INACTIVE] =
        HSLShift(frame, GetTintInternal(
            BrowserThemeProvider::TINT_FRAME_INACTIVE));
  }
  if (!colors->count(BrowserThemeProvider::COLOR_FRAME_INCOGNITO)) {
    (*colors)[BrowserThemeProvider::COLOR_FRAME_INCOGNITO] =
        HSLShift(frame, GetTintInternal(
            BrowserThemeProvider::TINT_FRAME_INCOGNITO));
  }
  if (!colors->count(BrowserThemeProvider::COLOR_FRAME_INCOGNITO_INACTIVE)) {
    (*colors)[BrowserThemeProvider::COLOR_FRAME_INCOGNITO_INACTIVE] =
        HSLShift(frame, GetTintInternal(
            BrowserThemeProvider::TINT_FRAME_INCOGNITO_INACTIVE));
  }
}

void BrowserThemePack::BuildDisplayPropertiesFromJSON(
    DictionaryValue* display_properties_value) {
  display_properties_ = new DisplayPropertyPair[kDisplayPropertySize];
  for (int i = 0; i < kDisplayPropertySize; ++i) {
    display_properties_[i].id = -1;
    display_properties_[i].property = 0;
  }

  if (!display_properties_value)
    return;

  std::map<int, int> temp_properties;
  for (DictionaryValue::key_iterator iter(
       display_properties_value->begin_keys());
       iter != display_properties_value->end_keys(); ++iter) {
    int property_id = GetIntForString(*iter, kDisplayProperties);
    switch (property_id) {
      case BrowserThemeProvider::NTP_BACKGROUND_ALIGNMENT: {
        std::string val;
        if (display_properties_value->GetString(*iter, &val)) {
          temp_properties[BrowserThemeProvider::NTP_BACKGROUND_ALIGNMENT] =
              BrowserThemeProvider::StringToAlignment(val);
        }
        break;
      }
      case BrowserThemeProvider::NTP_BACKGROUND_TILING: {
        std::string val;
        if (display_properties_value->GetString(*iter, &val)) {
          temp_properties[BrowserThemeProvider::NTP_BACKGROUND_TILING] =
              GetIntForString(val, kTilingStrings);
        }
        break;
      }
      case BrowserThemeProvider::NTP_LOGO_ALTERNATE: {
        int val = 0;
        if (display_properties_value->GetInteger(*iter, &val))
          temp_properties[BrowserThemeProvider::NTP_LOGO_ALTERNATE] = val;
        break;
      }
    }
  }

  // Copy data from the intermediary data structure to the array.
  int count = 0;
  for (std::map<int, int>::const_iterator it = temp_properties.begin();
       it != temp_properties.end() && count < kDisplayPropertySize;
       ++it, ++count) {
    display_properties_[count].id = it->first;
    display_properties_[count].property = it->second;
  }
}

void BrowserThemePack::ParseImageNamesFromJSON(
    DictionaryValue* images_value,
    FilePath images_path,
    FilePathMap* file_paths) const {
  if (!images_value)
    return;

  for (DictionaryValue::key_iterator iter(images_value->begin_keys());
       iter != images_value->end_keys(); ++iter) {
    std::string val;
    if (images_value->GetString(*iter, &val)) {
      int id = GetPersistentIDByName(*iter);
      if (id != -1)
        (*file_paths)[id] = images_path.AppendASCII(val);
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
    ImageCache* raw_bitmaps) {
  // Themes should be loaded on the file thread, not the UI thread.
  // http://crbug.com/61838
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  for (FilePathMap::const_iterator it = file_paths.begin();
       it != file_paths.end(); ++it) {
    scoped_refptr<RefCountedMemory> raw_data(ReadFileData(it->second));
    if (!raw_data.get()) {
      LOG(ERROR) << "Could not load theme image";
      return false;
    }

    int id = it->first;

    // Some images need to go directly into |image_memory_|. No modification is
    // necessary or desirable.
    bool is_copyable = false;
    for (size_t i = 0; i < arraysize(kPreloadIDs); ++i) {
      if (kPreloadIDs[i] == id) {
        is_copyable = true;
        break;
      }
    }

    if (is_copyable) {
      image_memory_[id] = raw_data;
    } else if (raw_data.get() && raw_data->size()) {
      // Decode the PNG.
      SkBitmap bitmap;
      if (gfx::PNGCodec::Decode(raw_data->front(), raw_data->size(),
                                &bitmap)) {
        (*raw_bitmaps)[it->first] = new SkBitmap(bitmap);
      } else {
        NOTREACHED() << "Unable to decode theme image resource " << it->first;
      }
    }
  }

  return true;
}

void BrowserThemePack::GenerateFrameImages(ImageCache* bitmaps) const {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Create all the output bitmaps in a separate cache and move them back into
  // the input bitmaps because there can be name collisions.
  ImageCache temp_output;

  for (size_t i = 0; i < arraysize(kFrameTintMap); ++i) {
    int prs_id = kFrameTintMap[i].key;
    scoped_ptr<SkBitmap> frame;
    // If there's no frame image provided for the specified id, then load
    // the default provided frame. If that's not provided, skip this whole
    // thing and just use the default images.
    int prs_base_id;

    if (prs_id == PRS_THEME_FRAME_INCOGNITO_INACTIVE) {
      prs_base_id = bitmaps->count(PRS_THEME_FRAME_INCOGNITO) ?
                    PRS_THEME_FRAME_INCOGNITO : PRS_THEME_FRAME;
    } else if (prs_id == PRS_THEME_FRAME_OVERLAY_INACTIVE) {
      prs_base_id = PRS_THEME_FRAME_OVERLAY;
    } else if (prs_id == PRS_THEME_FRAME_INACTIVE) {
      prs_base_id = PRS_THEME_FRAME;
    } else if (prs_id == PRS_THEME_FRAME_INCOGNITO &&
               !bitmaps->count(PRS_THEME_FRAME_INCOGNITO)) {
      prs_base_id = PRS_THEME_FRAME;
    } else {
      prs_base_id = prs_id;
    }

    if (bitmaps->count(prs_id)) {
      frame.reset(new SkBitmap(*(*bitmaps)[prs_id]));
    } else if (prs_base_id != prs_id && bitmaps->count(prs_base_id)) {
      frame.reset(new SkBitmap(*(*bitmaps)[prs_base_id]));
    } else if (prs_base_id == PRS_THEME_FRAME_OVERLAY &&
               bitmaps->count(PRS_THEME_FRAME)) {
      // If there is no theme overlay, don't tint the default frame,
      // because it will overwrite the custom frame image when we cache and
      // reload from disk.
      frame.reset(NULL);
    } else {
      // If the theme doesn't specify an image, then apply the tint to
      // the default frame.
      frame.reset(new SkBitmap(*rb.GetBitmapNamed(IDR_THEME_FRAME)));
    }

    if (frame.get()) {
      temp_output[prs_id] = new SkBitmap(
          SkBitmapOperations::CreateHSLShiftedBitmap(
              *frame, GetTintInternal(kFrameTintMap[i].value)));
    }
  }

  MergeImageCaches(temp_output, bitmaps);
}

void BrowserThemePack::GenerateTintedButtons(
    const color_utils::HSL& button_tint,
    ImageCache* processed_bitmaps) const {
  if (button_tint.h != -1 || button_tint.s != -1 || button_tint.l != -1) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const std::set<int>& idr_ids =
        BrowserThemeProvider::GetTintableToolbarButtons();
    for (std::set<int>::const_iterator it = idr_ids.begin();
         it != idr_ids.end(); ++it) {
      int prs_id = GetPersistentIDByIDR(*it);
      DCHECK(prs_id > 0);

      // Fetch the image by IDR...
      scoped_ptr<SkBitmap> button(new SkBitmap(*rb.GetBitmapNamed(*it)));

      // but save a version with the persistent ID.
      (*processed_bitmaps)[prs_id] = new SkBitmap(
          SkBitmapOperations::CreateHSLShiftedBitmap(*button, button_tint));
    }
  }
}

void BrowserThemePack::GenerateTabBackgroundImages(ImageCache* bitmaps) const {
  ImageCache temp_output;
  for (size_t i = 0; i < arraysize(kTabBackgroundMap); ++i) {
    int prs_id = kTabBackgroundMap[i].key;
    int prs_base_id = kTabBackgroundMap[i].value;

    // We only need to generate the background tab images if we were provided
    // with a PRS_THEME_FRAME.
    ImageCache::const_iterator it = bitmaps->find(prs_base_id);
    if (it != bitmaps->end()) {
      SkBitmap bg_tint = SkBitmapOperations::CreateHSLShiftedBitmap(
          *(it->second), GetTintInternal(
              BrowserThemeProvider::TINT_BACKGROUND_TAB));
      int vertical_offset = bitmaps->count(prs_id)
                            ? kRestoredTabVerticalOffset : 0;
      SkBitmap* bg_tab = new SkBitmap(SkBitmapOperations::CreateTiledBitmap(
          bg_tint, 0, vertical_offset, bg_tint.width(), bg_tint.height()));

      // If they've provided a custom image, overlay it.
      ImageCache::const_iterator overlay_it = bitmaps->find(prs_id);
      if (overlay_it != bitmaps->end()) {
        SkBitmap* overlay = overlay_it->second;
        SkCanvas canvas(*bg_tab);
        for (int x = 0; x < bg_tab->width(); x += overlay->width())
          canvas.drawBitmap(*overlay, static_cast<SkScalar>(x), 0, NULL);
      }

      temp_output[prs_id] = bg_tab;
    }
  }

  MergeImageCaches(temp_output, bitmaps);
}

void BrowserThemePack::RepackImages(const ImageCache& images,
                                    RawImages* reencoded_images) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  for (ImageCache::const_iterator it = images.begin();
       it != images.end(); ++it) {
    std::vector<unsigned char> image_data;
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(*(it->second), false, &image_data)) {
      NOTREACHED() << "Image file for resource " << it->first
                   << " could not be encoded.";
    } else {
      (*reencoded_images)[it->first] = RefCountedBytes::TakeVector(&image_data);
    }
  }
}

void BrowserThemePack::MergeImageCaches(
    const ImageCache& source, ImageCache* destination) const {

  for (ImageCache::const_iterator it = source.begin(); it != source.end();
       ++it) {
    ImageCache::const_iterator bitmap_it = destination->find(it->first);
    if (bitmap_it != destination->end())
      delete bitmap_it->second;

    (*destination)[it->first] = it->second;
  }
}

void BrowserThemePack::AddRawImagesTo(const RawImages& images,
                                      RawDataForWriting* out) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  for (RawImages::const_iterator it = images.begin(); it != images.end();
       ++it) {
    (*out)[it->first] = base::StringPiece(
        reinterpret_cast<const char*>(it->second->front()), it->second->size());
  }
}

color_utils::HSL BrowserThemePack::GetTintInternal(int id) const {
  if (tints_) {
    for (int i = 0; i < kTintArraySize; ++i) {
      if (tints_[i].id == id) {
        color_utils::HSL hsl;
        hsl.h = tints_[i].h;
        hsl.s = tints_[i].s;
        hsl.l = tints_[i].l;
        return hsl;
      }
    }
  }

  return BrowserThemeProvider::GetDefaultTint(id);
}
