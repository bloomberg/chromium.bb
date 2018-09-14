// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "components/base32/base32.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "net/base/ip_address.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

bool IsPreviewsDomain(const GURL& url) {
  GURL previews_host = previews::params::GetLitePagePreviewsDomainURL();
  return url.DomainIs(previews_host.host()) &&
         url.EffectiveIntPort() == previews_host.EffectiveIntPort();
}

bool IsPrivateDomain(const GURL& url) {
  if (url.host().find(".") == base::StringPiece::npos)
    return true;

  // Allow localhost check to be skipped if needed, like in testing.
  if (net::IsLocalhost(url))
    return !previews::params::LitePagePreviewsTriggerOnLocalhost();

  net::IPAddress ip_addr;
  if (url.HostIsIPAddress() && ip_addr.AssignFromIPLiteral(url.host()) &&
      !ip_addr.IsPubliclyRoutable()) {
    return true;
  }
  return false;
}

content::OpenURLParams MakeOpenURLParams(content::NavigationHandle* handle,
                                         GURL url,
                                         const std::string& headers) {
  content::OpenURLParams url_params(
      url, handle->GetReferrer(), WindowOpenDisposition::CURRENT_TAB,
      handle->GetPageTransition(), handle->IsRendererInitiated());
  url_params.extra_headers = headers;
  url_params.redirect_chain = handle->GetRedirectChain();
  url_params.frame_tree_node_id = handle->GetFrameTreeNodeId();
  url_params.user_gesture = handle->HasUserGesture();
  url_params.started_from_context_menu = handle->WasStartedFromContextMenu();
  return url_params;
}

}  // namespace

class WebContentsLifetimeHelper
    : public content::WebContentsUserData<WebContentsLifetimeHelper> {
 public:
  explicit WebContentsLifetimeHelper(content::WebContents* web_contents)
      : web_contents_(web_contents), weak_factory_(this) {}

  base::WeakPtr<WebContentsLifetimeHelper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void PostNewNavigation(const content::OpenURLParams& url_params) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(url_params.url.is_valid());
    DCHECK(url_params.url.SchemeIs(url::kHttpsScheme));
    web_contents_->OpenURL(url_params);
  }

 private:
  content::WebContents* web_contents_;
  base::WeakPtrFactory<WebContentsLifetimeHelper> weak_factory_;
};

PreviewsLitePageNavigationThrottle::PreviewsLitePageNavigationThrottle(
    content::NavigationHandle* handle,
    PreviewsLitePageNavigationThrottleManager* manager)
    : content::NavigationThrottle(handle), manager_(manager) {
  DCHECK(manager_);
  DCHECK(handle);
  DCHECK(handle->GetWebContents());
  DCHECK(handle->GetWebContents()->GetBrowserContext());
}

PreviewsLitePageNavigationThrottle::~PreviewsLitePageNavigationThrottle() =
    default;

bool PreviewsLitePageNavigationThrottle::IsEligibleForPreview() const {
  // Check if the parameters of the navigation are not eligible for the preview.
  std::vector<IneligibleReason> ineligible_reasons;
  const GURL& url = navigation_handle()->GetURL();
  if (!url.SchemeIs(url::kHttpsScheme))
    ineligible_reasons.push_back(IneligibleReason::kNonHttpsScheme);

  if (navigation_handle()->IsPost())
    ineligible_reasons.push_back(IneligibleReason::kHttpPost);

  if (!navigation_handle()->IsInMainFrame())
    ineligible_reasons.push_back(IneligibleReason::kSubframeNavigation);

  if (manager_->IsServerUnavailable())
    ineligible_reasons.push_back(IneligibleReason::kServerUnavailable);

  // Record UMA.
  for (IneligibleReason reason : ineligible_reasons) {
    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.IneligibleReasons",
                              reason);
  }
  if (!ineligible_reasons.empty())
    return false;

  // Check dynamic blacklists.
  std::vector<BlacklistReason> blacklist_reasons;

  if (IsPreviewsDomain(url))
    blacklist_reasons.push_back(BlacklistReason::kNavigationToPreviewsDomain);

  if (IsPrivateDomain(url))
    blacklist_reasons.push_back(BlacklistReason::kNavigationToPrivateDomain);

  std::vector<std::string> blacklisted_path_suffixes =
      previews::params::LitePagePreviewsBlacklistedPathSuffixes();
  for (const std::string& suffix : blacklisted_path_suffixes) {
    if (base::EndsWith(url.path(), suffix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      blacklist_reasons.push_back(BlacklistReason::kPathSuffixBlacklisted);
      break;
    }
  }

  // Record UMA
  for (BlacklistReason reason : blacklist_reasons) {
    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.BlacklistReasons",
                              reason);
  }

  return blacklist_reasons.empty();
}

