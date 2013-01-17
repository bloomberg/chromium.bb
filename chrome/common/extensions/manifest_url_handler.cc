// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_url_handler.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/error_utils.h"

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace extensions {

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
  return extension->UpdatesFromGallery() ?
      GURL(extension_urls::GetWebstoreItemDetailURLPrefix() + extension->id()) :
      GURL::EmptyGURL();
}

// static
const GURL& ManifestURL::GetUpdateURL(const Extension* extension) {
  return GetManifestURL(extension, keys::kUpdateURL);
}

// static
const GURL& ManifestURL::GetOptionsPage(const Extension* extension) {
  return GetManifestURL(extension, keys::kOptionsPage);
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

bool DevToolsPageHandler::Parse(const base::Value* value,
                                Extension* extension,
                                string16* error) {
  scoped_ptr<ManifestURL> manifest_url(new ManifestURL);
  std::string devtools_str;
  if (!value->GetAsString(&devtools_str)) {
    *error = ASCIIToUTF16(errors::kInvalidDevToolsPage);
    return false;
  }
  manifest_url->url_ = extension->GetResourceURL(devtools_str);
  extension->SetManifestData(keys::kDevToolsPage, manifest_url.release());
  return true;
}

HomepageURLHandler::HomepageURLHandler() {
}

HomepageURLHandler::~HomepageURLHandler() {
}

bool HomepageURLHandler::Parse(const base::Value* value,
                               Extension* extension,
                               string16* error) {
  scoped_ptr<ManifestURL> manifest_url(new ManifestURL);
  std::string homepage_url_str;
  if (!value->GetAsString(&homepage_url_str)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidHomepageURL, "");
    return false;
  }
  manifest_url->url_ = GURL(homepage_url_str);
  if (!manifest_url->url_.is_valid() ||
      (!manifest_url->url_.SchemeIs("http") &&
       !manifest_url->url_.SchemeIs("https"))) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidHomepageURL, homepage_url_str);
    return false;
  }
  extension->SetManifestData(keys::kHomepageURL, manifest_url.release());
  return true;
}

UpdateURLHandler::UpdateURLHandler() {
}

UpdateURLHandler::~UpdateURLHandler() {
}

bool UpdateURLHandler::Parse(const base::Value* value,
                             Extension* extension,
                             string16* error) {
  scoped_ptr<ManifestURL> manifest_url(new ManifestURL);
  std::string tmp_update_url;

  if (!value->GetAsString(&tmp_update_url)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidUpdateURL, "");
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

OptionsPageHandler::OptionsPageHandler() {
}

OptionsPageHandler::~OptionsPageHandler() {
}

bool OptionsPageHandler::Parse(const base::Value* value,
                               Extension* extension,
                               string16* error) {
  scoped_ptr<ManifestURL> manifest_url(new ManifestURL);
  std::string options_str;
  if (!value->GetAsString(&options_str)) {
    *error = ASCIIToUTF16(errors::kInvalidOptionsPage);
    return false;
  }

  if (extension->is_hosted_app()) {
    // hosted apps require an absolute URL.
    GURL options_url(options_str);
    if (!options_url.is_valid() ||
        !(options_url.SchemeIs("http") || options_url.SchemeIs("https"))) {
      *error = ASCIIToUTF16(errors::kInvalidOptionsPageInHostedApp);
      return false;
    }
    manifest_url->url_ = options_url;
  } else {
    GURL absolute(options_str);
    if (absolute.is_valid()) {
      *error = ASCIIToUTF16(errors::kInvalidOptionsPageExpectUrlInPackage);
      return false;
    }
    manifest_url->url_ = extension->GetResourceURL(options_str);
    if (!manifest_url->url_.is_valid()) {
      *error = ASCIIToUTF16(errors::kInvalidOptionsPage);
      return false;
    }
  }

  extension->SetManifestData(keys::kOptionsPage, manifest_url.release());
  return true;
}

URLOverridesHandler::URLOverridesHandler() {
}

URLOverridesHandler::~URLOverridesHandler() {
}

bool URLOverridesHandler::Parse(const base::Value* value,
                           Extension* extension,
                           string16* error) {
  const DictionaryValue* overrides = NULL;
  if (!value->GetAsDictionary(&overrides)) {
    *error = ASCIIToUTF16(errors::kInvalidChromeURLOverrides);
    return false;
  }
  scoped_ptr<URLOverrides> url_overrides(new URLOverrides);
  // Validate that the overrides are all strings
  for (DictionaryValue::key_iterator iter = overrides->begin_keys();
       iter != overrides->end_keys(); ++iter) {
    std::string page = *iter;
    std::string val;
    // Restrict override pages to a list of supported URLs.
    bool is_override = (page != chrome::kChromeUINewTabHost &&
                        page != chrome::kChromeUIBookmarksHost &&
                        page != chrome::kChromeUIHistoryHost);
#if defined(OS_CHROMEOS)
    is_override = (is_override &&
                   page != chrome::kChromeUIActivationMessageHost);
#endif
#if defined(FILE_MANAGER_EXTENSION)
    is_override = (is_override &&
                   !(extension->location() == Extension::COMPONENT &&
                     page == chrome::kChromeUIFileManagerHost));
#endif

    if (is_override || !overrides->GetStringWithoutPathExpansion(*iter, &val)) {
      *error = ASCIIToUTF16(errors::kInvalidChromeURLOverrides);
      return false;
    }
    // Replace the entry with a fully qualified chrome-extension:// URL.
    url_overrides->chrome_url_overrides_[page] = extension->GetResourceURL(val);

    // For component extensions, add override URL to extent patterns.
    if (extension->is_legacy_packaged_app() &&
        extension->location() == Extension::COMPONENT) {
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
    *error = ASCIIToUTF16(errors::kMultipleOverrides);
    return false;
  }
  extension->SetManifestData(keys::kChromeURLOverrides,
                             url_overrides.release());
  return true;
}

}  // namespace extensions
