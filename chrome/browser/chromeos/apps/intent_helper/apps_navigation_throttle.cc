// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_throttle.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/arc/intent_helper/page_transition_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

namespace {

constexpr char kGoogleCom[] = "google.com";

// Compares the host name of the referrer and target URL to decide whether
// the navigation needs to be overriden.
bool ShouldOverrideUrlLoading(const GURL& previous_url,
                              const GURL& current_url) {
  // When the navigation is initiated in a web page where sending a referrer
  // is disabled, |previous_url| can be empty. In this case, we should open
  // it in the desktop browser.
  if (!previous_url.is_valid() || previous_url.is_empty())
    return false;

  // Also check |current_url| just in case.
  if (!current_url.is_valid() || current_url.is_empty()) {
    DVLOG(1) << "Unexpected URL: " << current_url << ", opening it in Chrome.";
    return false;
  }

  // Check the scheme for both |previous_url| and |current_url| since an
  // extension could have referred us (e.g. Google Docs).
  if (!current_url.SchemeIsHTTPOrHTTPS() ||
      !previous_url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // TODO(dominickn): this was added as a special case for ARC. Reconsider if
  // it's necessary for all app platforms.
  if (net::registry_controlled_domains::SameDomainOrHost(
          current_url, previous_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    if (net::registry_controlled_domains::GetDomainAndRegistry(
            current_url,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES) ==
        kGoogleCom) {
      // Navigation within the google.com domain are good candidates for this
      // throttle (and consecuently the picker UI) only if they have different
      // hosts, this is because multiple services are hosted within the same
      // domain e.g. play.google.com, mail.google.com and so on.
      return current_url.host_piece() != previous_url.host_piece();
    }

    return false;
  }
  return true;
}

GURL GetStartingGURL(content::NavigationHandle* navigation_handle) {
  // This helps us determine a reference GURL for the current NavigationHandle.
  // This is the order or preferrence: Referrer > LastCommittedURL > SiteURL,
  // GetSiteURL *should* only be used on very rare cases, e.g. when the
  // navigation goes from https: to http: on a new tab, thus losing the other
  // potential referrers.
  const GURL referrer_url = navigation_handle->GetReferrer().url;
  if (referrer_url.is_valid() && !referrer_url.is_empty())
    return referrer_url;

  const GURL last_committed_url =
      navigation_handle->GetWebContents()->GetLastCommittedURL();
  if (last_committed_url.is_valid() && !last_committed_url.is_empty())
    return last_committed_url;

  return navigation_handle->GetStartingSiteInstance()->GetSiteURL();
}

}  // namespace

namespace chromeos {

// static
std::unique_ptr<content::NavigationThrottle>
AppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (arc::IsArcPlayStoreEnabledForProfile(Profile::FromBrowserContext(
          handle->GetWebContents()->GetBrowserContext())) &&
      !handle->GetWebContents()->GetBrowserContext()->IsOffTheRecord()) {
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(handle->GetWebContents());
    if (!prerender_contents)
      return std::make_unique<AppsNavigationThrottle>(handle);
  }

  return nullptr;
}

// static
void AppsNavigationThrottle::ShowIntentPickerBubble(const Browser* browser,
                                                    const GURL& url) {
  // TODO(crbug.com/824598): ensure that |browser| is still alive when the
  // callback is run.
  arc::ArcNavigationThrottle::QueryArcApps(
      browser, url,
      base::BindOnce(&AppsNavigationThrottle::ShowIntentPickerBubbleForApps,
                     browser, url));
}

// static
bool AppsNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
    const GURL& previous_url,
    const GURL& current_url) {
  return ShouldOverrideUrlLoading(previous_url, current_url);
}

AppsNavigationThrottle::AppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      arc_throttle_(std::make_unique<arc::ArcNavigationThrottle>()),
      ui_displayed_(false),
      weak_factory_(this) {}

AppsNavigationThrottle::~AppsNavigationThrottle() = default;

const char* AppsNavigationThrottle::GetNameForLogging() {
  return "AppsNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
AppsNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  starting_url_ = GetStartingGURL(navigation_handle());
  Browser* browser =
      chrome::FindBrowserWithWebContents(navigation_handle()->GetWebContents());
  if (browser)
    chrome::SetIntentPickerViewVisibility(browser, false);
  return HandleRequest();
}

content::NavigationThrottle::ThrottleCheckResult
AppsNavigationThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(dominickn): Consider what to do when there is another URL during the
  // same navigation that could be handled by apps. Two ideas are:
  //  1) update the bubble with a mix of both app candidates (if different)
  //  2) show a bubble based on the last url, thus closing all the previous ones
  if (ui_displayed_)
    return content::NavigationThrottle::PROCEED;

