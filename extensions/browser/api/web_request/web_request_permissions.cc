// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_permissions.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/login/login_state.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state.h"
#endif  // defined(OS_CHROMEOS)

using content::ResourceRequestInfo;
using extensions::PermissionsData;

namespace {

// Returns true if the scheme is one we want to allow extensions to have access
// to. Extensions still need specific permissions for a given URL, which is
// covered by CanExtensionAccessURL.
bool HasWebRequestScheme(const GURL& url) {
  return (url.SchemeIs(url::kAboutScheme) || url.SchemeIs(url::kFileScheme) ||
          url.SchemeIs(url::kFileSystemScheme) ||
          url.SchemeIs(url::kFtpScheme) || url.SchemeIsHTTPOrHTTPS() ||
          url.SchemeIs(extensions::kExtensionScheme) || url.SchemeIsWSOrWSS());
}

bool g_allow_all_extension_locations_in_public_session = false;

}  // namespace

// Returns true if the URL is sensitive and requests to this URL must not be
// modified/canceled by extensions, e.g. because it is targeted to the webstore
// to check for updates, extension blacklisting, etc.
bool IsSensitiveURL(const GURL& url,
                    bool is_request_from_browser_or_webui_renderer) {
  // TODO(battre) Merge this, CanExtensionAccessURL and
  // PermissionsData::CanAccessPage into one function.
  bool sensitive_chrome_url = false;
  const char kGoogleCom[] = "google.com";
  const char kClient[] = "clients";
  url::Origin origin(url);
  if (origin.DomainIs(kGoogleCom)) {
    base::StringPiece host = url.host_piece();
    while (host.ends_with("."))
      host.remove_suffix(1u);
    // Check for "clients[0-9]*.google.com" hosts.
    // This protects requests to several internal services such as sync,
    // extension update pings, captive portal detection, fraudulent certificate
    // reporting, autofill and others.
    //
    // These URLs are only protected for requests from the browser and webui
    // renderers, not for requests from common renderers, because
    // clients*.google.com are also used by websites.
    if (is_request_from_browser_or_webui_renderer) {
      base::StringPiece::size_type pos = host.rfind(kClient);
      if (pos != base::StringPiece::npos) {
        bool match = true;
        if (pos > 0 && host[pos - 1] != '.') {
          match = false;
        } else {
          for (base::StringPiece::const_iterator
                   i = host.begin() + pos + strlen(kClient),
                   end = host.end() - (strlen(kGoogleCom) + 1);
               i != end; ++i) {
            if (!isdigit(*i)) {
              match = false;
              break;
            }
          }
        }
        sensitive_chrome_url = sensitive_chrome_url || match;
      }
    }

    // Safebrowsing and Chrome Webstore URLs are always protected, i.e. also
    // for requests from common renderers.
    sensitive_chrome_url = sensitive_chrome_url ||
                           (url.DomainIs("chrome.google.com") &&
                            base::StartsWith(url.path_piece(), "/webstore",
                                             base::CompareCase::SENSITIVE));
  }
  return sensitive_chrome_url || extension_urls::IsWebstoreUpdateUrl(url) ||
         extension_urls::IsBlacklistUpdateUrl(url) ||
         extension_urls::IsSafeBrowsingUrl(origin, url.path_piece());
}

// static
bool WebRequestPermissions::HideRequest(
    const extensions::InfoMap* extension_info_map,
    const net::URLRequest* request,
    extensions::ExtensionNavigationUIData* navigation_ui_data) {
  // Hide requests from the Chrome WebStore App, signin process and WebUI.
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  // Requests from the browser and webui get special protection for
  // clients*.google.com URLs.
  bool is_request_from_browser = true;
  bool is_request_from_webui_renderer = false;
  if (info) {
    int process_id = info->GetChildID();
    // Never hide requests from guest processes.
    if (extensions::WebViewRendererState::GetInstance()->IsGuest(process_id) ||
        (navigation_ui_data && navigation_ui_data->is_web_view())) {
      return false;
    }

    if (extension_info_map &&
        extension_info_map->process_map().Contains(extensions::kWebStoreAppId,
                                                   process_id)) {
      return true;
    }

    is_request_from_browser = false;
    is_request_from_webui_renderer =
        content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
            process_id);
  }

  const GURL& url = request->url();
  return IsSensitiveURL(
             url, is_request_from_browser || is_request_from_webui_renderer) ||
         !HasWebRequestScheme(url);
}

