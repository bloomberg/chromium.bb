// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_url_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "extensions/common/error_utils.h"

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace extensions {

namespace {

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

}  // namespace extensions
