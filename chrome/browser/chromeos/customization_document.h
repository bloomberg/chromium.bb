// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
#define CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"

class DictionaryValue;
class ListValue;
class FilePath;

namespace chromeos {

// Base class for OEM customization document classes.

class CustomizationDocument {
 public:
  CustomizationDocument() {}
  virtual ~CustomizationDocument() {}

  virtual bool LoadManifestFromFile(const FilePath& manifest_path);
  virtual bool LoadManifestFromString(const std::string& manifest);

  const std::string& version() const { return version_; }

 protected:
  // Parses manifest's attributes from the JSON dictionary value.
  virtual bool ParseFromJsonValue(const DictionaryValue* root);

  // Manifest version string.
  std::string version_;

  DISALLOW_COPY_AND_ASSIGN(CustomizationDocument);
};

// OEM startup customization document class.

class StartupCustomizationDocument : public CustomizationDocument {
 public:
  struct SetupContent {
    SetupContent() {}
    SetupContent(const std::string& help_page_path,
                 const std::string& eula_page_path)
        : help_page_path(help_page_path),
          eula_page_path(eula_page_path) {}

    // Partner's help page for specific locale.
    std::string help_page_path;
    // Partner's EULA for specific locale.
    std::string eula_page_path;
  };

  typedef std::map<std::string, SetupContent> SetupContentMap;

  StartupCustomizationDocument() {}

  const std::string& product_sku() const { return product_sku_; }
  const std::string& initial_locale() const { return initial_locale_; }
  const std::string& initial_timezone() const { return initial_timezone_; }
  SkColor background_color() const { return background_color_; }
  const std::string& registration_url() const { return registration_url_; }

  const SetupContent* GetSetupContent(const std::string& locale) const;

 protected:
  virtual bool ParseFromJsonValue(const DictionaryValue* root);

  // Product SKU.
  std::string product_sku_;

  // Initial locale for the OOBE wizard.
  std::string initial_locale_;

  // Initial timezone for clock setting.
  std::string initial_timezone_;

  // OOBE wizard and login screen background color.
  SkColor background_color_;

  // Partner's product registration page URL.
  std::string registration_url_;

  // Setup content for different locales.
  SetupContentMap setup_content_;

  DISALLOW_COPY_AND_ASSIGN(StartupCustomizationDocument);
};

// OEM services customization document class.

class ServicesCustomizationDocument : public CustomizationDocument {
 public:
  typedef std::vector<std::string> StringList;

  ServicesCustomizationDocument() {}

  const std::string& initial_start_page_url() const {
    return initial_start_page_url_;
  }
  const std::string& app_menu_section_title() const {
    return app_menu_section_title_;
  }
  const std::string& app_menu_support_page_url() const {
    return app_menu_support_page_url_;
  }
  const StringList& web_apps() const { return web_apps_; }
  const StringList& extensions() const { return extensions_; }

 protected:
  virtual bool ParseFromJsonValue(const DictionaryValue* root);

  bool ParseStringListFromJsonValue(const ListValue* list_value,
                                    StringList* string_list);

  // Partner's welcome page that is opened right after the OOBE.
  std::string initial_start_page_url_;

  // Partner's featured apps URLs list.
  StringList web_apps_;

  // Partner's featured extensions URLs list.
  StringList extensions_;

  // Title for the partner's apps section in apps menu.
  std::string app_menu_section_title_;

  // Partner's apps section support page URL.
  std::string app_menu_support_page_url_;

  DISALLOW_COPY_AND_ASSIGN(ServicesCustomizationDocument);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
