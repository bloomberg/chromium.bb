// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/omnibox/autocomplete_result_consumer.h"
#import "ios/chrome/browser/ui/omnibox/image_retriever.h"

@protocol ImageRetriever;

// View controller used to display a list of omnibox autocomplete matches in the
// omnibox popup.
@interface OmniboxPopupViewController
    : UITableViewController<AutocompleteResultConsumer>

@property(nonatomic, assign) BOOL incognito;
@property(nonatomic, weak) id<AutocompleteResultConsumerDelegate> delegate;
@property(nonatomic, weak) id<ImageRetriever> imageRetriever;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// Set text alignment for popup cells.
- (void)setTextAlignment:(NSTextAlignment)alignment;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_CONTROLLER_H_
