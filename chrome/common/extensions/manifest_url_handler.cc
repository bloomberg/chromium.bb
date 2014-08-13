// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_url_handler.h"

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_AURA)
#include "ui/keyboard/keyboard_constants.h"
#endif

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

const char kOverrideExtentUrlPatternFormat[] = "chrome://%s/*";

const GURL& GetManifestURL(const Extension* extension,
                           const std::string& key) {
  ManifestURL* manifest_url =
      static_cast<ManifestURL*>(extension->GetManifestData(key));
  return manifest_url ? manifest_url->url_ : GURL::EmptyGURL();
}

}  // namespace

// static
const GURL& ManifestURL::GetDevToolsPage(const Extension* extension) {
  return GetManifestURL(extension, keys::kDevToolsPage);
}

// static
const GURL ManifestURL::GetHomepageURL(const Extension* extension) {
  const GURL& homepage_url = GetManifestURL(extension, keys::kHomepageURL);
  if (homepage_url.is_valid())
    return homepage_url;
  return UpdatesFromGallery(extension) ?
      GURL(extension_urls::GetWebstoreItemDetailURLPrefix() + extension->id()) :
      GURL::EmptyGURL();
}

// static
const GURL& ManifestURL::GetUpdateURL(const Extension* extension) {
  return GetManifestURL(extension, keys::kUpdateURL);
}

// static
bool ManifestURL::UpdatesFromGallery(const Extension* extension) {
  return extension_urls::IsWebstoreUpdateUrl(GetUpdateURL(extension));
}

// static
bool  ManifestURL::UpdatesFromGallery(const base::DictionaryValue* manifest) {
  std::string url;
  if (!manifest->GetString(keys::kUpdateURL, &url))
    return false;
  return extension_urls::IsWebstoreUpdateUrl(GURL(url));
}

// static
const GURL& ManifestURL::GetOptionsPage(const Extension* extension) {
  return GetManifestURL(extension, keys::kOptionsPage);
}

// static
const GURL& ManifestURL::GetAboutPage(const Extension* extension) {
  return GetManifestURL(extension, keys::kAboutPage);
}

// static
const GURL ManifestURL::GetDetailsURL(const Extension* extension) {
  return extension->from_webstore() ?
      GURL(extension_urls::GetWebstoreItemDetailURLPrefix() + extension->id()) :
      GURL::EmptyGURL();
}

URLOverrides::URLOverrides() {
}

URLOverrides::~URLOverrides() {
}

static base::LazyInstance<URLOverrides::URLOverrideMap> g_empty_url_overrides =
    LAZY_INSTANCE_INITIALIZER;

// static
const URLOverrides::URLOverrideMap&
    URLOverrides::GetChromeURLOverrides(const Extension* extension) {
  URLOverrides* url_overrides = static_cast<URLOverrides*>(
      extension->GetManifestData(keys::kChromeURLOverrides));
  return url_overrides ?
         url_overrides->chrome_url_overrides_ :
         g_empty_url_overrides.Get();
}

DevToolsPageHandler::DevToolsPageHandler() {
}

DevToolsPageHandler::~DevToolsPageHandler() {
}

bool DevToolsPageHandler::Parse(Extension* extension, base::string16* error) {
  scoped_ptr<ManifestURL> manifest_url(new ManifestURL);
  std::string devtools_str;
  if (!extension->manifest()->GetString(keys::kDevToolsPage, &devtools_str)) {
    *error = base::ASCIIToUTF16(errors::kInvalidDevToolsPage);
    return false;
  }
  manifest_url->url_ = extension->GetResourceURL(devtools_str);
  extension->SetManifestData(keys::kDevToolsPage, manifest_url.release());
  PermissionsParser::AddAPIPermission(extension, APIPermission::kDevtools);
  return true;
}

const std::vector<std::string> DevToolsPageHandler::Keys() const {
  return SingleKey(keys::kDevToolsPage);
}

HomepageURLHandler::HomepageURLHandler() {
}

HomepageURLHandler::~HomepageURLHandler() {
}

