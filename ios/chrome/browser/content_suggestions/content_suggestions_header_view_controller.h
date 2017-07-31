// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/content_suggestions_header_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_controlling.h"
#import "ios/chrome/browser/ui/ntp/google_landing_consumer.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_owner.h"
#import "ios/public/provider/chrome/browser/voice/logo_animation_controller.h"

@protocol ApplicationCommands;
@protocol ContentSuggestionsCollectionSynchronizing;
@protocol ContentSuggestionsHeaderViewControllerDelegate;
@protocol ContentSuggestionsHeaderViewControllerCommandHandler;
@protocol OmniboxFocuser;
@protocol UrlLoader;
class ReadingListModel;

// Controller for the header containing the logo and the fake omnibox, handling
// the interactions between the header and the collection, and the rest of the
// application.
@interface ContentSuggestionsHeaderViewController
    : UIViewController<ContentSuggestionsHeaderControlling,
                       ContentSuggestionsHeaderProvider,
                       GoogleLandingConsumer,
                       ToolbarOwner,
                       LogoAnimationControllerOwnerOwner>

@property(nonatomic, weak) id<ApplicationCommands, OmniboxFocuser, UrlLoader>
    dispatcher;
@property(nonatomic, weak) id<ContentSuggestionsHeaderViewControllerDelegate>
    delegate;
@property(nonatomic, weak)
    id<ContentSuggestionsHeaderViewControllerCommandHandler>
        commandHandler;
@property(nonatomic, weak) id<ContentSuggestionsCollectionSynchronizing>
    collectionSynchronizer;
@property(nonatomic, assign) ReadingListModel* readingListModel;

// Whether the Google logo or doodle is being shown.
@property(nonatomic, assign) BOOL logoIsShowing;

// |YES| if its view is visible.  When set to |NO| various UI updates are
// ignored.
@property(nonatomic, assign) BOOL isShowing;

// Return the toolbar view;
- (UIView*)toolBarView;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_H_