  return HandleRequest();
}

// static
void AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
    const Browser* browser,
    const GURL& url,
    const std::vector<IntentPickerAppInfo>& apps) {
  if (apps.empty())
    return;

  // TODO(crbug.com/824598): move the IntentPickerResponse callback and
  // CloseReason enum/UMA to be in this class.
  chrome::QueryAndDisplayArcApps(
      browser, apps,
      base::Bind(&arc::ArcNavigationThrottle::OnIntentPickerClosed, url));
}

void AppsNavigationThrottle::CancelNavigation() {
  content::WebContents* tab = navigation_handle()->GetWebContents();
  if (tab && tab->GetController().IsInitialNavigation())
    tab->Close();
  else
    CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
}

void AppsNavigationThrottle::OnDeferredRequestProcessed(
    AppsNavigationAction action,
    const std::vector<IntentPickerAppInfo>& apps) {
  if (action == AppsNavigationAction::CANCEL) {
    // We found a preferred ARC app to open; cancel the navigation and don't do
    // anything else.
    CancelNavigation();
    return;
  }

  content::NavigationHandle* handle = navigation_handle();
  ShowIntentPickerBubbleForApps(
      chrome::FindBrowserWithWebContents(handle->GetWebContents()),
      handle->GetURL(), apps);

  // We are about to resume the navigation, which will destroy this object.
  ui_displayed_ = false;
  Resume();
}

content::NavigationThrottle::ThrottleCheckResult
AppsNavigationThrottle::HandleRequest() {
  DCHECK(!ui_displayed_);
  content::NavigationHandle* handle = navigation_handle();

  // Always handle http(s) <form> submissions in Chrome for two reasons: 1) we
  // don't have a way to send POST data to ARC, and 2) intercepting http(s) form
  // submissions is not very important because such submissions are usually
  // done within the same domain. ShouldOverrideUrlLoading() below filters out
  // such submissions anyway.
  constexpr bool kAllowFormSubmit = false;

  // Ignore navigations with the CLIENT_REDIRECT qualifier on.
  constexpr bool kAllowClientRedirect = false;

  // We must never handle navigations started within a context menu.
  if (handle->WasStartedFromContextMenu())
    return content::NavigationThrottle::PROCEED;

  if (arc::ShouldIgnoreNavigation(handle->GetPageTransition(), kAllowFormSubmit,
                                  kAllowClientRedirect)) {
    return content::NavigationThrottle::PROCEED;
  }

  if (!ShouldOverrideUrlLoading(starting_url_, handle->GetURL()))
    return content::NavigationThrottle::PROCEED;

  if (arc_throttle_->ShouldDeferRequest(
          handle,
          base::BindOnce(&AppsNavigationThrottle::OnDeferredRequestProcessed,
                         weak_factory_.GetWeakPtr()))) {
    // Handling is now deferred to |arc_throttle_|, which asynchronously queries
    // ARC for apps, and runs OnDeferredRequestProcessed() with an action based
    // on whether an acceptable app was found and user consent to open received.
    // We assume the UI is shown or a preferred app was found; reset to false if
    // we resume the navigation.
    ui_displayed_ = true;
    return content::NavigationThrottle::DEFER;
  }

  return content::NavigationThrottle::PROCEED;
}

}  // namespace chromeos
