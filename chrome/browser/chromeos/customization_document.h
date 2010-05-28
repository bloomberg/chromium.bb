// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
#define CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_

#include <list>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"

class DictionaryValue;
class FilePath;

namespace chromeos {

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
  SkColor background_color() const { return background_color_; }
  const std::string& registration_url() const { return registration_url_; }

  const SetupContent* GetSetupContent(const std::string& locale) const;

 protected:
  virtual bool ParseFromJsonValue(const DictionaryValue* root);

  // Product SKU.
  std::string product_sku_;

  // Initial locale for the OOBE wizard.
  std::string initial_locale_;

  // OOBE wizard and login screen background color.
  SkColor background_color_;

  // Partner's product registration page URL.
  std::string registration_url_;

  // Setup content for different locales.
  SetupContentMap setup_content_;

  DISALLOW_COPY_AND_ASSIGN(StartupCustomizationDocument);
};

class ServicesCustomizationDocument : public CustomizationDocument {
 public:
  ServicesCustomizationDocument() {}

 protected:
  virtual bool ParseFromJsonValue(const DictionaryValue* root);

  // Partner's welcome page that is opened right after the OOBE.
  std::string initial_start_page_;

  // Title for the partner's apps section in apps menu.
  std::string app_menu_section_title_;

  // Partner's featured apps URLs.
  std::list<std::string> web_apps_;

  // Partner's featured extensions URLs.
  std::list<std::string> extensions_;

  // Partner's apps section support page URL.
  std::string app_menu_support_page_url_;

  DISALLOW_COPY_AND_ASSIGN(ServicesCustomizationDocument);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
