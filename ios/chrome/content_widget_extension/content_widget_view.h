// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CONTENT_WIDGET_EXTENSION_CONTENT_WIDGET_VIEW_H_
#define IOS_CHROME_CONTENT_WIDGET_EXTENSION_CONTENT_WIDGET_VIEW_H_

#import <UIKit/UIKit.h>

// View for the content widget. Shows 1 (compact view) or 2 (full size view)
// rows of 4 most visited tiles (favicon or fallback + title), if there are
// enough tiles to show. If there are fewer than 4 tiles, always displays a
// single row.
@interface ContentWidgetView : UIView

// The height of the widget in expanded mode.
@property(nonatomic, readonly) CGFloat widgetExpandedHeight;

// Designated initializer, creates the widget view. |compactHeight| indicates
// the size to use in compact display. |initiallyCompact| indicates which mode
// to display on initialization.
- (instancetype)initWithCompactHeight:(CGFloat)compactHeight
                     initiallyCompact:(BOOL)compact NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Updates the view to display a compact or expanded view, depending on
// |compact|. If |compact| is false, the view shows a maximum of 8 tiles. If
// |compact| is true, the view is set to show a single row of 4 tiles at most
// within the |compactHeight| passed in the constructor.
- (void)showMode:(BOOL)compact;

@end

#endif  // IOS_CHROME_CONTENT_WIDGET_EXTENSION_CONTENT_WIDGET_VIEW_H_
