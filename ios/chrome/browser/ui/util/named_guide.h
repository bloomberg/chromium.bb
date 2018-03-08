// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_NAMED_GUIDE_H_
#define IOS_CHROME_BROWSER_UI_UTIL_NAMED_GUIDE_H_

#import "ios/chrome/browser/ui/util/layout_guide_names.h"

// A UILayoutGuide subclass that represents the layout of a well-known piece of
// UI.  See layout_guide_names.h for a list of the UI components for which named
// guides are created.
@interface NamedGuide : UILayoutGuide

// Designated initializer for a guide with |name|.
- (instancetype)initWithName:(GuideName*)name NS_DESIGNATED_INITIALIZER;

// NamedGuides must be created using |-initWithName:|.
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Returns the NamedGuide with the given |name|, if one is attached to |view|
// or one of |view|'s ancestors.  If no guide is found, returns nil.
+ (instancetype)guideWithName:(GuideName*)name view:(UIView*)view;

// The GuideName passed on initialization.
@property(nonatomic, readonly) GuideName* name;

// The view to which this guide should be constrained.  Setting this property
// to a new value will update the guide's constraints to match the new view.
// Setting to nil removes constraints.
@property(nonatomic, weak) UIView* constrainedView;

@end

#endif  // IOS_CHROME_BROWSER_UI_UTIL_NAMED_GUIDE_H_
