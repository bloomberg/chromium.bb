// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
void NewTabPageTabHelper::CreateForWebState(
    web::WebState* web_state,
    WebStateList* web_state_list,
    id<NewTabPageTabHelperDelegate> delegate,
    id<UrlLoader> url_loader,
    id<NewTabPageControllerDelegate> toolbar_delegate,
    id<ApplicationCommands,
       BrowserCommands,
       OmniboxFocuser,
       FakeboxFocuser,
       SnackbarCommands,
       UrlLoader> dispatcher) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new NewTabPageTabHelper(
                               web_state, web_state_list, delegate, url_loader,
                               toolbar_delegate, dispatcher)));
  }
}

NewTabPageTabHelper::~NewTabPageTabHelper() = default;

NewTabPageTabHelper::NewTabPageTabHelper(
    web::WebState* web_state,
    WebStateList* web_state_list,
    id<NewTabPageTabHelperDelegate> delegate,
    id<UrlLoader> url_loader,
    id<NewTabPageControllerDelegate> toolbar_delegate,
    id<ApplicationCommands,
       BrowserCommands,
       OmniboxFocuser,
       FakeboxFocuser,
       SnackbarCommands,
       UrlLoader> dispatcher)
    : delegate_(delegate) {
  DCHECK(delegate);
  DCHECK(base::FeatureList::IsEnabled(kBrowserContainerContainsNTP));

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());

  ntp_coordinator_ =
      [[NewTabPageCoordinator alloc] initWithBrowserState:browser_state];
  ntp_coordinator_.webStateList = web_state_list;
  ntp_coordinator_.dispatcher = dispatcher;
  ntp_coordinator_.URLLoader = url_loader;
  ntp_coordinator_.toolbarDelegate = toolbar_delegate;

  web_state->AddObserver(this);

  if (web_state->GetVisibleURL().GetOrigin() == kChromeUINewTabURL) {
    [ntp_coordinator_ start];
  }
}

bool NewTabPageTabHelper::IsActive() const {
  return ntp_coordinator_.started;
}

UIViewController* NewTabPageTabHelper::GetViewController() const {
  DCHECK(IsActive());
  return ntp_coordinator_.viewController;
}

id<NewTabPageOwning> NewTabPageTabHelper::GetController() const {
  return ntp_coordinator_;
}

void NewTabPageTabHelper::DismissModals() const {
  return [ntp_coordinator_ dismissModals];
}

#pragma mark - WebStateObserver

void NewTabPageTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

void NewTabPageTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  // Save the NTP scroll offset before we navigate away.
  web::NavigationManager* manager = web_state->GetNavigationManager();
  if (web_state->GetLastCommittedURL().GetOrigin() == kChromeUINewTabURL) {
    DCHECK(IsActive());
    web::NavigationItem* item = manager->GetLastCommittedItem();
    web::PageDisplayState displayState;
    CGPoint scrollOffset = ntp_coordinator_.scrollOffset;
    displayState.scroll_state().set_offset_x(scrollOffset.x);
    displayState.scroll_state().set_offset_y(scrollOffset.y);
    item->SetPageDisplayState(displayState);
  }

  bool was_active = IsActive();

  // Start or stop the NTP.
  web::NavigationItem* item = manager->GetPendingItem();
  if (item && item->GetURL().GetOrigin() == kChromeUINewTabURL) {
    item->SetTitle(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
    [ntp_coordinator_ start];
  } else {
    [ntp_coordinator_ stop];
  }

  // Tell |delegate_| to show or hide the NTP, if necessary.
  if (IsActive() != was_active) {
    [delegate_ newTabPageHelperDidChangeVisibility:this];
  }
}
