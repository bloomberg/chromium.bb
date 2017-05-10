// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_SEARCH_WIDGET_EXTENSION_SEARCH_WIDGET_VIEW_H_
#define IOS_CHROME_SEARCH_WIDGET_EXTENSION_SEARCH_WIDGET_VIEW_H_

#import <UIKit/UIKit.h>

// Protocol to be implemented by targets for user actions coming from the search
// widget view.
@protocol SearchWidgetViewActionTarget

// Called when the user taps the Search button.
- (void)openSearch:(id)sender;
// Called when the user taps the Incognito Search button.
- (void)openIncognito:(id)sender;
// Called when the user taps the Voice Search button.
- (void)openVoice:(id)sender;
// Called when the user taps the QR Code button.
- (void)openQRCode:(id)sender;
// Called when the user taps the Open Copied URL section.
- (void)openCopiedURL:(id)sender;

@end

// View for the search widget. Shows two sections. The first section is a row of
// actions to launch the app represented as a circular icon and a label. When
// tapped, they call the callbacks in the SearchWidgetViewActionTarget protocol.
// The second section shows a button with the |copiedURLString| if it has been
// set. When tapped, the openCopiedURL callback in SearchWidgetViewActionTarget
// is called. If no |copiedURLString| was set, the button is replaced by a
// hairline separation and placeholder text. This section can be hidden with the
// |copiedURLVisible| variable.
@interface SearchWidgetView : UIView

// Set this property to show/hide the copied URL section.
@property(nonatomic, assign) BOOL copiedURLVisible;

// The copied URL string to be shown in the copied URL section.
@property(nonatomic, copy) NSString* copiedURLString;

// Designated initializer, creates the widget view with a |target| for user
// actions. The |primaryVibrancyEffect| and |secondaryVibrancyEffect| effects
// will be used to display view elements.
- (instancetype)initWithActionTarget:(id<SearchWidgetViewActionTarget>)target
               primaryVibrancyEffect:(UIVibrancyEffect*)primaryVibrancyEffect
             secondaryVibrancyEffect:(UIVibrancyEffect*)secondaryVibrancyEffect
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_SEARCH_WIDGET_EXTENSION_SEARCH_WIDGET_VIEW_H_