bool HomepageURLHandler::Parse(Extension* extension, base::string16* error) {
  scoped_ptr<ManifestURL> manifest_url(new ManifestURL);
  std::string homepage_url_str;
  if (!extension->manifest()->GetString(keys::kHomepageURL,
                                        &homepage_url_str)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidHomepageURL,
                                                 std::string());
    return false;
  }
  manifest_url->url_ = GURL(homepage_url_str);
  if (!manifest_url->url_.is_valid() ||
      !manifest_url->url_.SchemeIsHTTPOrHTTPS()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidHomepageURL, homepage_url_str);
    return false;
  }
  extension->SetManifestData(keys::kHomepageURL, manifest_url.release());
  return true;
}

const std::vector<std::string> HomepageURLHandler::Keys() const {
  return SingleKey(keys::kHomepageURL);
}

UpdateURLHandler::UpdateURLHandler() {
}

UpdateURLHandler::~UpdateURLHandler() {
}

bool UpdateURLHandler::Parse(Extension* extension, base::string16* error) {
  scoped_ptr<ManifestURL> manifest_url(new ManifestURL);
  std::string tmp_update_url;

  if (!extension->manifest()->GetString(keys::kUpdateURL, &tmp_update_url)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidUpdateURL,
                                                 std::string());
    return false;
  }

  manifest_url->url_ = GURL(tmp_update_url);
  if (!manifest_url->url_.is_valid() ||
      manifest_url->url_.has_ref()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidUpdateURL, tmp_update_url);
    return false;
  }

  extension->SetManifestData(keys::kUpdateURL, manifest_url.release());
  return true;
}

const std::vector<std::string> UpdateURLHandler::Keys() const {
  return SingleKey(keys::kUpdateURL);
}

OptionsPageHandler::OptionsPageHandler() {
}

OptionsPageHandler::~OptionsPageHandler() {
}

bool OptionsPageHandler::Parse(Extension* extension, base::string16* error) {
  scoped_ptr<ManifestURL> manifest_url(new ManifestURL);
  std::string options_str;
  if (!extension->manifest()->GetString(keys::kOptionsPage, &options_str)) {
    *error = base::ASCIIToUTF16(errors::kInvalidOptionsPage);
    return false;
  }

  if (extension->is_hosted_app()) {
    // hosted apps require an absolute URL.
    GURL options_url(options_str);
    if (!options_url.is_valid() ||
        !options_url.SchemeIsHTTPOrHTTPS()) {
      *error = base::ASCIIToUTF16(errors::kInvalidOptionsPageInHostedApp);
      return false;
    }
    manifest_url->url_ = options_url;
  } else {
    GURL absolute(options_str);
    if (absolute.is_valid()) {
      *error =
          base::ASCIIToUTF16(errors::kInvalidOptionsPageExpectUrlInPackage);
      return false;
    }
    manifest_url->url_ = extension->GetResourceURL(options_str);
    if (!manifest_url->url_.is_valid()) {
      *error = base::ASCIIToUTF16(errors::kInvalidOptionsPage);
      return false;
    }
  }

  extension->SetManifestData(keys::kOptionsPage, manifest_url.release());
  return true;
}

bool OptionsPageHandler::Validate(const Extension* extension,
                                  std::string* error,
                                  std::vector<InstallWarning>* warnings) const {
  // Validate path to the options page.  Don't check the URL for hosted apps,
  // because they are expected to refer to an external URL.
  if (!extensions::ManifestURL::GetOptionsPage(extension).is_empty() &&
      !extension->is_hosted_app()) {
    const base::FilePath options_path =
        extensions::file_util::ExtensionURLToRelativeFilePath(
            extensions::ManifestURL::GetOptionsPage(extension));
    const base::FilePath path =
        extension->GetResource(options_path).GetFilePath();
    if (path.empty() || !base::PathExists(path)) {
      *error =
          l10n_util::GetStringFUTF8(
              IDS_EXTENSION_LOAD_OPTIONS_PAGE_FAILED,
              options_path.LossyDisplayName());
      return false;
    }
  }
  return true;
}

const std::vector<std::string> OptionsPageHandler::Keys() const {
  return SingleKey(keys::kOptionsPage);
}

AboutPageHandler::AboutPageHandler() {
}

AboutPageHandler::~AboutPageHandler() {
}

