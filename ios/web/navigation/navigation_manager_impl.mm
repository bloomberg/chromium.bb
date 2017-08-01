// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#include "base/memory/ptr_util.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/public/web_client.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

NavigationManager::WebLoadParams::WebLoadParams(const GURL& url)
    : url(url),
      transition_type(ui::PAGE_TRANSITION_LINK),
      user_agent_override_option(UserAgentOverrideOption::INHERIT),
      is_renderer_initiated(false),
      post_data(nil) {}

NavigationManager::WebLoadParams::~WebLoadParams() {}

NavigationManager::WebLoadParams::WebLoadParams(const WebLoadParams& other)
    : url(other.url),
      referrer(other.referrer),
      transition_type(other.transition_type),
      user_agent_override_option(other.user_agent_override_option),
      is_renderer_initiated(other.is_renderer_initiated),
      extra_headers([other.extra_headers copy]),
      post_data([other.post_data copy]) {}

NavigationManager::WebLoadParams& NavigationManager::WebLoadParams::operator=(
    const WebLoadParams& other) {
  url = other.url;
  referrer = other.referrer;
  is_renderer_initiated = other.is_renderer_initiated;
  transition_type = other.transition_type;
  user_agent_override_option = other.user_agent_override_option;
  extra_headers.reset([other.extra_headers copy]);
  post_data.reset([other.post_data copy]);

  return *this;
}

/* static */
bool NavigationManagerImpl::IsFragmentChangeNavigationBetweenUrls(
    const GURL& existing_url,
    const GURL& new_url) {
  // TODO(crbug.com/749542): Current implementation incorrectly returns false
  // if URL changes from http://google.com#foo to http://google.com.
  if (existing_url == new_url || !new_url.has_ref())
    return false;

  return existing_url.EqualsIgnoringRef(new_url);
}

/* static */
void NavigationManagerImpl::UpdatePendingItemUserAgentType(
    UserAgentOverrideOption user_agent_override_option,
    const NavigationItem* inherit_from_item,
    NavigationItem* pending_item) {
  DCHECK(pending_item);

  // |user_agent_override_option| must be INHERIT if |pending_item|'s
  // UserAgentType is NONE, as requesting a desktop or mobile user agent should
  // be disabled for app-specific URLs.
  DCHECK(pending_item->GetUserAgentType() != UserAgentType::NONE ||
         user_agent_override_option == UserAgentOverrideOption::INHERIT);

  // Newly created pending items are created with UserAgentType::NONE for native
  // pages or UserAgentType::MOBILE for non-native pages.  If the pending item's
  // URL is non-native, check which user agent type it should be created with
  // based on |user_agent_override_option|.
  DCHECK_NE(UserAgentType::DESKTOP, pending_item->GetUserAgentType());
  if (pending_item->GetUserAgentType() == UserAgentType::NONE)
    return;

  switch (user_agent_override_option) {
    case UserAgentOverrideOption::DESKTOP:
      pending_item->SetUserAgentType(UserAgentType::DESKTOP);
      break;
    case UserAgentOverrideOption::MOBILE:
      pending_item->SetUserAgentType(UserAgentType::MOBILE);
      break;
    case UserAgentOverrideOption::INHERIT: {
      // Propagate the last committed non-native item's UserAgentType if there
      // is one, otherwise keep the default value, which is mobile.
      DCHECK(!inherit_from_item ||
             inherit_from_item->GetUserAgentType() != UserAgentType::NONE);
      if (inherit_from_item) {
        pending_item->SetUserAgentType(inherit_from_item->GetUserAgentType());
      }
      break;
    }
  }
}

NavigationManagerImpl::NavigationManagerImpl()
    : delegate_(nullptr), browser_state_(nullptr) {}

NavigationManagerImpl::~NavigationManagerImpl() = default;

void NavigationManagerImpl::SetDelegate(NavigationManagerDelegate* delegate) {
  delegate_ = delegate;
}

void NavigationManagerImpl::SetBrowserState(BrowserState* browser_state) {
  browser_state_ = browser_state;
}

void NavigationManagerImpl::RemoveTransientURLRewriters() {
  transient_url_rewriters_.clear();
}

std::unique_ptr<NavigationItemImpl> NavigationManagerImpl::CreateNavigationItem(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    NavigationInitiationType initiation_type) {
  GURL loaded_url(url);

  bool url_was_rewritten = false;
  if (!transient_url_rewriters_.empty()) {
    url_was_rewritten = web::BrowserURLRewriter::RewriteURLWithWriters(
        &loaded_url, browser_state_, transient_url_rewriters_);
  }

  if (!url_was_rewritten) {
    web::BrowserURLRewriter::GetInstance()->RewriteURLIfNecessary(
        &loaded_url, browser_state_);
  }

  if (initiation_type == web::NavigationInitiationType::RENDERER_INITIATED &&
      loaded_url != url && web::GetWebClient()->IsAppSpecificURL(loaded_url)) {
    const NavigationItem* last_committed_item = GetLastCommittedItem();
    bool last_committed_url_is_app_specific =
        last_committed_item &&
        web::GetWebClient()->IsAppSpecificURL(last_committed_item->GetURL());
    if (!last_committed_url_is_app_specific) {
      // The URL should not be changed to app-specific URL if the load was
      // renderer-initiated requested by non app-specific URL. Pages with
      // app-specific urls have elevated previledges and should not be allowed
      // to open app-specific URLs.
      loaded_url = url;
    }
  }

  RemoveTransientURLRewriters();

  auto item = base::MakeUnique<NavigationItemImpl>();
  item->SetOriginalRequestURL(loaded_url);
  item->SetURL(loaded_url);
  item->SetReferrer(referrer);
  item->SetTransitionType(transition);
  item->SetNavigationInitiationType(initiation_type);
  if (web::GetWebClient()->IsAppSpecificURL(loaded_url)) {
    item->SetUserAgentType(web::UserAgentType::NONE);
  }

  return item;
}

void NavigationManagerImpl::AddTransientURLRewriter(
    BrowserURLRewriter::URLRewriter rewriter) {
  DCHECK(rewriter);
  transient_url_rewriters_.push_back(rewriter);
}

void NavigationManagerImpl::Reload(ReloadType reload_type,
                                   bool check_for_reposts) {
  if (!GetTransientItem() && !GetPendingItem() && !GetLastCommittedItem())
    return;

  // Reload with ORIGINAL_REQUEST_URL type should reload with the original
  // request url of the transient item, or pending item if transient doesn't
  // exist, or last committed item if both of them don't exist. The reason is
  // that a server side redirect may change the item's url.
  // For example, the user visits www.chromium.org and is then redirected
  // to m.chromium.org, when the user wants to refresh the page with a different
  // configuration (e.g. user agent), the user would be expecting to visit
  // www.chromium.org instead of m.chromium.org.
  if (reload_type == web::ReloadType::ORIGINAL_REQUEST_URL) {
    NavigationItem* reload_item = nullptr;
    if (GetTransientItem())
      reload_item = GetTransientItem();
    else if (GetPendingItem())
      reload_item = GetPendingItem();
    else
      reload_item = GetLastCommittedItem();
    DCHECK(reload_item);

    reload_item->SetURL(reload_item->GetOriginalRequestURL());
  }

  delegate_->Reload();
}

}  // namespace web
