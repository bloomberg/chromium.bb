// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_document.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"

// Manifest attributes names.

namespace {

const wchar_t kVersionAttr[] = L"version";
const wchar_t kProductSkuAttr[] = L"product_sku";
const wchar_t kInitialLocaleAttr[] = L"initial_locale";
const wchar_t kInitialTimezoneAttr[] = L"initial_timezone";
const wchar_t kBackgroundColorAttr[] = L"background_color";
const wchar_t kRegistrationUrlAttr[] = L"registration_url";
const wchar_t kSetupContentAttr[] = L"setup_content";
const wchar_t kContentLocaleAttr[] = L"content_locale";
const wchar_t kHelpPageAttr[] = L"help_page";
const wchar_t kEulaPageAttr[] = L"eula_page";
const wchar_t kAppMenuAttr[] = L"app_menu";
const wchar_t kInitialStartPageAttr[] = L"initial_start_page";
const wchar_t kSectionTitleAttr[] = L"section_title";
const wchar_t kWebAppsAttr[] = L"web_apps";
const wchar_t kSupportPageAttr[] = L"support_page";
const wchar_t kExtensionsAttr[] = L"extensions";

const char kAcceptedManifestVersion[] = "1.0";

}  // anonymous namespace

namespace chromeos {

// CustomizationDocument implementation.

bool CustomizationDocument::LoadManifestFromFile(
    const FilePath& manifest_path) {
  std::string manifest;
  bool read_success = file_util::ReadFileToString(manifest_path, &manifest);
  if (!read_success) {
    return false;
  }
  return LoadManifestFromString(manifest);
}

bool CustomizationDocument::LoadManifestFromString(
    const std::string& manifest) {
  scoped_ptr<Value> root(base::JSONReader::Read(manifest, true));
  DCHECK(root.get() != NULL);
  if (root.get() == NULL)
    return false;
  DCHECK(root->GetType() == Value::TYPE_DICTIONARY);
  return ParseFromJsonValue(static_cast<DictionaryValue*>(root.get()));
}

bool CustomizationDocument::ParseFromJsonValue(const DictionaryValue* root) {
  // Partner customization manifests share only one required field -
  // version string.
  version_.clear();
  bool result = root->GetString(kVersionAttr, &version_);
  return result && version_ == kAcceptedManifestVersion;
}

// StartupCustomizationDocument implementation.

bool StartupCustomizationDocument::ParseFromJsonValue(
    const DictionaryValue* root) {
  if (!CustomizationDocument::ParseFromJsonValue(root))
    return false;

  // Required fields.
  product_sku_.clear();
  if (!root->GetString(kProductSkuAttr, &product_sku_))
    return false;

  // Optional fields.
  initial_locale_.clear();
  root->GetString(kInitialLocaleAttr, &initial_locale_);

  initial_timezone_.clear();
  root->GetString(kInitialTimezoneAttr, &initial_timezone_);

  std::string background_color_string;
  root->GetString(kBackgroundColorAttr, &background_color_string);
  if (!background_color_string.empty()) {
    if (background_color_string[0] == '#') {
      background_color_ = static_cast<SkColor>(
          0xff000000 | HexStringToInt(background_color_string.substr(1)));
    } else {
      // Literal color constants are not supported yet.
      return false;
    }
  }

  registration_url_.clear();
  root->GetString(kRegistrationUrlAttr, &registration_url_);

  ListValue* setup_content_value = NULL;
  root->GetList(kSetupContentAttr, &setup_content_value);
  if (setup_content_value != NULL) {
    for (ListValue::const_iterator iter = setup_content_value->begin();
         iter != setup_content_value->end();
         ++iter) {
      const DictionaryValue* dict = NULL;
      dict = static_cast<const DictionaryValue*>(*iter);
      DCHECK(dict->GetType() == Value::TYPE_DICTIONARY);
      std::string content_locale;
      if (!dict->GetString(kContentLocaleAttr, &content_locale))
        return false;
      SetupContent content;
      if (!dict->GetString(kHelpPageAttr, &content.help_page_path))
        return false;
      if (!dict->GetString(kEulaPageAttr, &content.eula_page_path))
        return false;
      setup_content_[content_locale] = content;
    }
  }

  return true;
}

const StartupCustomizationDocument::SetupContent*
    StartupCustomizationDocument::GetSetupContent(
        const std::string& locale) const {
  SetupContentMap::const_iterator content_iter = setup_content_.find(locale);
  if (content_iter != setup_content_.end()) {
    return &content_iter->second;
  }
  return NULL;
}

// ServicesCustomizationDocument implementation.

bool ServicesCustomizationDocument::ParseFromJsonValue(
    const DictionaryValue* root) {
  if (!CustomizationDocument::ParseFromJsonValue(root))
    return false;

  // Required app menu settings.
  DictionaryValue* app_menu_value = NULL;
  root->GetDictionary(kAppMenuAttr, &app_menu_value);
  if (app_menu_value == NULL)
    return false;

  app_menu_section_title_.clear();
  if (!app_menu_value->GetString(kSectionTitleAttr,
                                 &app_menu_section_title_))
    return false;
  app_menu_support_page_url_.clear();
  if (!app_menu_value->GetString(kSupportPageAttr,
                                 &app_menu_support_page_url_))
    return false;

  ListValue* web_apps_value = NULL;
  app_menu_value->GetList(kWebAppsAttr, &web_apps_value);
  if (!ParseStringListFromJsonValue(web_apps_value, &web_apps_))
    return false;

  ListValue* extensions_value = NULL;
  app_menu_value->GetList(kExtensionsAttr, &extensions_value);
  if (!ParseStringListFromJsonValue(extensions_value, &extensions_))
    return false;

  // Optional fields.
  initial_start_page_url_.clear();
  root->GetString(kInitialStartPageAttr, &initial_start_page_url_);

  return true;
}

bool ServicesCustomizationDocument::ParseStringListFromJsonValue(
    const ListValue* list_value,
    StringList* string_list) {
  if (list_value == NULL || string_list == NULL)
    return false;
  DCHECK(list_value->GetType() == Value::TYPE_LIST);
  string_list->clear();
  for (ListValue::const_iterator iter = list_value->begin();
       iter != list_value->end();
       ++iter) {
    std::string url;
    if ((*iter)->GetAsString(&url))
      string_list->push_back(url);
  }
  return true;
}

}  // namespace chromeos
