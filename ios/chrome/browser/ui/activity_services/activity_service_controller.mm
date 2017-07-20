// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"
#import "ios/chrome/browser/ui/activity_services/appex_constants.h"
#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_source.h"
#import "ios/chrome/browser/ui/activity_services/print_activity.h"
#import "ios/chrome/browser/ui/activity_services/reading_list_activity.h"
#import "ios/chrome/browser/ui/activity_services/share_protocol.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#include "ios/chrome/browser/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ActivityServiceController () {
  BOOL active_;
  id<ShareToDelegate> shareToDelegate_;
  UIActivityViewController* activityViewController_;
}

// Resets the controller's user interface and delegate.
- (void)resetUserInterface;
// Called when UIActivityViewController user interface is dismissed by user
// signifying the end of the Share/Action activity.
- (void)shareFinishedWithActivityType:(NSString*)activityType
                            completed:(BOOL)completed
                        returnedItems:(NSArray*)returnedItems
                                error:(NSError*)activityError;
// Returns an array of UIActivityItemSource objects to provide the |data| to
// share to the sharing activities.
- (NSArray*)activityItemsForData:(ShareToData*)data;
// Returns an array of UIActivity objects that can handle the given |data|.
- (NSArray*)applicationActivitiesForData:(ShareToData*)data
                              controller:(UIViewController*)controller
                              dispatcher:(id<BrowserCommands>)dispatcher;
// Processes |extensionItems| returned from App Extension invocation returning
// the |activityType|. Calls shareDelegate_ with the processed returned items
// and |result| of activity. Returns whether caller should reset UI.
- (BOOL)processItemsReturnedFromActivity:(NSString*)activityType
                                  status:(ShareTo::ShareResult)result
                                   items:(NSArray*)extensionItems;
@end

@implementation ActivityServiceController

+ (ActivityServiceController*)sharedInstance {
  static ActivityServiceController* instance =
      [[ActivityServiceController alloc] init];
  return instance;
}

#pragma mark - ShareProtocol

- (BOOL)isActive {
  return active_;
}

- (void)cancelShareAnimated:(BOOL)animated {
  if (!active_) {
    return;
  }
  DCHECK(activityViewController_);
  // There is no guarantee that the completion callback will be called because
  // the |activityViewController_| may have been dismissed already. For example,
  // if the user selects Facebook Share Extension, the UIActivityViewController
  // is first dismissed and then the UI for Facebook Share Extension comes up.
  // At this time, if the user backgrounds Chrome and then relaunch Chrome
  // through an external app (e.g. with googlechrome://url.com), Chrome restart
  // dismisses the modal UI coming through this path. But since the
  // UIActivityViewController has already been dismissed, the following method
  // does nothing and completion callback is not called. The call
  // -shareFinishedWithActivityType:completed:returnedItems:error: must be
  // called explicitly to do the clean up or else future attempts to use
  // Share will fail.
  [activityViewController_ dismissViewControllerAnimated:animated
                                              completion:nil];
  [self shareFinishedWithActivityType:nil
                            completed:NO
                        returnedItems:nil
                                error:nil];
}