// static
bool PreviewsLitePageNavigationThrottle::GetOriginalURL(
    const GURL& url,
    std::string* original_url) {
  if (!url.is_valid())
    return false;

  if (!IsPreviewsDomain(url))
    return false;

  std::string original_url_query_param;
  if (!net::GetValueForKeyInQuery(url, "u", &original_url_query_param))
    return false;

  if (original_url)
    *original_url = original_url_query_param;
  return true;
}

// static
GURL PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(
    const GURL& original_url) {
  DCHECK(original_url.is_valid());
  DCHECK(!IsPreviewsDomain(original_url));

  std::string origin_hash = base::ToLowerASCII(base32::Base32Encode(
      crypto::SHA256HashString(
          original_url.scheme() + "://" + original_url.host() + ":" +
          base::IntToString(original_url.EffectiveIntPort())),
      base32::Base32EncodePolicy::OMIT_PADDING));
  GURL previews_host = previews::params::GetLitePagePreviewsDomainURL();
  GURL previews_url = GURL(
      previews_host.scheme() + "://" + origin_hash + "." +
      previews_host.host() +
      (previews_host.has_port() ? (":" + previews_host.port()) : "") + "/p?u=" +
      net::EscapeQueryParamValue(original_url.spec(), true /* use_plus */));
  DCHECK(previews_url.is_valid());
  DCHECK_EQ(previews_host.scheme(), previews_url.scheme());
  return previews_url;
}

