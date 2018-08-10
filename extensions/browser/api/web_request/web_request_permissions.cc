// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_permissions.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state.h"
#endif  // defined(OS_CHROMEOS)

using content::ResourceRequestInfo;
using extensions::PermissionsData;

namespace {

// Describes the different cases pertaining to permissions check for the
// initiator.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InitiatorAccess {
  kAbsent = 0,
  kNoAccess = 1,
  kHasAccess = 2,
  kMaxValue = kHasAccess,
};

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

PermissionsData::PageAccess GetHostAccessForURL(
    const extensions::Extension& extension,
    const GURL& url,
    int tab_id) {
  // about: URLs are not covered in host permissions, but are allowed
  // anyway.
  if (url.SchemeIs(url::kAboutScheme) ||
      url::IsSameOriginWith(url, extension.url())) {
    return PermissionsData::PageAccess::kAllowed;
  }

  return extension.permissions_data()->GetPageAccess(url, tab_id,
                                                     nullptr /*error*/);
}

// Returns the most restricted access type out of |access1| and |access2|.
PermissionsData::PageAccess GetMinimumAccessType(
    PermissionsData::PageAccess access1,
    PermissionsData::PageAccess access2) {
  PermissionsData::PageAccess access = PermissionsData::PageAccess::kDenied;
  switch (access1) {
    case PermissionsData::PageAccess::kDenied:
      access = PermissionsData::PageAccess::kDenied;
      break;
    case PermissionsData::PageAccess::kWithheld:
      access = (access2 == PermissionsData::PageAccess::kDenied
                    ? PermissionsData::PageAccess::kDenied
                    : PermissionsData::PageAccess::kWithheld);
      break;
    case PermissionsData::PageAccess::kAllowed:
      access = access2;
      break;
  }
  return access;
}

PermissionsData::PageAccess CanExtensionAccessURLInternal(
    const extensions::InfoMap* extension_info_map,
    const std::string& extension_id,
    const GURL& url,
    int tab_id,
    bool crosses_incognito,
    WebRequestPermissions::HostPermissionsCheck host_permissions_check,
    const base::Optional<url::Origin>& initiator) {
  // extension_info_map can be NULL in testing.
  if (!extension_info_map)
    return PermissionsData::PageAccess::kAllowed;

  const extensions::Extension* extension =
      extension_info_map->extensions().GetByID(extension_id);
  if (!extension)
    return PermissionsData::PageAccess::kDenied;

  // Prevent viewing / modifying requests initiated by a host protected by
  // policy.
  if (initiator &&
      extension->permissions_data()->IsPolicyBlockedHost(initiator->GetURL()))
    return PermissionsData::PageAccess::kDenied;

// When restrictions are enabled in Public Session, allow all URLs for
// webRequests initiated by a regular extension (but don't allow chrome://
// URLs).
#if defined(OS_CHROMEOS)
  if (chromeos::LoginState::IsInitialized() &&
      chromeos::LoginState::Get()->ArePublicSessionRestrictionsEnabled() &&
      extension->is_extension() && !url.SchemeIs("chrome")) {
    // Make sure that the extension is truly installed by policy (the assumption
    // in Public Session is that all extensions are installed by policy).
    CHECK(g_allow_all_extension_locations_in_public_session ||
          extensions::Manifest::IsPolicyLocation(extension->location()));
    return PermissionsData::PageAccess::kAllowed;
  }
#endif

  // Check if this event crosses incognito boundaries when it shouldn't.
  if (crosses_incognito && !extension_info_map->CanCrossIncognito(extension))
    return PermissionsData::PageAccess::kDenied;

  PermissionsData::PageAccess access = PermissionsData::PageAccess::kDenied;
  switch (host_permissions_check) {
    case WebRequestPermissions::DO_NOT_CHECK_HOST:
      access = PermissionsData::PageAccess::kAllowed;
      break;
    case WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL:
      access = GetHostAccessForURL(*extension, url, tab_id);
      // If access to the host was withheld, check if the extension has access
      // to the initiator. If it does, allow the extension to see the request.
      // This is important for extensions with webRequest to work well with
      // runtime host permissions.
      if (access == PermissionsData::PageAccess::kWithheld) {
        PermissionsData::PageAccess initiator_access =
            initiator
                ? GetHostAccessForURL(*extension, initiator->GetURL(), tab_id)
                : PermissionsData::PageAccess::kDenied;
        if (initiator_access == PermissionsData::PageAccess::kAllowed)
          access = PermissionsData::PageAccess::kAllowed;
      }
      break;
    case WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR: {
      PermissionsData::PageAccess request_access =
          GetHostAccessForURL(*extension, url, tab_id);
      PermissionsData::PageAccess initiator_access =
          initiator
              ? GetHostAccessForURL(*extension, initiator->GetURL(), tab_id)
              : PermissionsData::PageAccess::kAllowed;
      access = GetMinimumAccessType(request_access, initiator_access);
      break;
    }
    case WebRequestPermissions::REQUIRE_ALL_URLS:
      if (extension->permissions_data()->HasEffectiveAccessToAllHosts())
        access = PermissionsData::PageAccess::kAllowed;
      // else ACCESS_DENIED
      break;
  }

  return access;
}

}  // namespace

