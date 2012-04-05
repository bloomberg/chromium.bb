// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/host_port_pair.h"
#include "net/base/dns_util.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/public_key_hashes.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/url_blacklist_manager.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceRequestInfo;

namespace {

// If the |request| failed due to problems with a proxy, forward the error to
// the proxy extension API.
void ForwardProxyErrors(net::URLRequest* request,
                        ExtensionEventRouterForwarder* event_router,
                        void* profile) {
  if (request->status().status() == net::URLRequestStatus::FAILED) {
    switch (request->status().error()) {
      case net::ERR_PROXY_AUTH_UNSUPPORTED:
      case net::ERR_PROXY_CONNECTION_FAILED:
      case net::ERR_TUNNEL_CONNECTION_FAILED:
        extensions::ProxyEventRouter::GetInstance()->OnProxyError(
            event_router, profile, request->status().error());
    }
  }
}

enum RequestStatus { REQUEST_STARTED, REQUEST_DONE };

// Notifies the ExtensionProcessManager that a request has started or stopped
// for a particular RenderView.
void NotifyEPMRequestStatus(RequestStatus status,
                            void* profile_id,
                            int process_id,
                            int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  ExtensionProcessManager* extension_process_manager =
      profile->GetExtensionProcessManager();
  // This may be NULL in unit tests.
  if (!extension_process_manager)
    return;

  // Will be NULL if the request was not issued on behalf of a renderer (e.g. a
  // system-level request).
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(process_id, render_view_id);
  if (render_view_host) {
    if (status == REQUEST_STARTED) {
      extension_process_manager->OnNetworkRequestStarted(render_view_host);
    } else if (status == REQUEST_DONE) {
      extension_process_manager->OnNetworkRequestDone(render_view_host);
    } else {
      NOTREACHED();
    }
  }
}

void ForwardRequestStatus(
    RequestStatus status, net::URLRequest* request, void* profile_id) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (!info)
    return;

  int process_id, render_view_id;
  if (info->GetAssociatedRenderView(&process_id, &render_view_id)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&NotifyEPMRequestStatus,
                   status, profile_id, process_id, render_view_id));
  }
}

const char* const kComodoCerts[] = {
  kSPKIHash_AAACertificateServices,
  kSPKIHash_AddTrustClass1CARoot,
  kSPKIHash_AddTrustExternalCARoot,
  kSPKIHash_AddTrustPublicCARoot,
  kSPKIHash_AddTrustQualifiedCARoot,
  kSPKIHash_COMODOCertificationAuthority,
  kSPKIHash_SecureCertificateServices,
  kSPKIHash_TrustedCertificateServices,
  kSPKIHash_UTNDATACorpSGC,
  kSPKIHash_UTNUSERFirstClientAuthenticationandEmail,
  kSPKIHash_UTNUSERFirstHardware,
  kSPKIHash_UTNUSERFirstObject,
};

// IsComodoCertificate returns true if a known Comodo public key appears in
// |public_key_hashes|.
// TODO(agl): remove once experiment is complete, by July 2012.
static bool IsComodoCertificate(
    const std::vector<net::SHA1Fingerprint>& public_key_hashes) {
  for (std::vector<net::SHA1Fingerprint>::const_iterator
       i = public_key_hashes.begin(); i != public_key_hashes.end(); ++i) {
    std::string base64_hash;
    base::Base64Encode(
        base::StringPiece(reinterpret_cast<const char*>(i->data),
                          arraysize(i->data)),
        &base64_hash);
    base64_hash = "sha1/" + base64_hash;

    for (size_t j = 0; j < arraysize(kComodoCerts); j++) {
      if (base64_hash == kComodoCerts[j])
        return true;
    }
  }

  return false;
}

