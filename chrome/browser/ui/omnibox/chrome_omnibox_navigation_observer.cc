// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"

#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"


// Helpers --------------------------------------------------------------------

namespace {

// HTTP 2xx, 401, and 407 all indicate that the target address exists.
bool ResponseCodeIndicatesSuccess(int response_code) {
  return ((response_code / 100) == 2) || (response_code == 401) ||
         (response_code == 407);
}

// Returns true if |final_url| doesn't represent an ISP hijack of
// |original_url|, based on the IntranetRedirectDetector's RedirectOrigin().
bool IsValidNavigation(const GURL& original_url, const GURL& final_url) {
  const GURL& redirect_url(IntranetRedirectDetector::RedirectOrigin());
  return !redirect_url.is_valid() ||
         net::registry_controlled_domains::SameDomainOrHost(
             original_url, final_url,
             net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES) ||
         !net::registry_controlled_domains::SameDomainOrHost(
             final_url, redirect_url,
             net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

// ChromeOmniboxNavigationObserver --------------------------------------------

ChromeOmniboxNavigationObserver::ChromeOmniboxNavigationObserver(
    Profile* profile,
    const base::string16& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternate_nav_match)
    : text_(text),
      match_(match),
      alternate_nav_match_(alternate_nav_match),
      shortcuts_backend_(ShortcutsBackendFactory::GetForProfile(profile)),
      load_state_(LOAD_NOT_SEEN),
      fetch_state_(FETCH_NOT_COMPLETE) {
  if (alternate_nav_match_.destination_url.is_valid()) {
    fetcher_ = net::URLFetcher::Create(alternate_nav_match_.destination_url,
                                       net::URLFetcher::HEAD, this);
    fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
    fetcher_->SetStopOnRedirect(true);
  }
  // We need to start by listening to AllSources, since we don't know which tab
  // the navigation might occur in.
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                 content::NotificationService::AllSources());
}

ChromeOmniboxNavigationObserver::~ChromeOmniboxNavigationObserver() {}

void ChromeOmniboxNavigationObserver::OnSuccessfulNavigation() {
  if (shortcuts_backend_.get())
    shortcuts_backend_->AddOrUpdateShortcut(text_, match_);
}

bool ChromeOmniboxNavigationObserver::HasSeenPendingLoad() const {
  return load_state_ != LOAD_NOT_SEEN;
}

void ChromeOmniboxNavigationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_PENDING, type);

  // It's possible for an attempted omnibox navigation to cause the extensions
  // system to synchronously navigate an extension background page.  Not only is
  // this navigation not the one we want to observe, the associated WebContents
  // is invisible and has no InfoBarService, so trying to show an infobar in it
  // later will crash.  Just ignore this navigation and keep listening.
  content::NavigationController* controller =
      content::Source<content::NavigationController>(source).ptr();
  content::WebContents* web_contents = controller->GetWebContents();
  if (!InfoBarService::FromWebContents(web_contents))
    return;

  // Ignore navgiations to the wrong URL.
  // This shouldn't actually happen, but right now it's possible because the
  // prerenderer doesn't properly notify us when it swaps in a prerendered page.
  // Plus, the swap-in can trigger instant to kick off a new background
  // prerender, which we _do_ get notified about.  Once crbug.com/247848 is
  // fixed, this conditional should be able to be replaced with a [D]CHECK;
  // until then we ignore the incorrect navigation (and will be torn down
  // without having received the correct notification).
  if (match_.destination_url !=
      content::Details<content::NavigationEntry>(details)->GetVirtualURL())
    return;

  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                    content::NotificationService::AllSources());
  if (fetcher_) {
    fetcher_->SetRequestContext(
        controller->GetBrowserContext()->GetRequestContext());
  }
  WebContentsObserver::Observe(web_contents);
  // DidStartNavigationToPendingEntry() will be called for this load as well.
}

void ChromeOmniboxNavigationObserver::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  if (load_state_ == LOAD_NOT_SEEN) {
    load_state_ = LOAD_PENDING;
    if (fetcher_)
      fetcher_->Start();
  } else {
    delete this;
  }
}

void ChromeOmniboxNavigationObserver::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  if ((load_state_ != LOAD_COMMITTED) && !render_frame_host->GetParent())
    delete this;
}

void ChromeOmniboxNavigationObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  load_state_ = LOAD_COMMITTED;
  if (ResponseCodeIndicatesSuccess(load_details.http_status_code) &&
      IsValidNavigation(match_.destination_url,
                        load_details.entry->GetVirtualURL()))
    OnSuccessfulNavigation();
  if (!fetcher_ || (fetch_state_ != FETCH_NOT_COMPLETE))
    OnAllLoadingFinished();  // deletes |this|!
}

void ChromeOmniboxNavigationObserver::WebContentsDestroyed() {
  delete this;
}

void ChromeOmniboxNavigationObserver::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK_EQ(fetcher_.get(), source);
  const net::URLRequestStatus& status = source->GetStatus();
  int response_code = source->GetResponseCode();
  fetch_state_ =
      (status.is_success() && ResponseCodeIndicatesSuccess(response_code)) ||
              ((status.status() == net::URLRequestStatus::CANCELED) &&
               ((response_code / 100) == 3) &&
               IsValidNavigation(alternate_nav_match_.destination_url,
                                 source->GetURL()))
          ? FETCH_SUCCEEDED
          : FETCH_FAILED;
  if (load_state_ == LOAD_COMMITTED)
    OnAllLoadingFinished();  // deletes |this|!
}

void ChromeOmniboxNavigationObserver::OnAllLoadingFinished() {
  if (fetch_state_ == FETCH_SUCCEEDED) {
    AlternateNavInfoBarDelegate::Create(
        web_contents(), text_, alternate_nav_match_, match_.destination_url);
  }
  delete this;
}