// static
void WebRequestPermissions::
     AllowAllExtensionLocationsInPublicSessionForTesting(bool value) {
  g_allow_all_extension_locations_in_public_session = value;
}

// static
PermissionsData::AccessType WebRequestPermissions::CanExtensionAccessURL(
    const extensions::InfoMap* extension_info_map,
    const std::string& extension_id,
    const GURL& url,
    int tab_id,
    bool crosses_incognito,
    HostPermissionsCheck host_permissions_check,
    const base::Optional<url::Origin>& initiator) {
  // extension_info_map can be NULL in testing.
  if (!extension_info_map)
    return PermissionsData::ACCESS_ALLOWED;

  const extensions::Extension* extension =
      extension_info_map->extensions().GetByID(extension_id);
  if (!extension)
    return PermissionsData::ACCESS_DENIED;

  // Prevent viewing / modifying requests initiated by a host protected by
  // policy.
  if (initiator && extension->permissions_data()->IsRuntimeBlockedHost(
                       initiator->GetPhysicalOrigin().GetURL()))
    return PermissionsData::ACCESS_DENIED;

  // When we are in a Public Session, allow all URLs for webRequests initiated
  // by a regular extension (but don't allow chrome:// URLs).
#if defined(OS_CHROMEOS)
  if (chromeos::LoginState::IsInitialized() &&
      chromeos::LoginState::Get()->IsPublicSessionUser() &&
      extension->is_extension() &&
      !url.SchemeIs("chrome")) {
    // Make sure that the extension is truly installed by policy (the assumption
    // in Public Session is that all extensions are installed by policy).
    CHECK(g_allow_all_extension_locations_in_public_session ||
          extensions::Manifest::IsPolicyLocation(extension->location()));
    return PermissionsData::ACCESS_ALLOWED;
  }
#endif

  // Check if this event crosses incognito boundaries when it shouldn't.
  if (crosses_incognito && !extension_info_map->CanCrossIncognito(extension))
    return PermissionsData::ACCESS_DENIED;

  PermissionsData::AccessType access = PermissionsData::ACCESS_DENIED;
  switch (host_permissions_check) {
    case DO_NOT_CHECK_HOST:
      access = PermissionsData::ACCESS_ALLOWED;
      break;
    case REQUIRE_HOST_PERMISSION:
      // about: URLs are not covered in host permissions, but are allowed
      // anyway.
      if (url.SchemeIs(url::kAboutScheme) ||
          url::IsSameOriginWith(url, extension->url())) {
        access = PermissionsData::ACCESS_ALLOWED;
        break;
      }
      access = extension->permissions_data()->GetPageAccess(extension, url,
                                                            tab_id, nullptr);
      break;
    case REQUIRE_ALL_URLS:
      if (extension->permissions_data()->HasEffectiveAccessToAllHosts())
        access = PermissionsData::ACCESS_ALLOWED;
      // else ACCESS_DENIED
      break;
  }

  return access;
}

// static
bool WebRequestPermissions::CanExtensionAccessInitiator(
    const extensions::InfoMap* extension_info_map,
    const extensions::ExtensionId extension_id,
    const base::Optional<url::Origin>& initiator,
    int tab_id,
    bool crosses_incognito) {
  PermissionsData::AccessType access = PermissionsData::ACCESS_ALLOWED;
  if (initiator) {
    access = CanExtensionAccessURL(
        extension_info_map, extension_id, initiator->GetURL(), tab_id,
        crosses_incognito, WebRequestPermissions::REQUIRE_HOST_PERMISSION,
        base::nullopt);
  }
  return access == PermissionsData::ACCESS_ALLOWED;
}
