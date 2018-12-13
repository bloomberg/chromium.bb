// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_model_delegate_ios.h"

#include "base/logging.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/ssl/ios_security_state_tab_helper.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

LocationBarModelDelegateIOS::LocationBarModelDelegateIOS(
    WebStateList* web_state_list)
    : web_state_list_(web_state_list) {}

LocationBarModelDelegateIOS::~LocationBarModelDelegateIOS() {}

web::WebState* LocationBarModelDelegateIOS::GetActiveWebState() const {
  return web_state_list_->GetActiveWebState();
}

web::NavigationItem* LocationBarModelDelegateIOS::GetNavigationItem() const {
  web::WebState* web_state = GetActiveWebState();
  web::NavigationManager* navigation_manager =
      web_state ? web_state->GetNavigationManager() : nullptr;
  return navigation_manager ? navigation_manager->GetVisibleItem() : nullptr;
}

base::string16
LocationBarModelDelegateIOS::FormattedStringWithEquivalentMeaning(
    const GURL& url,
    const base::string16& formatted_url) const {
  return AutocompleteInput::FormattedStringWithEquivalentMeaning(
      url, formatted_url, AutocompleteSchemeClassifierImpl(), nullptr);
}

bool LocationBarModelDelegateIOS::GetURL(GURL* url) const {
  DCHECK(url);
  web::NavigationItem* item = GetNavigationItem();
  if (!item)
    return false;
  *url = ShouldDisplayURL() ? item->GetVirtualURL() : GURL::EmptyGURL();
  return true;
}

bool LocationBarModelDelegateIOS::ShouldDisplayURL() const {
  web::NavigationItem* item = GetNavigationItem();
  if (item) {
    GURL url = item->GetURL();
    GURL virtual_url = item->GetVirtualURL();
    if (url.SchemeIs(kChromeUIScheme) ||
        virtual_url.SchemeIs(kChromeUIScheme)) {
      if (!url.SchemeIs(kChromeUIScheme))
        url = virtual_url;
      base::StringPiece host = url.host_piece();
      return host != kChromeUINewTabHost;
    }
  }
  return true;
}

void LocationBarModelDelegateIOS::GetSecurityInfo(
    security_state::SecurityInfo* result) const {
  web::WebState* web_state = GetActiveWebState();
  // If there is no active WebState (which can happen during toolbar
  // initialization), assume no security style.
  if (!web_state) {
    *result = security_state::SecurityInfo();
    return;
  }
  auto* client = IOSSecurityStateTabHelper::FromWebState(web_state);
  client->GetSecurityInfo(result);
}

scoped_refptr<net::X509Certificate>
LocationBarModelDelegateIOS::GetCertificate() const {
  web::NavigationItem* item = GetNavigationItem();
  if (item)
    return item->GetSSL().certificate;
  return scoped_refptr<net::X509Certificate>();
}

const gfx::VectorIcon* LocationBarModelDelegateIOS::GetVectorIconOverride()
    const {
  return nullptr;
}

bool LocationBarModelDelegateIOS::IsOfflinePage() const {
  web::WebState* web_state = GetActiveWebState();
  if (!web_state)
    return false;
  auto* navigationManager = web_state->GetNavigationManager();
  auto* visibleItem = navigationManager->GetVisibleItem();
  if (!visibleItem)
    return false;
  const GURL& url = visibleItem->GetURL();
  return url.SchemeIs(kChromeUIScheme) && url.host() == kChromeUIOfflineHost;
}
