// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_mediator.h"

#import <MobileCoreServices/MobileCoreServices.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/ui/activity_services/activities/bookmark_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/copy_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/find_in_page_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/generate_qr_code_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/print_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/reading_list_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/request_desktop_or_mobile_site_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/send_tab_to_self_activity.h"
#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"
#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_image_source.h"
#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_item_source.h"
#import "ios/chrome/browser/ui/activity_services/data/chrome_activity_url_source.h"
#import "ios/chrome/browser/ui/activity_services/data/share_image_data.h"
#import "ios/chrome/browser/ui/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ActivityServiceMediator ()

@property(nonatomic, weak)
    id<BrowserCommands, FindInPageCommands, QRGenerationCommands>
        handler;

@property(nonatomic, assign) PrefService* prefService;

@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;

@end

@implementation ActivityServiceMediator

#pragma mark - Public

- (instancetype)
    initWithHandler:
        (id<BrowserCommands, FindInPageCommands, QRGenerationCommands>)handler
        prefService:(PrefService*)prefService
      bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel {
  if (self = [super init]) {
    _handler = handler;
    _prefService = prefService;
    _bookmarkModel = bookmarkModel;
  }
  return self;
}

- (NSArray<ChromeActivityURLSource*>*)activityItemsForData:(ShareToData*)data {
  // The provider object ChromeActivityURLSource supports the public.url UTType
  // for Share Extensions (e.g. Facebook, Twitter).
  ChromeActivityURLSource* urlActivitySource = [[ChromeActivityURLSource alloc]
        initWithShareURL:data.shareNSURL
                 subject:data.title
      thumbnailGenerator:data.thumbnailGenerator];
  return @[ urlActivitySource ];
}

- (NSArray*)applicationActivitiesForData:(ShareToData*)data {
  NSMutableArray* applicationActivities = [NSMutableArray array];

  [applicationActivities
      addObject:[[CopyActivity alloc] initWithURL:data.shareURL]];

  if (data.shareURL.SchemeIsHTTPOrHTTPS()) {
    SendTabToSelfActivity* sendTabToSelfActivity =
        [[SendTabToSelfActivity alloc] initWithData:data handler:self.handler];
    [applicationActivities addObject:sendTabToSelfActivity];

    ReadingListActivity* readingListActivity =
        [[ReadingListActivity alloc] initWithURL:data.shareURL
                                           title:data.title
                                      dispatcher:self.handler];
    [applicationActivities addObject:readingListActivity];

    BookmarkActivity* bookmarkActivity =
        [[BookmarkActivity alloc] initWithURL:data.visibleURL
                                bookmarkModel:self.bookmarkModel
                                      handler:self.handler
                                  prefService:self.prefService];
    [applicationActivities addObject:bookmarkActivity];

    GenerateQrCodeActivity* generateQrCodeActivity =
        [[GenerateQrCodeActivity alloc] initWithURL:data.shareURL
                                              title:data.title
                                            handler:self.handler];
    [applicationActivities addObject:generateQrCodeActivity];

    FindInPageActivity* findInPageActivity =
        [[FindInPageActivity alloc] initWithData:data handler:self.handler];
    [applicationActivities addObject:findInPageActivity];

    RequestDesktopOrMobileSiteActivity* requestActivity =
        [[RequestDesktopOrMobileSiteActivity alloc]
            initWithUserAgent:data.userAgent
                      handler:self.handler];
    [applicationActivities addObject:requestActivity];
  }
  PrintActivity* printActivity =
      [[PrintActivity alloc] initWithData:data handler:self.handler];
  [applicationActivities addObject:printActivity];
  return applicationActivities;
}

- (NSArray<ChromeActivityImageSource*>*)activityItemsForImageData:
    (ShareImageData*)data {
  return @[ [[ChromeActivityImageSource alloc] initWithImage:data.image
                                                       title:data.title] ];
}

- (NSArray*)applicationActivitiesForImageData:(ShareImageData*)data {
  // For images, we're using the native activities.
  return @[];
}

- (NSSet*)excludedActivityTypesForItems:
    (NSArray<id<ChromeActivityItemSource>>*)items {
  NSMutableSet* mutableSet = [[NSMutableSet alloc] init];
  for (id<ChromeActivityItemSource> item in items) {
    [mutableSet unionSet:item.excludedActivityTypes];
  }
  return mutableSet;
}

- (void)shareFinishedWithActivityType:(NSString*)activityType
                            completed:(BOOL)completed
                        returnedItems:(NSArray*)returnedItems
                                error:(NSError*)activityError {
  if (activityType) {
    activity_type_util::ActivityType type =
        activity_type_util::TypeFromString(activityType);
    activity_type_util::RecordMetricForActivity(type);
  }

  if (!activityType || !completed) {
    // Share action was cancelled.
    base::RecordAction(base::UserMetricsAction("MobileShareMenuCancel"));
  }
}

@end
