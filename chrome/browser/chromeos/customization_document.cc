// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/customization_document.h"

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/system_access.h"

// Manifest attributes names.

namespace {

const char kVersionAttr[] = "version";
const char kDefaultAttr[] = "default";
const char kInitialLocaleAttr[] = "initial_locale";
const char kInitialTimezoneAttr[] = "initial_timezone";
const char kKeyboardLayoutAttr[] = "keyboard_layout";
const char kRegistrationUrlAttr[] = "registration_url";
const char kHwidMapAttr[] = "hwid_map";
const char kHwidMaskAttr[] = "hwid_mask";
const char kSetupContentAttr[] = "setup_content";
const char kHelpPageAttr[] = "help_page";
const char kEulaPageAttr[] = "eula_page";
const char kAppContentAttr[] = "app_content";
const char kInitialStartPageAttr[] = "initial_start_page";
const char kSupportPageAttr[] = "support_page";

const char kAcceptedManifestVersion[] = "1.0";

const char kHwid[] = "hwid";

}  // anonymous namespace

namespace chromeos {

// CustomizationDocument implementation.
bool CustomizationDocument::LoadManifestFromFile(
    const FilePath& manifest_path) {
  std::string manifest;
  if (!file_util::ReadFileToString(manifest_path, &manifest))
    return false;
  return LoadManifestFromString(manifest);
}

bool CustomizationDocument::LoadManifestFromString(
    const std::string& manifest) {
  scoped_ptr<Value> root(base::JSONReader::Read(manifest, true));
  DCHECK(root.get() != NULL);
  if (root.get() == NULL)
    return false;
  DCHECK(root->GetType() == Value::TYPE_DICTIONARY);
  if (root->GetType() == Value::TYPE_DICTIONARY) {
    root_.reset(static_cast<DictionaryValue*>(root.release()));
    std::string result;
    if (root_->GetString(kVersionAttr, &result) &&
        result == kAcceptedManifestVersion)
      return true;

    LOG(ERROR) << "Wrong customization manifest version";
    root_.reset(NULL);
  }
  return false;
}

std::string CustomizationDocument::GetLocaleSpecificString(
    const std::string& locale,
    const std::string& dictionary_name,
    const std::string& entry_name) const {
  DictionaryValue* dictionary_content = NULL;
  if (!root_->GetDictionary(dictionary_name, &dictionary_content))
    return std::string();

  DictionaryValue* locale_dictionary = NULL;
  if (dictionary_content->GetDictionary(locale, &locale_dictionary)) {
    std::string result;
    if (locale_dictionary->GetString(entry_name, &result))
      return result;
  }

  DictionaryValue* default_dictionary = NULL;
  if (dictionary_content->GetDictionary(kDefaultAttr, &default_dictionary)) {
    std::string result;
    if (default_dictionary->GetString(entry_name, &result))
      return result;
  }

  return std::string();
}

// StartupCustomizationDocument implementation.
StartupCustomizationDocument::StartupCustomizationDocument(
    SystemAccess* system_access)
    : system_access_(system_access) {
}

bool StartupCustomizationDocument::LoadManifestFromString(
    const std::string& manifest) {
  DCHECK(system_access_);

  if (!CustomizationDocument::LoadManifestFromString(manifest)) {
    return false;
  }

  root_->GetString(kInitialLocaleAttr, &initial_locale_);
  root_->GetString(kInitialTimezoneAttr, &initial_timezone_);
  root_->GetString(kKeyboardLayoutAttr, &keyboard_layout_);
  root_->GetString(kRegistrationUrlAttr, &registration_url_);

  std::string hwid;
  if (system_access_->GetMachineStatistic(kHwid, &hwid)) {
    ListValue* hwid_list = NULL;
    if (root_->GetList(kHwidMapAttr, &hwid_list)) {
      for (size_t i = 0; i < hwid_list->GetSize(); ++i) {
        DictionaryValue* hwid_dictionary = NULL;
        std::string hwid_mask;
        if (hwid_list->GetDictionary(i, &hwid_dictionary) &&
            hwid_dictionary->GetString(kHwidMaskAttr, &hwid_mask)) {
          if (MatchPattern(hwid, hwid_mask)) {
            // If HWID for this machine matches some mask, use HWID specific
            // settings.
            std::string result;
            if (hwid_dictionary->GetString(kInitialLocaleAttr, &result))
              initial_locale_ = result;

            if (hwid_dictionary->GetString(kInitialTimezoneAttr, &result))
              initial_timezone_ = result;

            if (hwid_dictionary->GetString(kKeyboardLayoutAttr, &result))
              keyboard_layout_ = result;
          }
          // Don't break here to allow other entires to be applied if match.
        } else {
          LOG(ERROR) << "Syntax error in customization manifest";
        }
      }
    }
  } else {
    LOG(ERROR) << "HWID is missing in machine statistics";
  }

  system_access_->GetMachineStatistic(kInitialLocaleAttr, &initial_locale_);
  system_access_->GetMachineStatistic(kInitialTimezoneAttr, &initial_timezone_);
  system_access_->GetMachineStatistic(kKeyboardLayoutAttr, &keyboard_layout_);

  // system_access_ is no longer used.
  system_access_ = NULL;

  return true;
}

std::string StartupCustomizationDocument::GetHelpPage(
    const std::string& locale) const {
  return GetLocaleSpecificString(locale, kSetupContentAttr, kHelpPageAttr);
}

std::string StartupCustomizationDocument::GetEULAPage(
    const std::string& locale) const {
  return GetLocaleSpecificString(locale, kSetupContentAttr, kEulaPageAttr);
}

// ServicesCustomizationDocument implementation.
std::string ServicesCustomizationDocument::GetInitialStartPage(
    const std::string& locale) const {
  return GetLocaleSpecificString(
      locale, kAppContentAttr, kInitialStartPageAttr);
}

std::string ServicesCustomizationDocument::GetSupportPage(
    const std::string& locale) const {
  return GetLocaleSpecificString(
      locale, kAppContentAttr, kSupportPageAttr);
}

}  // namespace chromeos
