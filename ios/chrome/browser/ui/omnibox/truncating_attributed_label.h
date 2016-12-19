// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_TRUNCATING_ATTRIBUTED_LABEL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_TRUNCATING_ATTRIBUTED_LABEL_H_

#import <CoreText/CoreText.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

#include "base/mac/scoped_nsobject.h"

typedef enum {
  OmniboxPopupTruncatingTail = 0x1,
  OmniboxPopupTruncatingHead = 0x2,
  OmniboxPopupTruncatingHeadAndTail =
      OmniboxPopupTruncatingHead | OmniboxPopupTruncatingTail
} OmniboxPopupTruncatingMode;

// This is a copy of GTMFadeTruncatingLabel that supports
// NSMutableAttributedString to change font (face and size), and color by using
// CATextLayer.  Unlike GTMFadeTruncatingLabel, it's not based on a UILabel, so
// it does not support shadowOffset and shadowColor, baselineAdjustment,
// highlighted, minimumFontSize, and numberOfLines.
@interface OmniboxPopupTruncatingLabel : UIView

// Which side(s) to truncate.
@property(nonatomic, assign) OmniboxPopupTruncatingMode truncateMode;

// Text alignment.
@property(nonatomic, assign) NSTextAlignment textAlignment;

// For display of an attributed string.
@property(nonatomic, retain) NSMutableAttributedString* attributedText;

// Set the label highlight state.
@property(nonatomic, assign) BOOL highlighted;

// Set the label highlight color.
@property(nonatomic, retain) NSMutableAttributedString* highlightedText;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_TRUNCATING_ATTRIBUTED_LABEL_H_
