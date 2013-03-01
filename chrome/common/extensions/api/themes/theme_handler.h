// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_THEMES_THEME_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_THEMES_THEME_HANDLER_H_

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace base {
class DictionaryValue;
}

namespace extensions {

// A structure to hold the parsed theme data.
struct ThemeInfo : public Extension::ManifestData {
  // Define out of line constructor/destructor to please Clang.
  ThemeInfo();
  virtual ~ThemeInfo();

  static base::DictionaryValue* GetThemeImages(const Extension* extension);
  static base::DictionaryValue* GetThemeColors(const Extension* extension);
  static base::DictionaryValue* GetThemeTints(const Extension* extension);
  static base::DictionaryValue* GetThemeDisplayProperties(
      const Extension* extension);

  // A map of resource id's to relative file paths.
  scoped_ptr<base::DictionaryValue> theme_images_;

  // A map of color names to colors.
  scoped_ptr<base::DictionaryValue> theme_colors_;

  // A map of color names to colors.
  scoped_ptr<base::DictionaryValue> theme_tints_;

  // A map of display properties.
  scoped_ptr<base::DictionaryValue> theme_display_properties_;
};

// Parses the "theme" manifest key.
class ThemeHandler : public ManifestHandler {
 public:
  ThemeHandler();
  virtual ~ThemeHandler();

  virtual bool Parse(Extension* extension, string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ThemeHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_THEMES_THEME_HANDLER_H_
