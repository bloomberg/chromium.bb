// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ALERT_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ALERT_FACTORY_H_

#import <UIKit/UIKit.h>

@class AlertCoordinator;
@class CollectionViewItem;
@protocol ContentSuggestionsAlertCommands;

// Factory for AlertCoordinators for ContentSuggestions.
@interface ContentSuggestionsAlertFactory : NSObject

// Returns an AlertCoordinator for a suggestions |item| with the indexPath
// |indexPath|. The alert will be presented on the |viewController| at the
// |touchLocation|, in the coordinates of the |viewController|'s view. The
// |commandHandler| will receive callbacks when the user chooses one of the
// options displayed by the alert.
+ (AlertCoordinator*)
alertCoordinatorForSuggestionItem:(CollectionViewItem*)item
                 onViewController:(UIViewController*)viewController
                          atPoint:(CGPoint)touchLocation
                      atIndexPath:(NSIndexPath*)indexPath
                   commandHandler:
                       (id<ContentSuggestionsAlertCommands>)commandHandler;

// Same as above but for a MostVisited item.
+ (AlertCoordinator*)
alertCoordinatorForMostVisitedItem:(CollectionViewItem*)item
                  onViewController:(UIViewController*)viewController
                           atPoint:(CGPoint)touchLocation
                       atIndexPath:(NSIndexPath*)indexPath
                    commandHandler:
                        (id<ContentSuggestionsAlertCommands>)commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ALERT_FACTORY_H_
