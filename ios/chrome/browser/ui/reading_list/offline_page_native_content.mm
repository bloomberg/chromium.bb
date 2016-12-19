// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/offline_page_native_content.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/reading_list/core/reading_list_switches.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_entry_loading_util.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#include "ios/web/public/browser_state.h"
#include "net/base/network_change_notifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark -
#pragma mark Public

@implementation OfflinePageNativeContent {
  // The virtual URL that will be displayed to the user.
  GURL _virtualURL;

  // The Reading list model needed to reload the entry.
  ReadingListModel* _model;

  // The WebState of the current tab.
  web::WebState* _webState;
}

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(web::BrowserState*)browserState
                      webState:(web::WebState*)webState
                           URL:(const GURL&)URL {
  DCHECK(loader);
  DCHECK(browserState);
  DCHECK(URL.is_valid());

  if (reading_list::switches::IsReadingListEnabled()) {
    _model = ReadingListModelFactory::GetForBrowserState(
        ios::ChromeBrowserState::FromBrowserState(browserState));
  }
  _webState = webState;
  GURL resourcesRoot;
  GURL fileURL = reading_list::FileURLForDistilledURL(
      URL, browserState->GetStatePath(), &resourcesRoot);

  StaticHtmlViewController* HTMLViewController =
      [[StaticHtmlViewController alloc] initWithFileURL:fileURL
                                allowingReadAccessToURL:resourcesRoot
                                           browserState:browserState];
  _virtualURL = reading_list::VirtualURLForDistilledURL(URL);

  return [super initWithLoader:loader
      staticHTMLViewController:HTMLViewController
                           URL:URL];
}

- (GURL)virtualURL {
  return _virtualURL;
}

- (void)reload {
  if (!_model || net::NetworkChangeNotifier::IsOffline()) {
    [super reload];
    return;
  }
  const ReadingListEntry* entry = _model->GetEntryByURL([self virtualURL]);
  if (entry) {
    reading_list::LoadReadingListEntry(*entry, _model, _webState);
  } else {
    [super reload];
  }
}

@end