- (void)shareWithData:(ShareToData*)data
           controller:(UIViewController*)controller
         browserState:(ios::ChromeBrowserState*)browserState
           dispatcher:(id<BrowserCommands>)dispatcher
      shareToDelegate:(id<ShareToDelegate>)delegate
             fromRect:(CGRect)fromRect
               inView:(UIView*)inView {
  DCHECK(controller);
  DCHECK(data);
  DCHECK(!active_);
  DCHECK(!shareToDelegate_);
  if (IsIPadIdiom()) {
    DCHECK(fromRect.size.height);
    DCHECK(fromRect.size.width);
    DCHECK(inView);
  }

  DCHECK(!activityViewController_);
  shareToDelegate_ = delegate;
  activityViewController_ = [[UIActivityViewController alloc]
      initWithActivityItems:[self activityItemsForData:data]
      applicationActivities:[self applicationActivitiesForData:data
                                                    controller:controller
                                                    dispatcher:dispatcher]];

  // Reading List and Print activities refer to iOS' version of these.
  // Chrome-specific implementations of these two activities are provided below
  // in applicationActivitiesForData:controller:dispatcher:
  NSArray* excludedActivityTypes = @[
    UIActivityTypeAddToReadingList, UIActivityTypePrint,
    UIActivityTypeSaveToCameraRoll
  ];
  [activityViewController_ setExcludedActivityTypes:excludedActivityTypes];

  __weak ActivityServiceController* weakSelf = self;
  [activityViewController_ setCompletionWithItemsHandler:^(
                               NSString* activityType, BOOL completed,
                               NSArray* returnedItems, NSError* activityError) {
    [weakSelf shareFinishedWithActivityType:activityType
                                  completed:completed
                              returnedItems:returnedItems
                                      error:activityError];
  }];

  active_ = YES;
  activityViewController_.modalPresentationStyle = UIModalPresentationPopover;
  activityViewController_.popoverPresentationController.sourceView = inView;
  activityViewController_.popoverPresentationController.sourceRect = fromRect;
  [controller presentViewController:activityViewController_
                           animated:YES
                         completion:nil];
}

#pragma mark - Private

- (void)resetUserInterface {
  shareToDelegate_ = nil;
  activityViewController_ = nil;
  active_ = NO;
}

- (void)shareFinishedWithActivityType:(NSString*)activityType
                            completed:(BOOL)completed
                        returnedItems:(NSArray*)returnedItems
                                error:(NSError*)activityError {
  DCHECK(active_);
  DCHECK(shareToDelegate_);

  BOOL shouldResetUI = YES;
  if (activityType) {
    ShareTo::ShareResult shareResult = completed
                                           ? ShareTo::ShareResult::SHARE_SUCCESS
                                           : ShareTo::ShareResult::SHARE_CANCEL;
    if (activity_type_util::TypeFromString(activityType) ==
        activity_type_util::APPEX_PASSWORD_MANAGEMENT) {
      // A compatible Password Management App Extension was invoked.
      shouldResetUI = [self processItemsReturnedFromActivity:activityType
                                                      status:shareResult
                                                       items:returnedItems];
    } else {
      activity_type_util::ActivityType type =
          activity_type_util::TypeFromString(activityType);
      activity_type_util::RecordMetricForActivity(type);
      NSString* completionMessage =
          activity_type_util::CompletionMessageForActivity(type);
      [shareToDelegate_ shareDidComplete:shareResult
                       completionMessage:completionMessage];
    }
  } else {
    [shareToDelegate_ shareDidComplete:ShareTo::ShareResult::SHARE_CANCEL
                     completionMessage:nil];
  }
  if (shouldResetUI)
    [self resetUserInterface];
}

- (NSArray*)activityItemsForData:(ShareToData*)data {
  NSMutableArray* activityItems = [NSMutableArray array];
  // ShareToData object guarantees that there is a NSURL.
  DCHECK(data.nsurl);

  // In order to support find-login-action protocol, the provider object
  // UIActivityURLSource supports both Password Management App Extensions
  // (e.g. 1Password) and also provide a public.url UTType for Share Extensions
  // (e.g. Facebook, Twitter).
  UIActivityURLSource* loginActionProvider =
      [[UIActivityURLSource alloc] initWithURL:data.nsurl
                                       subject:data.title
                            thumbnailGenerator:data.thumbnailGenerator];
  [activityItems addObject:loginActionProvider];

  UIActivityTextSource* textProvider =
      [[UIActivityTextSource alloc] initWithText:data.title];
  [activityItems addObject:textProvider];

  if (data.image) {
    UIActivityImageSource* imageProvider =
        [[UIActivityImageSource alloc] initWithImage:data.image];
    [activityItems addObject:imageProvider];
  }

  return activityItems;
}

