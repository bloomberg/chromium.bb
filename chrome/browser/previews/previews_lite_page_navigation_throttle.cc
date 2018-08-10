// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"

#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
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
#include "url/gurl.h"

namespace {

bool IsPreviewsDomain(const GURL& url) {
  GURL previews_host = previews::params::GetLitePagePreviewsDomainURL();
  return url.DomainIs(previews_host.host()) &&
         url.EffectiveIntPort() == previews_host.EffectiveIntPort();
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

  if (IsPreviewsDomain(navigation_handle()->GetURL()))
    return false;

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
PreviewsLitePageNavigationThrottle::MaybeNavigateToPreview() const {
  if (!IsEligibleForPreview()) {
    return content::NavigationThrottle::PROCEED;
  }

  content::OpenURLParams url_params(GetPreviewsURL(),
                                    navigation_handle()->GetReferrer(),
                                    WindowOpenDisposition::CURRENT_TAB,
                                    navigation_handle()->GetPageTransition(),
                                    navigation_handle()->IsRendererInitiated());
  // TODO(crbug.com/864652): Add chrome-proxy headers.
  url_params.redirect_chain = navigation_handle()->GetRedirectChain();
  url_params.frame_tree_node_id = navigation_handle()->GetFrameTreeNodeId();
  url_params.user_gesture = navigation_handle()->HasUserGesture();
  url_params.started_from_context_menu =
      navigation_handle()->WasStartedFromContextMenu();

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
PreviewsLitePageNavigationThrottle::WillStartRequest() {
  return MaybeNavigateToPreview();
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillRedirectRequest() {
  return MaybeNavigateToPreview();
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillFailRequest() {
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
PreviewsLitePageNavigationThrottle::WillProcessResponse() {
  return content::NavigationThrottle::PROCEED;
}

const char* PreviewsLitePageNavigationThrottle::GetNameForLogging() {
  return "PreviewsLitePageNavigationThrottle";
}
