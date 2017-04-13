// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_helper_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "components/favicon/ios/web_favicon_driver.h"
#include "components/history/core/browser/top_sites.h"
#import "components/history/ios/browser/web_state_top_sites_observer.h"
#include "components/keyed_service/core/service_access_type.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/reading_list_web_state_observer.h"
#import "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/signin/account_consistency_service_factory.h"
#import "ios/chrome/browser/ssl/ios_security_state_tab_helper.h"
#import "ios/chrome/browser/store_kit/store_kit_tab_helper.h"
#import "ios/chrome/browser/sync/ios_chrome_synced_tab_delegate.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/web/blocked_popup_tab_helper.h"
#import "ios/chrome/browser/web/network_activity_indicator_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/web/public/web_state/web_state.h"

void AttachTabHelpers(web::WebState* web_state) {
  LegacyTabHelper::CreateForWebState(web_state);
  Tab* tab = LegacyTabHelper::GetTabForWebState(web_state);
  DCHECK(tab);

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());

  // IOSChromeSessionTabHelper comes first because it sets up the tab ID,
  // and other helpers may rely on that.
  IOSChromeSessionTabHelper::CreateForWebState(web_state);

  NetworkActivityIndicatorTabHelper::CreateForWebState(web_state, tab.tabId);
  IOSChromeSyncedTabDelegate::CreateForWebState(web_state);
  InfoBarManagerImpl::CreateForWebState(web_state);
  IOSSecurityStateTabHelper::CreateForWebState(web_state);
  RepostFormTabHelper::CreateForWebState(web_state);
  BlockedPopupTabHelper::CreateForWebState(web_state);
  FindTabHelper::CreateForWebState(web_state, tab.findInPageControllerDelegate);
  StoreKitTabHelper::CreateForWebState(web_state);
  SadTabTabHelper::CreateForWebState(web_state, tab);

  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserState(browser_state);
  ReadingListWebStateObserver::FromWebState(web_state, model);

  if (AccountConsistencyService* account_consistency_service =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              browser_state)) {
    account_consistency_service->SetWebStateHandler(web_state, tab);
  }
  ChromeIOSTranslateClient::CreateForWebState(web_state);

  ios::ChromeBrowserState* original_browser_state =
      browser_state->GetOriginalChromeBrowserState();
  favicon::WebFaviconDriver::CreateForWebState(
      web_state,
      ios::FaviconServiceFactory::GetForBrowserState(
          original_browser_state, ServiceAccessType::IMPLICIT_ACCESS),
      ios::HistoryServiceFactory::GetForBrowserState(
          original_browser_state, ServiceAccessType::IMPLICIT_ACCESS),
      ios::BookmarkModelFactory::GetForBrowserState(original_browser_state));
  history::WebStateTopSitesObserver::CreateForWebState(
      web_state,
      ios::TopSitesFactory::GetForBrowserState(original_browser_state).get());

  // Allow the embedder to attach tab helpers.
  ios::GetChromeBrowserProvider()->AttachTabHelpers(web_state, tab);

  // Allow the Tab to attach tab helper like objects (all those objects should
  // really be tab helpers and created above).
  [tab attachTabHelpers];
}