- (NSArray*)applicationActivitiesForData:(ShareToData*)data
                              controller:(UIViewController*)controller
                              dispatcher:(id<BrowserCommands>)dispatcher {
  NSMutableArray* applicationActivities = [NSMutableArray array];
  if (data.isPagePrintable) {
    PrintActivity* printActivity = [[PrintActivity alloc] init];
    printActivity.dispatcher = dispatcher;
    [applicationActivities addObject:printActivity];
  }
  if (data.url.SchemeIsHTTPOrHTTPS()) {
    ReadingListActivity* readingListActivity =
        [[ReadingListActivity alloc] initWithURL:data.url
                                           title:data.title
                                       responder:controller];
    [applicationActivities addObject:readingListActivity];
  }
  return applicationActivities;
}

- (BOOL)processItemsReturnedFromActivity:(NSString*)activityType
                                  status:(ShareTo::ShareResult)result
                                   items:(NSArray*)extensionItems {
  NSItemProvider* itemProvider = nil;
  if ([extensionItems count] > 0) {
    // Based on calling convention described in
    // https://github.com/AgileBits/onepassword-app-extension/blob/master/OnePasswordExtension.m
    // the username/password is always in the first element of the returned
    // item.
    NSExtensionItem* extensionItem = extensionItems[0];
    // Checks that there is at least one attachment and that the attachment
    // is a property list which can be converted into a NSDictionary object.
    // If not, early return.
    if (extensionItem.attachments.count > 0) {
      itemProvider = [extensionItem.attachments objectAtIndex:0];
      if (![itemProvider
              hasItemConformingToTypeIdentifier:(NSString*)kUTTypePropertyList])
        itemProvider = nil;
    }
  }
  if (!itemProvider) {
    // ShareToDelegate callback method must still be called on incorrect
    // |extensionItems|.
    [shareToDelegate_ passwordAppExDidFinish:ShareTo::ShareResult::SHARE_ERROR
                                    username:nil
                                    password:nil
                           completionMessage:nil];
    return YES;
  }

  // |completionHandler| is the block that will be executed once the
  // property list has been loaded from the attachment.
  void (^completionHandler)(id, NSError*) = ^(id item, NSError* error) {
    ShareTo::ShareResult activityResult = result;
    NSString* username = nil;
    NSString* password = nil;
    NSString* message = nil;
    NSDictionary* loginDictionary = base::mac::ObjCCast<NSDictionary>(item);
    if (error || !loginDictionary) {
      activityResult = ShareTo::ShareResult::SHARE_ERROR;
    } else {
      username = loginDictionary[activity_services::kPasswordAppExUsernameKey];
      password = loginDictionary[activity_services::kPasswordAppExPasswordKey];
      activity_type_util::ActivityType type =
          activity_type_util::TypeFromString(activityType);
      activity_type_util::RecordMetricForActivity(type);
      message = activity_type_util::CompletionMessageForActivity(type);
    }
    // Password autofill uses JavaScript injection which must be executed on
    // the main thread, however,
    // loadItemForTypeIdentifier:options:completionHandler: documentation states
    // that completion block "may  be executed on a background thread", so the
    // code to do password filling must be re-dispatched back to main thread.
    // Completion block intentionally retains |self|.
    dispatch_async(dispatch_get_main_queue(), ^{
      [shareToDelegate_ passwordAppExDidFinish:activityResult
                                      username:username
                                      password:password
                             completionMessage:message];
      // Controller state can be reset only after delegate has
      // processed the item returned from the App Extension.
      [self resetUserInterface];
    });
  };
  [itemProvider loadItemForTypeIdentifier:(NSString*)kUTTypePropertyList
                                  options:nil
                        completionHandler:completionHandler];
  return NO;
}

#pragma mark - For Testing

- (void)setShareToDelegateForTesting:(id<ShareToDelegate>)delegate {
  shareToDelegate_ = delegate;
}

@end