bool AboutPageHandler::Parse(Extension* extension, base::string16* error) {
  scoped_ptr<ManifestURL> manifest_url(new ManifestURL);
  std::string about_str;
  if (!extension->manifest()->GetString(keys::kAboutPage, &about_str)) {
    *error = base::ASCIIToUTF16(errors::kInvalidAboutPage);
    return false;
  }

  GURL absolute(about_str);
  if (absolute.is_valid()) {
    *error = base::ASCIIToUTF16(errors::kInvalidAboutPageExpectRelativePath);
    return false;
  }
  manifest_url->url_ = extension->GetResourceURL(about_str);
  if (!manifest_url->url_.is_valid()) {
    *error = base::ASCIIToUTF16(errors::kInvalidAboutPage);
    return false;
  }
  extension->SetManifestData(keys::kAboutPage, manifest_url.release());
  return true;
}

bool AboutPageHandler::Validate(const Extension* extension,
                                std::string* error,
                                std::vector<InstallWarning>* warnings) const {
  // Validate path to the options page.
  if (!extensions::ManifestURL::GetAboutPage(extension).is_empty()) {
    const base::FilePath about_path =
        extensions::file_util::ExtensionURLToRelativeFilePath(
            extensions::ManifestURL::GetAboutPage(extension));
    const base::FilePath path =
        extension->GetResource(about_path).GetFilePath();
    if (path.empty() || !base::PathExists(path)) {
      *error = l10n_util::GetStringFUTF8(IDS_EXTENSION_LOAD_ABOUT_PAGE_FAILED,
                                         about_path.LossyDisplayName());
      return false;
    }
  }
  return true;
}

const std::vector<std::string> AboutPageHandler::Keys() const {
  return SingleKey(keys::kAboutPage);
}

URLOverridesHandler::URLOverridesHandler() {
}

URLOverridesHandler::~URLOverridesHandler() {
}

bool URLOverridesHandler::Parse(Extension* extension, base::string16* error) {
  const base::DictionaryValue* overrides = NULL;
  if (!extension->manifest()->GetDictionary(keys::kChromeURLOverrides,
                                            &overrides)) {
    *error = base::ASCIIToUTF16(errors::kInvalidChromeURLOverrides);
    return false;
  }
  scoped_ptr<URLOverrides> url_overrides(new URLOverrides);
  // Validate that the overrides are all strings
  for (base::DictionaryValue::Iterator iter(*overrides); !iter.IsAtEnd();
         iter.Advance()) {
    std::string page = iter.key();
    std::string val;
    // Restrict override pages to a list of supported URLs.
    bool is_override = (page != chrome::kChromeUINewTabHost &&
                        page != chrome::kChromeUIBookmarksHost &&
                        page != chrome::kChromeUIHistoryHost);
#if defined(OS_CHROMEOS)
    is_override = (is_override &&
                   page != chrome::kChromeUIActivationMessageHost);
#endif
#if defined(OS_CHROMEOS)
    is_override = (is_override && page != keyboard::kKeyboardHost);
#endif

    if (is_override || !iter.value().GetAsString(&val)) {
      *error = base::ASCIIToUTF16(errors::kInvalidChromeURLOverrides);
      return false;
    }
    // Replace the entry with a fully qualified chrome-extension:// URL.
    url_overrides->chrome_url_overrides_[page] = extension->GetResourceURL(val);

    // For component extensions, add override URL to extent patterns.
    if (extension->is_legacy_packaged_app() &&
        extension->location() == Manifest::COMPONENT) {
      URLPattern pattern(URLPattern::SCHEME_CHROMEUI);
      std::string url = base::StringPrintf(kOverrideExtentUrlPatternFormat,
                                           page.c_str());
      if (pattern.Parse(url) != URLPattern::PARSE_SUCCESS) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidURLPatternError, url);
        return false;
      }
      extension->AddWebExtentPattern(pattern);
    }
  }

  // An extension may override at most one page.
  if (overrides->size() > 1) {
    *error = base::ASCIIToUTF16(errors::kMultipleOverrides);
    return false;
  }
  extension->SetManifestData(keys::kChromeURLOverrides,
                             url_overrides.release());
  return true;
}

const std::vector<std::string> URLOverridesHandler::Keys() const {
  return SingleKey(keys::kChromeURLOverrides);
}

}  // namespace extensions