// RecordComodoDNSResult is a callback from a DNS resolution. It records the
// elapsed time in a histogram.
// TODO(agl): remove once experiment is complete, by July 2012.
static void RecordComodoDNSResult(net::RRResponse* response,
                                  base::TimeTicks start_time,
                                  int result) {
  base::TimeDelta total_time = base::TimeTicks::Now() - start_time;
  if (total_time.InMilliseconds() > 10) {
    // If the reply took > 10 ms there's a good chance that it didn't come from
    // a local DNS cache.
    if (result == net::OK &&
        response->rrdatas.size() &&
        response->rrdatas[0].find("wibble") != std::string::npos) {
      UMA_HISTOGRAM_TIMES("Net.ComodoDNSExperimentSuccessTime", total_time);
    } else {
      UMA_HISTOGRAM_TIMES("Net.ComodoDNSExperimentFailureTime", total_time);
    }
  }
}

bool g_comodo_dns_experiment_enabled = false;

}  // namespace

ChromeNetworkDelegate::ChromeNetworkDelegate(
    ExtensionEventRouterForwarder* event_router,
    ExtensionInfoMap* extension_info_map,
    const policy::URLBlacklistManager* url_blacklist_manager,
    void* profile,
    CookieSettings* cookie_settings,
    BooleanPrefMember* enable_referrers)
    : event_router_(event_router),
      profile_(profile),
      cookie_settings_(cookie_settings),
      extension_info_map_(extension_info_map),
      enable_referrers_(enable_referrers),
      url_blacklist_manager_(url_blacklist_manager) {
  DCHECK(event_router);
  DCHECK(enable_referrers);
  DCHECK(!profile || cookie_settings);
}

ChromeNetworkDelegate::~ChromeNetworkDelegate() {}

// static
void ChromeNetworkDelegate::InitializeReferrersEnabled(
    BooleanPrefMember* enable_referrers,
    PrefService* pref_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enable_referrers->Init(prefs::kEnableReferrers, pref_service, NULL);
  enable_referrers->MoveToThread(BrowserThread::IO);
}

// static
void ChromeNetworkDelegate::EnableComodoDNSExperiment() {
  g_comodo_dns_experiment_enabled = true;
}

int ChromeNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  // TODO(joaodasilva): This prevents extensions from seeing URLs that are
  // blocked. However, an extension might redirect the request to another URL,
  // which is not blocked.
  if (url_blacklist_manager_ &&
      url_blacklist_manager_->IsURLBlocked(request->url())) {
    // URL access blocked by policy.
    scoped_refptr<net::NetLog::EventParameters> params;
    params = new net::NetLogStringParameter("url", request->url().spec());
    request->net_log().AddEvent(
        net::NetLog::TYPE_CHROME_POLICY_ABORTED_REQUEST, params);
    return net::ERR_NETWORK_ACCESS_DENIED;
  }
#endif

  ForwardRequestStatus(REQUEST_STARTED, request, profile_);

  if (!enable_referrers_->GetValue())
    request->set_referrer(std::string());
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      profile_, extension_info_map_.get(), request, callback, new_url);
}

int ChromeNetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
      profile_, extension_info_map_.get(), request, callback, headers);
}

void ChromeNetworkDelegate::OnSendHeaders(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  ExtensionWebRequestEventRouter::GetInstance()->OnSendHeaders(
      profile_, extension_info_map_.get(), request, headers);
}

int ChromeNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
      profile_, extension_info_map_.get(), request, callback,
      original_response_headers, override_response_headers);
}

void ChromeNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                             const GURL& new_location) {
  ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRedirect(
      profile_, extension_info_map_.get(), request, new_location);
}


