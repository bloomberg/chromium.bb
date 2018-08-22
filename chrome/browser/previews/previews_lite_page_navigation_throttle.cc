// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/base32/base32.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/referrer.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

bool IsPreviewsDomain(const GURL& url) {
  GURL previews_host = previews::params::GetLitePagePreviewsDomainURL();
  return url.DomainIs(previews_host.host()) &&
         url.EffectiveIntPort() == previews_host.EffectiveIntPort();
}

content::OpenURLParams MakeOpenURLParams(content::NavigationHandle* handle,
                                         GURL url) {
  content::OpenURLParams url_params(
      url, handle->GetReferrer(), WindowOpenDisposition::CURRENT_TAB,
      handle->GetPageTransition(), handle->IsRendererInitiated());
  // TODO(crbug.com/864652): Add chrome-proxy headers if this is a Preview.
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
  if (!navigation_handle()->GetURL().SchemeIs(url::kHttpsScheme))
    return false;

  if (navigation_handle()->IsPost())
    return false;

  if (!navigation_handle()->IsInMainFrame())
    return false;

  if (manager_->IsServerUnavailable())
    return false;

  if (IsPreviewsDomain(navigation_handle()->GetURL()))
    return false;

  return true;
}

// static
bool PreviewsLitePageNavigationThrottle::GetOriginalURL(
    GURL url,
    std::string* original_url) {
  if (!url.is_valid())
    return false;

  if (!IsPreviewsDomain(url))
    return false;

  std::string original_url_query_param;
  if (!net::GetValueForKeyInQuery(url, "u", &original_url_query_param))
    return false;

  *original_url = original_url_query_param;
  return true;
}

GURL PreviewsLitePageNavigationThrottle::GetPreviewsURL() const {
  GURL original_url = navigation_handle()->GetURL();
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
  DCHECK(previews_url.SchemeIs(url::kHttpsScheme));
  return previews_url;
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
  content::OpenURLParams url_params =
      MakeOpenURLParams(navigation_handle(), GetPreviewsURL());
  return CreateNewNavigation(url_params);
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::MaybeNavigateToPreview() const {
  const GURL url = navigation_handle()->GetURL();
  if (!IsEligibleForPreview())
    return content::NavigationThrottle::PROCEED;

  // Make sure we're not trying to bypass this navigation.
  if (manager_->CheckSingleBypass(url.spec()))
    return content::NavigationThrottle::PROCEED;

  return TriggerPreview();
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

  // The Preview was triggered but there was some irrecoverable issue (like
  // there is no network connection). Load the original page and let it go
  // through the normal process for whatever error it is.
  manager_->AddSingleBypass(original_url);
  content::OpenURLParams url_params =
      MakeOpenURLParams(navigation_handle(), GURL(original_url));

  return CreateNewNavigation(url_params);
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillProcessResponse() {
  const net::HttpResponseHeaders* response_headers =
      navigation_handle()->GetResponseHeaders();
  if (!response_headers)
    return content::NavigationThrottle::PROCEED;

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

  if (response_code == net::HTTP_OK ||
      response_code == net::HTTP_TEMPORARY_REDIRECT)
    return content::NavigationThrottle::PROCEED;

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
  }

  manager_->AddSingleBypass(original_url);
  content::OpenURLParams url_params =
      MakeOpenURLParams(navigation_handle(), GURL(original_url));

  return CreateNewNavigation(url_params);
}

const char* PreviewsLitePageNavigationThrottle::GetNameForLogging() {
  return "PreviewsLitePageNavigationThrottle";
}
