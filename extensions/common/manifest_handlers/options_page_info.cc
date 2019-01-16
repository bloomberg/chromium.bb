// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/options_page_info.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

using api::extensions_manifest_types::OptionsUI;

namespace {

OptionsPageInfo* GetOptionsPageInfo(const Extension* extension) {
  return static_cast<OptionsPageInfo*>(
      extension->GetManifestData(keys::kOptionsUI));
}

// Parses |url_string| into a GURL |result| if it is a valid options page for
// this app/extension. If not, it returns the reason in |error|. Because this
// handles URLs for both "options_page" and "options_ui.page", the name of the
// manifest field must be provided in |manifest_field_name|.
bool ParseOptionsUrl(Extension* extension,
                     const std::string& url_string,
                     const std::string& manifest_field_name,
                     base::string16* error,
                     GURL* result) {
  if (extension->is_hosted_app()) {
    // Hosted apps require an absolute URL.
    GURL options_url(url_string);
    if (!options_url.is_valid() || !options_url.SchemeIsHTTPOrHTTPS()) {
      *error = base::ASCIIToUTF16(errors::kInvalidOptionsPageInHostedApp);
      return false;
    }
    *result = options_url;
    return true;
  }

  // Otherwise the options URL should be inside the extension.
  if (GURL(url_string).is_valid()) {
    *error = base::ASCIIToUTF16(errors::kInvalidOptionsPageExpectUrlInPackage);
    return false;
  }

  GURL resource_url = extension->GetResourceURL(url_string);
  if (!resource_url.is_valid()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidOptionsPage,
                                                 manifest_field_name);
    return false;
  }
  *result = resource_url;
  return true;
}

}  // namespace

OptionsPageInfo::OptionsPageInfo(const GURL& options_page,
                                 bool chrome_styles,
                                 bool open_in_tab)
    : options_page_(options_page),
      chrome_styles_(chrome_styles),
      open_in_tab_(open_in_tab) {
}

OptionsPageInfo::~OptionsPageInfo() {
}

// static
const GURL& OptionsPageInfo::GetOptionsPage(const Extension* extension) {
  OptionsPageInfo* info = GetOptionsPageInfo(extension);
  return info ? info->options_page_ : GURL::EmptyGURL();
}

// static
bool OptionsPageInfo::HasOptionsPage(const Extension* extension) {
  return !OptionsPageInfo::GetOptionsPage(extension).is_empty();
}

// static
bool OptionsPageInfo::ShouldUseChromeStyle(const Extension* extension) {
  OptionsPageInfo* info = GetOptionsPageInfo(extension);
  return info && info->chrome_styles_;
}

// static
bool OptionsPageInfo::ShouldOpenInTab(const Extension* extension) {
  OptionsPageInfo* info = GetOptionsPageInfo(extension);
  return info && info->open_in_tab_;
}

std::unique_ptr<OptionsPageInfo> OptionsPageInfo::Create(
    Extension* extension,
    const base::Value* options_ui_value,
    const std::string& options_page_string,
    std::vector<InstallWarning>* install_warnings,
    base::string16* error) {
  GURL options_page;
  // Chrome styling is always opt-in.
  bool chrome_style = false;
  // Extensions can opt in or out to opening in a tab, and users can choose via
  // the --embedded-extension-options flag which should be the default.
  bool open_in_tab = !FeatureSwitch::embedded_extension_options()->IsEnabled();

  // Parse the options_ui object.
  if (options_ui_value) {
    base::string16 options_ui_error;

    std::unique_ptr<OptionsUI> options_ui =
        OptionsUI::FromValue(*options_ui_value, &options_ui_error);
    if (!options_ui_error.empty()) {
      // OptionsUI::FromValue populates |error| both when there are
      // errors (in which case |options_ui| will be NULL) and warnings
      // (in which case |options_ui| will be valid). Either way, show it
      // as an install warning.
      install_warnings->push_back(
          InstallWarning(base::UTF16ToASCII(options_ui_error)));
    }

    if (options_ui) {
      base::string16 options_parse_error;
      if (!ParseOptionsUrl(extension,
                           options_ui->page,
                           keys::kOptionsUI,
                           &options_parse_error,
                           &options_page)) {
        install_warnings->push_back(
            InstallWarning(base::UTF16ToASCII(options_parse_error)));
      }
      if (options_ui->chrome_style.get())
        chrome_style = *options_ui->chrome_style;
      open_in_tab = false;
      if (options_ui->open_in_tab.get())
        open_in_tab = *options_ui->open_in_tab;
    }
  }

  // Parse the legacy options_page entry if there was no entry for
  // options_ui.page.
  if (!options_page_string.empty() && !options_page.is_valid()) {
    if (!ParseOptionsUrl(extension,
                         options_page_string,
                         keys::kOptionsPage,
                         error,
                         &options_page)) {
      return nullptr;
    }
  }

  return std::make_unique<OptionsPageInfo>(options_page, chrome_style,
                                           open_in_tab);
}

OptionsPageManifestHandler::OptionsPageManifestHandler() {
}

OptionsPageManifestHandler::~OptionsPageManifestHandler() {
}

bool OptionsPageManifestHandler::Parse(Extension* extension,
                                       base::string16* error) {
  std::vector<InstallWarning> install_warnings;
  const Manifest* manifest = extension->manifest();

  std::string options_page_string;
  if (manifest->HasPath(keys::kOptionsPage) &&
      !manifest->GetString(keys::kOptionsPage, &options_page_string)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidOptionsPage,
                                                 keys::kOptionsPage);
    return false;
  }

  const base::Value* options_ui_value = NULL;
  ignore_result(manifest->Get(keys::kOptionsUI, &options_ui_value));

  std::unique_ptr<OptionsPageInfo> info =
      OptionsPageInfo::Create(extension, options_ui_value, options_page_string,
                              &install_warnings, error);
  if (!info)
    return false;

  extension->AddInstallWarnings(std::move(install_warnings));
  extension->SetManifestData(keys::kOptionsUI, std::move(info));
  return true;
}

bool OptionsPageManifestHandler::Validate(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  // Validate path to the options page.  Don't check the URL for hosted apps,
  // because they are expected to refer to an external URL.
  if (!OptionsPageInfo::HasOptionsPage(extension) || extension->is_hosted_app())
    return true;

  base::FilePath options_path = file_util::ExtensionURLToRelativeFilePath(
      OptionsPageInfo::GetOptionsPage(extension));
  base::FilePath path = extension->GetResource(options_path).GetFilePath();
  if (path.empty() || !base::PathExists(path)) {
    *error = l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_OPTIONS_PAGE_FAILED,
                                       options_path.LossyDisplayName());
    return false;
  }
  return true;
}

base::span<const char* const> OptionsPageManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kOptionsPage, keys::kOptionsUI};
  return kKeys;
}

}  // namespace extensions