// Returns true if the given |request| is sensitive and must not be
// modified/canceled by extensions, e.g. because it is targeted to the webstore
// to check for updates, extension blacklisting, etc.
bool IsSensitiveRequest(const extensions::WebRequestInfo& request,
                        bool is_request_from_browser,
                        bool is_request_from_webui_renderer) {
  const bool is_request_from_sensitive_source =
      is_request_from_browser || is_request_from_webui_renderer;
  const GURL& url = request.url;

  const bool is_network_request =
      url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS();
  if (is_network_request && is_request_from_webui_renderer) {
    // WebUI renderers should never be making network requests, but we may make
    // some exceptions for now. See https://crbug.com/829412 for details.
    //
    // The DCHECK helps avoid proliferation of such behavior. In any case, we
    // treat the requests as sensitive to ensure that the Web Request API
    // doesn't see them.
    DCHECK(request.initiator.has_value());
    DCHECK(extensions::ExtensionsBrowserClient::Get()
               ->IsWebUIAllowedToMakeNetworkRequests(*request.initiator))
        << "Unsupported network request from "
        << request.initiator->GetURL().spec() << " for " << url.spec();
    return true;
  }

  // TODO(battre) Merge this, CanExtensionAccessURL and
  // PermissionsData::CanAccessPage into one function.
  bool sensitive_chrome_url = false;
  const char kGoogleCom[] = "google.com";
  const char kClient[] = "clients";
  url::Origin origin = url::Origin::Create(url);
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
    if (is_request_from_sensitive_source) {
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

  return sensitive_chrome_url ||
         extensions::ExtensionsAPIClient::Get()
             ->ShouldHideBrowserNetworkRequest(request) ||
         extension_urls::IsWebstoreUpdateUrl(url) ||
         extension_urls::IsBlacklistUpdateUrl(url) ||
         extension_urls::IsSafeBrowsingUrl(origin, url.path_piece());
}

// static
bool WebRequestPermissions::HideRequest(
    const extensions::InfoMap* extension_info_map,
    const extensions::WebRequestInfo& request) {
  // Requests from <webview> are never hidden.
  if (request.is_web_view)
    return false;

  // Requests from PAC scripts are always hidden.
  // See https://crbug.com/794674
  if (request.is_pac_request)
    return true;

  // Requests from the browser and webui get special protection for
  // clients*.google.com URLs.
  bool is_request_from_browser =
      request.render_process_id == -1 &&
      // Browser requests are often of the "other" resource type.
      // Main frame requests are not unconditionally seen as a sensitive browser
      // request, because a request can also be browser-driven if there is no
      // process to associate the request with. E.g. navigations via the
      // chrome.tabs.update extension API.
      request.type != content::RESOURCE_TYPE_MAIN_FRAME;
  bool is_request_from_webui_renderer = false;
  if (!is_request_from_browser) {
    // Requests from guest processes are never hidden.
    if (request.is_web_view)
      return false;

    // Hide requests from the Chrome WebStore App, signin process, and WebUI.
    if (extension_info_map &&
        extension_info_map->process_map().Contains(extensions::kWebStoreAppId,
                                                   request.render_process_id)) {
      return true;
    }

    is_request_from_webui_renderer =
        content::ChildProcessSecurityPolicy::GetInstance()->HasWebUIBindings(
            request.render_process_id);
  }

  return IsSensitiveRequest(request, is_request_from_browser,
                            is_request_from_webui_renderer) ||
         !HasWebRequestScheme(request.url);
}

// static
void WebRequestPermissions::
     AllowAllExtensionLocationsInPublicSessionForTesting(bool value) {
  g_allow_all_extension_locations_in_public_session = value;
}

// static
PermissionsData::PageAccess WebRequestPermissions::CanExtensionAccessURL(
    const extensions::InfoMap* extension_info_map,
    const std::string& extension_id,
    const GURL& url,
    int tab_id,
    bool crosses_incognito,
    HostPermissionsCheck host_permissions_check,
    const base::Optional<url::Origin>& initiator) {
  PermissionsData::PageAccess access = CanExtensionAccessURLInternal(
      extension_info_map, extension_id, url, tab_id, crosses_incognito,
      host_permissions_check, initiator);

  // For clients only checking host permissions for |url| (e.g. the web request
  // API), log metrics to see whether they have host permissions to |initiator|,
  // given they have access to |url|.
  bool log_metrics =
      host_permissions_check == REQUIRE_HOST_PERMISSION_FOR_URL &&
      access != PermissionsData::PageAccess::kDenied;
  if (!log_metrics)
    return access;

  InitiatorAccess initiator_access = InitiatorAccess::kAbsent;
  if (initiator) {
    PermissionsData::PageAccess access = CanExtensionAccessURLInternal(
        extension_info_map, extension_id, initiator->GetURL(), tab_id,
        crosses_incognito, REQUIRE_HOST_PERMISSION_FOR_URL, base::nullopt);
    initiator_access = access == PermissionsData::PageAccess::kDenied
                           ? InitiatorAccess::kNoAccess
                           : InitiatorAccess::kHasAccess;
  }

  UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequest.InitiatorAccess",
                            initiator_access);
  return access;
}

// static
bool WebRequestPermissions::CanExtensionAccessInitiator(
    const extensions::InfoMap* extension_info_map,
    const extensions::ExtensionId extension_id,
    const base::Optional<url::Origin>& initiator,
    int tab_id,
    bool crosses_incognito) {
  if (!initiator)
    return true;

  return CanExtensionAccessURLInternal(
             extension_info_map, extension_id, initiator->GetURL(), tab_id,
             crosses_incognito,
             WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
             base::nullopt) == PermissionsData::PageAccess::kAllowed;
}