void ChromeNetworkDelegate::OnResponseStarted(net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      profile_, extension_info_map_.get(), request);
  ForwardProxyErrors(request, event_router_.get(), profile_);

  if (g_comodo_dns_experiment_enabled) {
    // This is a temporary experiment, in conjuction with Comodo, to measure the
    // effectiveness of a possible DNS-based revocation mechanism.
    // TODO(agl): remove once experiment is complete, by July 2012.
    const net::SSLInfo& ssl_info = request->response_info().ssl_info;
    if (request->response_info().was_cached == false &&
        ssl_info.is_valid() &&
        ssl_info.is_issued_by_known_root &&
        IsComodoCertificate(ssl_info.public_key_hashes)) {
      if (dnsrr_resolver_.get() == NULL)
        dnsrr_resolver_.reset(new net::DnsRRResolver);

      // The Comodo DNS record has a 20 minute TTL, so we won't actually
      // request it more than three times an hour because it'll be in the
      // DnsRRResolver's cache. However, just in case something goes wrong we
      // also implement a hard stop to prevent resolutions more than once
      // every ten minutes.
      const base::TimeTicks now(base::TimeTicks::Now());
      if (!last_comodo_resolution_time_.is_null() &&
          (now - last_comodo_resolution_time_).InMinutes() < 10) {
        return;
      }
      last_comodo_resolution_time_ = now;

      net::RRResponse* response = new net::RRResponse;
      base::TimeTicks start_time(now);
      dnsrr_resolver_->Resolve(
          "wibble.comodoca.com", net::kDNS_TXT, 0 /* flags */,
          Bind(RecordComodoDNSResult, base::Owned(response), start_time),
          response, 0 /* priority */,
          request->net_log());
    }
  }
}

void ChromeNetworkDelegate::OnRawBytesRead(const net::URLRequest& request,
                                           int bytes_read) {
  TaskManager::GetInstance()->model()->NotifyBytesRead(request, bytes_read);
}

void ChromeNetworkDelegate::OnCompleted(net::URLRequest* request,
                                        bool started) {
  if (request->status().status() == net::URLRequestStatus::SUCCESS ||
      request->status().status() == net::URLRequestStatus::HANDLED_EXTERNALLY) {
    bool is_redirect = request->response_headers() &&
        net::HttpResponseHeaders::IsRedirectResponseCode(
            request->response_headers()->response_code());
    if (!is_redirect) {
      ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
          profile_, extension_info_map_.get(), request);
    }
  } else if (request->status().status() == net::URLRequestStatus::FAILED ||
             request->status().status() == net::URLRequestStatus::CANCELED) {
    ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
            profile_, extension_info_map_.get(), request, started);
  } else {
    NOTREACHED();
  }
  ForwardProxyErrors(request, event_router_.get(), profile_);

  ForwardRequestStatus(REQUEST_DONE, request, profile_);
}

void ChromeNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnURLRequestDestroyed(
      profile_, request);
}

void ChromeNetworkDelegate::OnPACScriptError(int line_number,
                                             const string16& error) {
  extensions::ProxyEventRouter::GetInstance()->OnPACScriptError(
      event_router_.get(), profile_, line_number, error);
}

net::NetworkDelegate::AuthRequiredResponse
ChromeNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnAuthRequired(
      profile_, extension_info_map_.get(), request, auth_info,
      callback, credentials);
}

bool ChromeNetworkDelegate::CanGetCookies(
    const net::URLRequest* request,
    const net::CookieList& cookie_list) {
  // NULL during tests, or when we're running in the system context.
  if (!cookie_settings_)
    return true;

  bool allow = cookie_settings_->IsReadingCookieAllowed(
      request->url(), request->first_party_for_cookies());

  int render_process_id = -1;
  int render_view_id = -1;
  if (content::ResourceRequestInfo::GetRenderViewForRequest(
          request, &render_process_id, &render_view_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::CookiesRead,
                   render_process_id, render_view_id,
                   request->url(), request->first_party_for_cookies(),
                   cookie_list, !allow));
  }

  return allow;
}

bool ChromeNetworkDelegate::CanSetCookie(
    const net::URLRequest* request,
    const std::string& cookie_line,
    net::CookieOptions* options) {
  // NULL during tests, or when we're running in the system context.
  if (!cookie_settings_)
    return true;

  bool allow = cookie_settings_->IsSettingCookieAllowed(
      request->url(), request->first_party_for_cookies());

  if (cookie_settings_->IsCookieSessionOnly(request->url()))
    options->set_force_session();

  int render_process_id = -1;
  int render_view_id = -1;
  if (content::ResourceRequestInfo::GetRenderViewForRequest(
          request, &render_process_id, &render_view_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::CookieChanged,
                   render_process_id, render_view_id,
                   request->url(), request->first_party_for_cookies(),
                   cookie_line, *options, !allow));
  }

  return allow;
}
