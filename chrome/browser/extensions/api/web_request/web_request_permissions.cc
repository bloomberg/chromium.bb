// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_request/web_request_permissions.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

namespace {

// Returns true if the URL is sensitive and requests to this URL must not be
// modified/canceled by extensions, e.g. because it is targeted to the webstore
// to check for updates, extension blacklisting, etc.
bool IsSensitiveURL(const GURL& url) {
  // TODO(battre) Merge this, CanExtensionAccessURL of web_request_api.cc and
  // Extension::CanExecuteScriptOnPage into one function.
  bool is_webstore_gallery_url =
      StartsWithASCII(url.spec(), extension_urls::kGalleryBrowsePrefix, true);
  bool sensitive_chrome_url = false;
  if (EndsWith(url.host(), "google.com", true)) {
    sensitive_chrome_url |= (url.host() == "www.google.com") &&
                            StartsWithASCII(url.path(), "/chrome", true);
    sensitive_chrome_url |= (url.host() == "chrome.google.com");
    if (StartsWithASCII(url.host(), "client", true)) {
      for (int i = 0; i < 10; ++i) {
        sensitive_chrome_url |=
            (StringPrintf("client%d.google.com", i) == url.host());
      }
    }
  }
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  GURL url_without_query = url.ReplaceComponents(replacements);
  return is_webstore_gallery_url || sensitive_chrome_url ||
      extension_urls::IsWebstoreUpdateUrl(url_without_query) ||
      extension_urls::IsBlacklistUpdateUrl(url);
}

// Returns true if the scheme is one we want to allow extensions to have access
// to. Extensions still need specific permissions for a given URL, which is
// covered by CanExtensionAccessURL.
bool HasWebRequestScheme(const GURL& url) {
  return (url.SchemeIs(chrome::kAboutScheme) ||
          url.SchemeIs(chrome::kFileScheme) ||
          url.SchemeIs(chrome::kFileSystemScheme) ||
          url.SchemeIs(chrome::kFtpScheme) ||
          url.SchemeIs(chrome::kHttpScheme) ||
          url.SchemeIs(chrome::kHttpsScheme) ||
          url.SchemeIs(chrome::kExtensionScheme));
}

}  // namespace

// static
bool WebRequestPermissions::HideRequest(const net::URLRequest* request) {
  const GURL& url = request->url();
  const GURL& first_party_url = request->first_party_for_cookies();
  bool hide = false;
  if (first_party_url.is_valid()) {
    hide = IsSensitiveURL(first_party_url) ||
           !HasWebRequestScheme(first_party_url);
  }
  if (!hide)
    hide = IsSensitiveURL(url) || !HasWebRequestScheme(url);
  return hide;
}

// static
bool WebRequestPermissions::CanExtensionAccessURL(
    const ExtensionInfoMap* extension_info_map,
    const std::string& extension_id,
    const GURL& url,
    bool crosses_incognito,
    bool enforce_host_permissions) {
  // extension_info_map can be NULL in testing.
  if (!extension_info_map)
    return true;

  const extensions::Extension* extension =
      extension_info_map->extensions().GetByID(extension_id);
  if (!extension)
    return false;

  // Check if this event crosses incognito boundaries when it shouldn't.
  if (crosses_incognito && !extension_info_map->CanCrossIncognito(extension))
    return false;

  if (enforce_host_permissions) {
    // about: URLs are not covered in host permissions, but are allowed anyway.
    bool host_permissions_ok = (url.SchemeIs(chrome::kAboutScheme) ||
                                extension->HasHostPermission(url) ||
                                url.GetOrigin() == extension->url());
    if (!host_permissions_ok)
      return false;
  }

  return true;
}
