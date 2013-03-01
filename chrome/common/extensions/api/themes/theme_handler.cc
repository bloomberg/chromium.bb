// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/themes/theme_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"

namespace extensions {

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace {

bool LoadThemeImages(const DictionaryValue* theme_value,
                     string16* error,
                     ThemeInfo* theme_info) {
  const DictionaryValue* images_value = NULL;
  if (theme_value->GetDictionary(keys::kThemeImages, &images_value)) {
    // Validate that the images are all strings.
    for (DictionaryValue::key_iterator iter = images_value->begin_keys();
         iter != images_value->end_keys(); ++iter) {
      std::string val;
      if (!images_value->GetString(*iter, &val)) {
        *error = ASCIIToUTF16(errors::kInvalidThemeImages);
        return false;
      }
    }
    theme_info->theme_images_.reset(images_value->DeepCopy());
  }
  return true;
}

bool LoadThemeColors(const DictionaryValue* theme_value,
                     string16* error,
                     ThemeInfo* theme_info) {
  const DictionaryValue* colors_value = NULL;
  if (theme_value->GetDictionary(keys::kThemeColors, &colors_value)) {
    // Validate that the colors are RGB or RGBA lists.
    for (DictionaryValue::key_iterator iter = colors_value->begin_keys();
         iter != colors_value->end_keys(); ++iter) {
      const ListValue* color_list = NULL;
      double alpha = 0.0;
      int color = 0;
      // The color must be a list...
      if (!colors_value->GetListWithoutPathExpansion(*iter, &color_list) ||
          // ... and either 3 items (RGB) or 4 (RGBA).
          ((color_list->GetSize() != 3) &&
           ((color_list->GetSize() != 4) ||
            // For RGBA, the fourth item must be a real or int alpha value.
            // Note that GetDouble() can get an integer value.
            !color_list->GetDouble(3, &alpha))) ||
          // For both RGB and RGBA, the first three items must be ints (R,G,B).
          !color_list->GetInteger(0, &color) ||
          !color_list->GetInteger(1, &color) ||
          !color_list->GetInteger(2, &color)) {
        *error = ASCIIToUTF16(errors::kInvalidThemeColors);
        return false;
      }
    }
    theme_info->theme_colors_.reset(colors_value->DeepCopy());
  }
  return true;
}

bool LoadThemeTints(const DictionaryValue* theme_value,
                    string16* error,
                    ThemeInfo* theme_info) {
  const DictionaryValue* tints_value = NULL;
  if (!theme_value->GetDictionary(keys::kThemeTints, &tints_value))
    return true;

  // Validate that the tints are all reals.
  for (DictionaryValue::key_iterator iter = tints_value->begin_keys();
       iter != tints_value->end_keys(); ++iter) {
    const ListValue* tint_list = NULL;
    double v = 0.0;
    if (!tints_value->GetListWithoutPathExpansion(*iter, &tint_list) ||
        tint_list->GetSize() != 3 ||
        !tint_list->GetDouble(0, &v) ||
        !tint_list->GetDouble(1, &v) ||
        !tint_list->GetDouble(2, &v)) {
      *error = ASCIIToUTF16(errors::kInvalidThemeTints);
      return false;
    }
  }
  theme_info->theme_tints_.reset(tints_value->DeepCopy());
  return true;
}

bool LoadThemeDisplayProperties(const DictionaryValue* theme_value,
                                string16* error,
                                ThemeInfo* theme_info) {
  const DictionaryValue* display_properties_value = NULL;
  if (theme_value->GetDictionary(keys::kThemeDisplayProperties,
                                 &display_properties_value)) {
    theme_info->theme_display_properties_.reset(
        display_properties_value->DeepCopy());
  }
  return true;
}

const ThemeInfo* GetThemeInfo(const Extension* extension) {
  return static_cast<ThemeInfo*>(extension->GetManifestData(keys::kTheme));
}

}  // namespace

ThemeInfo::ThemeInfo() {
}

ThemeInfo::~ThemeInfo() {
}

// static
DictionaryValue* ThemeInfo::GetThemeImages(const Extension* extension) {
  const ThemeInfo* theme_info = GetThemeInfo(extension);
  return theme_info ? theme_info->theme_images_.get() : NULL;
}

// static
DictionaryValue* ThemeInfo::GetThemeColors(const Extension* extension) {
  const ThemeInfo* theme_info = GetThemeInfo(extension);
  return theme_info ? theme_info->theme_colors_.get() : NULL;
}

// static
DictionaryValue* ThemeInfo::GetThemeTints(const Extension* extension) {
  const ThemeInfo* theme_info = GetThemeInfo(extension);
  return theme_info ? theme_info->theme_tints_.get() : NULL;
}

// static
DictionaryValue* ThemeInfo::GetThemeDisplayProperties(
    const Extension* extension) {
  const ThemeInfo* theme_info = GetThemeInfo(extension);
  return theme_info ? theme_info->theme_display_properties_.get() : NULL;
}

ThemeHandler::ThemeHandler() {
}

ThemeHandler::~ThemeHandler() {
}

bool ThemeHandler::Parse(Extension* extension, string16* error) {
  const DictionaryValue* theme_value = NULL;
  if (!extension->manifest()->GetDictionary(keys::kTheme, &theme_value)) {
    *error = ASCIIToUTF16(errors::kInvalidTheme);
    return false;
  }

  scoped_ptr<ThemeInfo> theme_info(new ThemeInfo);
  if (!LoadThemeImages(theme_value, error, theme_info.get()))
    return false;
  if (!LoadThemeColors(theme_value, error, theme_info.get()))
    return false;
  if (!LoadThemeTints(theme_value, error, theme_info.get()))
    return false;
  if (!LoadThemeDisplayProperties(theme_value, error, theme_info.get()))
    return false;

  extension->SetManifestData(keys::kTheme, theme_info.release());
  return true;
}

const std::vector<std::string> ThemeHandler::Keys() const {
  return SingleKey(keys::kTheme);
}

}  // namespace extensions
