// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/content_suggestions_alert_factory.h"

#import "ios/chrome/browser/content_suggestions/content_suggestions_alert_commands.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsAlertFactory

+ (AlertCoordinator*)
alertCoordinatorForSuggestionItem:(CollectionViewItem*)item
                 onViewController:(UIViewController*)viewController
                          atPoint:(CGPoint)touchLocation
                      atIndexPath:(NSIndexPath*)indexPath
                   commandHandler:
                       (id<ContentSuggestionsAlertCommands>)commandHandler {
  AlertCoordinator* alertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:viewController
                           title:nil
                         message:nil
                            rect:CGRectMake(touchLocation.x, touchLocation.y, 0,
                                            0)
                            view:[viewController view]];

  __weak CollectionViewItem* weakItem = item;
  __weak id<ContentSuggestionsAlertCommands> weakCommandHandler =
      commandHandler;

  NSString* openInNewTabTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  [alertCoordinator addItemWithTitle:openInNewTabTitle
                              action:^{
                                CollectionViewItem* strongItem = weakItem;
                                if (strongItem) {
                                  // TODO(crbug.com/691979): Add metrics.
                                  [weakCommandHandler
                                      openNewTabWithSuggestionsItem:strongItem
                                                          incognito:NO];
                                }
                              }
                               style:UIAlertActionStyleDefault];

  NSString* openInNewTabIncognitoTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
  [alertCoordinator addItemWithTitle:openInNewTabIncognitoTitle
                              action:^{
                                CollectionViewItem* strongItem = weakItem;
                                if (strongItem) {
                                  // TODO(crbug.com/691979): Add metrics.
                                  [weakCommandHandler
                                      openNewTabWithSuggestionsItem:strongItem
                                                          incognito:YES];
                                }
                              }
                               style:UIAlertActionStyleDefault];

  NSString* readLaterTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST);
  [alertCoordinator
      addItemWithTitle:readLaterTitle
                action:^{
                  CollectionViewItem* strongItem = weakItem;
                  if (strongItem) {
                    // TODO(crbug.com/691979): Add metrics.
                    [weakCommandHandler addItemToReadingList:strongItem];
                  }
                }
                 style:UIAlertActionStyleDefault];

  NSString* deleteTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_REMOVE);
  [alertCoordinator addItemWithTitle:deleteTitle
                              action:^{
                                CollectionViewItem* strongItem = weakItem;
                                if (strongItem) {
                                  // TODO(crbug.com/691979): Add metrics.
                                  [weakCommandHandler
                                      dismissSuggestion:strongItem
                                            atIndexPath:indexPath];
                                }
                              }
                               style:UIAlertActionStyleDestructive];

  [alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                              action:^{
                                // TODO(crbug.com/691979): Add metrics.
                              }
                               style:UIAlertActionStyleCancel];
  return alertCoordinator;
}

+ (AlertCoordinator*)
alertCoordinatorForMostVisitedItem:(CollectionViewItem*)item
                  onViewController:(UIViewController*)viewController
                           atPoint:(CGPoint)touchLocation
                       atIndexPath:(NSIndexPath*)indexPath
                    commandHandler:
                        (id<ContentSuggestionsAlertCommands>)commandHandler {
  AlertCoordinator* alertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:viewController
                           title:nil
                         message:nil
                            rect:CGRectMake(touchLocation.x, touchLocation.y, 0,
                                            0)
                            view:[viewController view]];

  __weak CollectionViewItem* weakItem = item;
  __weak id<ContentSuggestionsAlertCommands> weakCommandHandler =
      commandHandler;

  [alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                action:^{
                  CollectionViewItem* strongItem = weakItem;
                  if (strongItem) {
                    [weakCommandHandler
                        openNewTabWithMostVisitedItem:strongItem
                                            incognito:NO
                                              atIndex:indexPath.item];
                  }
                }
                 style:UIAlertActionStyleDefault];

  [alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                action:^{
                  CollectionViewItem* strongItem = weakItem;
                  if (strongItem) {
                    [weakCommandHandler
                        openNewTabWithMostVisitedItem:strongItem
                                            incognito:YES
                                              atIndex:indexPath.item];
                  }
                }
                 style:UIAlertActionStyleDefault];

  [alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)
                action:^{
                  CollectionViewItem* strongItem = weakItem;
                  if (strongItem) {
                    [weakCommandHandler removeMostVisited:strongItem];
                  }
                }
                 style:UIAlertActionStyleDestructive];

  [alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                              action:nil
                               style:UIAlertActionStyleCancel];

  return alertCoordinator;
}

@end
