// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_CRUILABEL_ATTRIBUTEUTILS_H_
#define IOS_CHROME_BROWSER_UI_UTIL_CRUILABEL_ATTRIBUTEUTILS_H_

#import <UIKit/UIKit.h>

@interface UILabel (CRUILabelAttributeUtils)
@property(nonatomic, assign, setter=cr_setLineHeight:) CGFloat cr_lineHeight;

// Adjusts the line height of the receiver so that the lines will evenly f
- (void)cr_adjustLineHeightForMaximimumLines:(NSUInteger)maximumLines;

@end

#endif  // IOS_CHROME_BROWSER_UI_UTIL_CRUILABEL_ATTRIBUTEUTILS_H_