GURL PreviewsLitePageNavigationThrottle::GetPreviewsURL() const {
  return GetPreviewsURLForURL(navigation_handle()->GetURL());
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::CreateNewNavigation(
    content::OpenURLParams url_params) const {
  // The helper class and its weak pointer protect against the WebContents
  // dying in-between the PostTask and its execution, resulting in a use after
  // free bug. Since the helper is a WebContentsUserData, it will be
  // destroyed when the WebContents is and the task will not be executed.
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  WebContentsLifetimeHelper::CreateForWebContents(web_contents);
  WebContentsLifetimeHelper* helper =
      WebContentsLifetimeHelper::FromWebContents(web_contents);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&WebContentsLifetimeHelper::PostNewNavigation,
                     helper->GetWeakPtr(), url_params));

  return content::NavigationThrottle::CANCEL;
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::TriggerPreview() const {
  net::HttpRequestHeaders request_headers;
  content::BrowserContext* browser_context =
      navigation_handle()->GetWebContents()->GetBrowserContext();

  // Set DRP headers.
  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);
  DCHECK(drp_settings);
  request_headers.MergeFrom(drp_settings->GetProxyRequestHeaders());

  // Set ECT header.
  request_headers.SetHeader(data_reduction_proxy::chrome_proxy_ect_header(),
                            net::GetNameForEffectiveConnectionType(
                                g_browser_process->network_quality_tracker()
                                    ->GetEffectiveConnectionType()));

  // TODO(crbug.com/883955): Add in page id to the chrome-proxy header.
  content::OpenURLParams url_params = MakeOpenURLParams(
      navigation_handle(), GetPreviewsURL(), request_headers.ToString());
  return CreateNewNavigation(url_params);
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::MaybeNavigateToPreview() const {
  const bool trigger =
      IsEligibleForPreview() &&
      !manager_->CheckSingleBypass(navigation_handle()->GetURL().spec());
  UMA_HISTOGRAM_BOOLEAN("Previews.ServerLitePage.Triggered", trigger);
  if (trigger)
    return TriggerPreview();
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillStartRequest() {
  return MaybeNavigateToPreview();
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillRedirectRequest() {
  return MaybeNavigateToPreview();
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillFailRequest() {
  std::string original_url;
  if (!GetOriginalURL(navigation_handle()->GetURL(), &original_url))
    return content::NavigationThrottle::PROCEED;

  UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                            ServerResponse::kFailed);

  // The Preview was triggered but there was some irrecoverable issue (like
  // there is no network connection). Load the original page and let it go
  // through the normal process for whatever error it is.
  manager_->AddSingleBypass(original_url);
  content::OpenURLParams url_params =
      MakeOpenURLParams(navigation_handle(), GURL(original_url), std::string());

  return CreateNewNavigation(url_params);
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillProcessResponse() {
  const net::HttpResponseHeaders* response_headers =
      navigation_handle()->GetResponseHeaders();
  DCHECK(response_headers);

  std::string original_url;
  if (!GetOriginalURL(navigation_handle()->GetURL(), &original_url)) {
    // Return early if this request was not for a Preview.
    return content::NavigationThrottle::PROCEED;
  }

  // After this point, the given response is known to be for a Preview.
  // The Previews server will only send the following response codes: 200, 307,
  // 404, and 503. 200 and 307 should proceed as normal, 404 and 503 request the
  // client to load the original page instead because the server is not capable
  // of generating a lite page. All other response codes are treated as a 404.

  const int response_code = response_headers->response_code();

  if (response_code == net::HTTP_OK) {
    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                              ServerResponse::kOk);
    return content::NavigationThrottle::PROCEED;
  }

  if (response_code == net::HTTP_TEMPORARY_REDIRECT) {
    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                              ServerResponse::kRedirect);
    return content::NavigationThrottle::PROCEED;
  }

  const base::TimeDelta penalty =
      base::TimeTicks::Now() - navigation_handle()->NavigationStart();
  UMA_HISTOGRAM_MEDIUM_TIMES("Previews.ServerLitePage.HttpOnlyFallbackPenalty",
                             penalty);
  manager_->AddSingleBypass(original_url);
  content::OpenURLParams original_url_params =
      MakeOpenURLParams(navigation_handle(), GURL(original_url), std::string());

  if (response_code == net::HTTP_NOT_FOUND) {
    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                              ServerResponse::kPreviewUnavailable);
    return CreateNewNavigation(original_url_params);
  }

  if (response_code == net::HTTP_SERVICE_UNAVAILABLE) {
    std::string retry_after_header;
    base::TimeDelta retry_after = base::TimeDelta::FromSeconds(
        base::RandInt(60, previews::params::PreviewServerLoadshedMaxSeconds()));
    if (response_headers->EnumerateHeader(nullptr, "retry-after",
                                          &retry_after_header)) {
      net::HttpUtil::ParseRetryAfterHeader(retry_after_header,
                                           base::Time::Now(), &retry_after);
    }
    manager_->SetServerUnavailableFor(retry_after);

    UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                              ServerResponse::kServiceUnavailable);
    return CreateNewNavigation(original_url_params);
  }

  UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.ServerResponse",
                            ServerResponse::kOther);
  return CreateNewNavigation(original_url_params);
}

const char* PreviewsLitePageNavigationThrottle::GetNameForLogging() {
  return "PreviewsLitePageNavigationThrottle";
}
