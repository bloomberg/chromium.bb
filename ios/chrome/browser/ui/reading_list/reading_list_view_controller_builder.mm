// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller_builder.h"

#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ReadingListViewControllerBuilder

+ (ReadingListViewController*)
readingListViewControllerInBrowserState:(ios::ChromeBrowserState*)browserState
                                 loader:(id<UrlLoader>)loader {
  ReadingListModel* model =
      ReadingListModelFactory::GetInstance()->GetForBrowserState(browserState);
  favicon::LargeIconService* service =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(browserState);
  ReadingListDownloadService* rlservice =
      ReadingListDownloadServiceFactory::GetInstance()->GetForBrowserState(
          browserState);
  ReadingListViewController* vc =
      [[ReadingListViewController alloc] initWithModel:model
                                                loader:loader
                                      largeIconService:service
                            readingListDownloadService:rlservice];
  return vc;
}

@end
