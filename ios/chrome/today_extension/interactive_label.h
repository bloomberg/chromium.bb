// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TODAY_EXTENSION_INTERACTIVE_LABEL_H_
#define IOS_CHROME_TODAY_EXTENSION_INTERACTIVE_LABEL_H_

#import <UIKit/UIKit.h>

#import <base/ios/block_types.h>

// Label view that can contain a link (delimited by BEGIN_LINK and END_LINK in
// |labelString|) and a button (if |buttonString| and |buttonBlock| are both non
// nil).
@interface InteractiveLabel : UIView

- (instancetype)initWithFrame:(CGRect)frame
                  labelString:(NSString*)labelString
                     fontSize:(CGFloat)fontSize
               labelAlignment:(NSTextAlignment)labelAlignment
                       insets:(UIEdgeInsets)insets
                 buttonString:(NSString*)buttonString
                    linkBlock:(ProceduralBlock)linkBlock
                  buttonBlock:(ProceduralBlock)buttonBlock;

@end

#endif  // IOS_CHROME_TODAY_EXTENSION_INTERACTIVE_LABEL_H_
