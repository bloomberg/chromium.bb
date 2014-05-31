// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/proxy_advisor.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/http/http_status_code.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

// TODO(marq): Remove this class because it is not being used.

// Ensure data reduction features are available.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
#error proxy_advisor should only be included in Android or iOS builds.
#endif

using content::BrowserThread;
using data_reduction_proxy::DataReductionProxySettings;

namespace {
const char kOmniboxMotivation[] = "omnibox";
const char kLowThresholdOmniboxMotivation[] = "low_threshold_omnibox";
const char kStartupDNSMotivation[] = "startup_dns";
const char kEarlyLoadMotivation[] = "early_load";
const char kLearnedReferralMotivation[] = "learned_referral";
const char kLowThresholdLearnedReferralMotivation[] =
    "low_threshold_learned_referral";
const char kSelfReferralMotivation[] = "self_referral";
const char kPageScanMotivation[] = "page_scan";

// Maps a ResolutionMotivation to a string for use in the advisory HEAD
// request.
const char* MotivationName(
    chrome_browser_net::UrlInfo::ResolutionMotivation motivation,
    bool is_preconnect) {
  switch (motivation) {
    case chrome_browser_net::UrlInfo::OMNIBOX_MOTIVATED:
      return
          is_preconnect ? kOmniboxMotivation : kLowThresholdOmniboxMotivation;
    case chrome_browser_net::UrlInfo::STARTUP_LIST_MOTIVATED:
      return kStartupDNSMotivation;
    case chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED:
      return kEarlyLoadMotivation;
    case chrome_browser_net::UrlInfo::LEARNED_REFERAL_MOTIVATED:
      return is_preconnect ?
          kLearnedReferralMotivation : kLowThresholdLearnedReferralMotivation;
    case chrome_browser_net::UrlInfo::SELF_REFERAL_MOTIVATED:
      return kSelfReferralMotivation;
    case chrome_browser_net::UrlInfo::PAGE_SCAN_MOTIVATED:
      return kPageScanMotivation;
    default:
      // Other motivations should never be passed to here.
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return "";
}

}  // namespace

ProxyAdvisor::ProxyAdvisor(PrefService* pref_service,
                           net::URLRequestContextGetter* context_getter)
    : context_getter_(context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // pref_service may be null in mock test subclasses.
  if (pref_service) {
    proxy_pref_member_.Init(
        data_reduction_proxy::prefs::kDataReductionProxyEnabled,
        pref_service,
        base::Bind(&ProxyAdvisor::UpdateProxyState, base::Unretained(this)));
    proxy_pref_member_.MoveToThread(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }
}

ProxyAdvisor::~ProxyAdvisor() {
  STLDeleteElements(&inflight_requests_);
}

void ProxyAdvisor::OnResponseStarted(net::URLRequest* request) {
  const net::URLRequestStatus& status(request->status());
  if (!status.is_success()) {
    DLOG(WARNING) << "Proxy advisory failed "
        << "status:" << status.status()
        << " error:" << status.error();
  } else if (request->GetResponseCode() != net::HTTP_OK) {
    DLOG(WARNING) << "Proxy advisory status: " << request->GetResponseCode();
  }
  RequestComplete(request);
}

void ProxyAdvisor::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  // No-op for now, as we don't care yet.
}

void ProxyAdvisor::Advise(
    const GURL& url,
    chrome_browser_net::UrlInfo::ResolutionMotivation motivation,
    bool is_preconnect) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!WouldProxyURL(url))
    return;

  std::string motivation_name(MotivationName(motivation, is_preconnect));
  std::string header_value = motivation_name + " " + url.spec();
  net::URLRequestContext* context = context_getter_->GetURLRequestContext();
  data_reduction_proxy::DataReductionProxyParams params(
      data_reduction_proxy::DataReductionProxyParams::kAllowed |
      data_reduction_proxy::DataReductionProxyParams::kFallbackAllowed |
      data_reduction_proxy::DataReductionProxyParams::kPromoAllowed);
  std::string endpoint =
      params.origin().spec() + "preconnect";
  scoped_ptr<net::URLRequest> request = context->CreateRequest(
      GURL(endpoint), net::DEFAULT_PRIORITY, this, NULL);
  request->set_method("HEAD");
  request->SetExtraRequestHeaderByName(
      "Proxy-Host-Advisory", header_value, false);
  request->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_BYPASS_PROXY |
                        net::LOAD_DISABLE_CACHE);
  net::URLRequest* raw_request = request.get();
  inflight_requests_.insert(request.release());
  raw_request->Start();
}

// TODO(marq): Move this into DataReductionProxySettings, and add support for
// inspecting the current proxy configs -- if ResolveProxy on |url| can be
// done synchronously, then this is no longer an approximation.
bool ProxyAdvisor::WouldProxyURL(const GURL& url)  {
  if (!proxy_pref_member_.GetValue())
    return false;

  if (url.SchemeIsSecure())
    return false;

  return true;
}

void ProxyAdvisor::RequestComplete(net::URLRequest* request) {
  DCHECK_EQ(1u, inflight_requests_.count(request));
  scoped_ptr<net::URLRequest> scoped_request_for_deletion(request);
  inflight_requests_.erase(request);
  // |scoped_request_for_deletion| will delete |request|
}

void ProxyAdvisor::UpdateProxyState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Delete all inflight requests. Each request's destructor will call Cancel().
  STLDeleteElements(&inflight_requests_);
}
