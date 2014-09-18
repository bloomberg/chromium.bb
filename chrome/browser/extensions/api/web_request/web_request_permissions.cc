// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_request/web_request_permissions.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

using content::ResourceRequestInfo;

namespace {

// Returns true if the URL is sensitive and requests to this URL must not be
// modified/canceled by extensions, e.g. because it is targeted to the webstore
// to check for updates, extension blacklisting, etc.
bool IsSensitiveURL(const GURL& url) {
  // TODO(battre) Merge this, CanExtensionAccessURL and
  // PermissionsData::CanAccessPage into one function.
  bool sensitive_chrome_url = false;
  const std::string host = url.host();
  const char kGoogleCom[] = ".google.com";
  const char kClient[] = "clients";
  if (EndsWith(host, kGoogleCom, true)) {
    // Check for "clients[0-9]*.google.com" hosts.
    // This protects requests to several internal services such as sync,
    // extension update pings, captive portal detection, fraudulent certificate
    // reporting, autofill and others.
    if (StartsWithASCII(host, kClient, true)) {
      bool match = true;
      for (std::string::const_iterator i = host.begin() + strlen(kClient),
               end = host.end() - strlen(kGoogleCom); i != end; ++i) {
        if (!isdigit(*i)) {
          match = false;
          break;
        }
      }
      sensitive_chrome_url = sensitive_chrome_url || match;
    }
    // This protects requests to safe browsing, link doctor, and possibly
    // others.
    sensitive_chrome_url = sensitive_chrome_url ||
        EndsWith(url.host(), ".clients.google.com", true) ||
        url.host() == "sb-ssl.google.com" ||
        (url.host() ==  "chrome.google.com" &&
             StartsWithASCII(url.path(), "/webstore", true));
  }
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  GURL url_without_query = url.ReplaceComponents(replacements);
  return sensitive_chrome_url ||
      extension_urls::IsWebstoreUpdateUrl(url_without_query) ||
      extension_urls::IsBlacklistUpdateUrl(url);
}

// Returns true if the scheme is one we want to allow extensions to have access
// to. Extensions still need specific permissions for a given URL, which is
// covered by CanExtensionAccessURL.
bool HasWebRequestScheme(const GURL& url) {
  return (url.SchemeIs(url::kAboutScheme) || url.SchemeIs(url::kFileScheme) ||
          url.SchemeIs(url::kFileSystemScheme) ||
          url.SchemeIs(url::kFtpScheme) || url.SchemeIs(url::kHttpScheme) ||
          url.SchemeIs(url::kHttpsScheme) ||
          url.SchemeIs(extensions::kExtensionScheme));
}

}  // namespace

// static
bool WebRequestPermissions::HideRequest(
    const extensions::InfoMap* extension_info_map,
    const net::URLRequest* request) {
  // Hide requests from the Chrome WebStore App or signin process.
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (info) {
    int process_id = info->GetChildID();
    // Never hide requests from guest processes.
    if (extensions::WebViewRendererState::GetInstance()->IsGuest(process_id))
      return false;

    if (extension_info_map && (
        extension_info_map->IsSigninProcess(process_id) ||
        extension_info_map->process_map().Contains(
            extensions::kWebStoreAppId, process_id))) {
      return true;
    }
  }

  const GURL& url = request->url();
  return IsSensitiveURL(url) || !HasWebRequestScheme(url);
}

// static
bool WebRequestPermissions::CanExtensionAccessURL(
    const extensions::InfoMap* extension_info_map,
    const std::string& extension_id,
    const GURL& url,
    bool crosses_incognito,
    HostPermissionsCheck host_permissions_check) {
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

  switch (host_permissions_check) {
    case DO_NOT_CHECK_HOST:
      break;
    case REQUIRE_HOST_PERMISSION:
      // about: URLs are not covered in host permissions, but are allowed
      // anyway.
      if (!((url.SchemeIs(url::kAboutScheme) ||
             extension->permissions_data()->HasHostPermission(url) ||
             url.GetOrigin() == extension->url()))) {
        return false;
      }
      break;
    case REQUIRE_ALL_URLS:
      if (!extension->permissions_data()->HasEffectiveAccessToAllHosts())
        return false;
      break;
  }

  return true;
}
