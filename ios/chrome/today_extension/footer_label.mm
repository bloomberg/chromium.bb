// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/today_extension/footer_label.h"

#import "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/common/string_util.h"
#include "ios/chrome/today_extension/interactive_label.h"
#include "ios/chrome/today_extension/transparent_button.h"
#include "ios/chrome/today_extension/ui_util.h"
#include "ios/today_extension/grit/ios_today_extension_strings.h"
#include "ui/base/l10n/l10n_util.h"

// Font size of a footer.
const CGFloat kFooterFontSize = 11;

@implementation SimpleFooterLabel {
  base::scoped_nsobject<InteractiveLabel> _label;
}

- (instancetype)initWithLabelString:(NSString*)labelString
                       buttonString:(NSString*)buttonString
                          linkBlock:(ProceduralBlock)linkBlock
                        buttonBlock:(ProceduralBlock)buttonBlock {
  self = [super init];
  UIEdgeInsets insets = UIEdgeInsetsMake(
      ui_util::kFooterVerticalMargin, ui_util::kFooterHorizontalMargin,
      ui_util::kFooterVerticalMargin, ui_util::kFooterHorizontalMargin);
  _label.reset([[InteractiveLabel alloc] initWithFrame:CGRectZero
                                           labelString:labelString
                                              fontSize:kFooterFontSize
                                        labelAlignment:NSTextAlignmentCenter
                                                insets:insets
                                          buttonString:buttonString
                                             linkBlock:linkBlock
                                           buttonBlock:buttonBlock]);

  return self;
}

- (CGFloat)heightForWidth:(CGFloat)width {
  CGSize size = [_label sizeThatFits:CGSizeMake(width, CGFLOAT_MAX)];
  return size.height;
}

- (void)setFrame:(CGRect)frame {
  [_label setFrame:frame];
}

- (UIView*)view {
  return _label;
}

@end

@implementation PWIsOnFooterLabel

- (instancetype)initWithLearnMoreBlock:(LearnMoreBlock)learnMore
                          turnOffBlock:(EnableDisableBlock)turnOffBlock {
  self = [super
      initWithLabelString:
          l10n_util::GetNSString(
              IDS_IOS_PYSICAL_WEB_ON_TODAY_EXTENSION_FOOTER_LABEL)
             buttonString:
                 l10n_util::GetNSString(
                     IDS_IOS_PYSICAL_WEB_TURN_OFF_TODAY_EXTENSION_FOOTER_LABEL)
                linkBlock:learnMore
              buttonBlock:turnOffBlock];
  return self;
}

@end

@implementation PWIsOffFooterLabel

- (instancetype)initWithLearnMoreBlock:(LearnMoreBlock)learnMore
                           turnOnBlock:(EnableDisableBlock)turnOnBlock {
  self = [super
      initWithLabelString:
          l10n_util::GetNSString(
              IDS_IOS_PYSICAL_WEB_OFF_TODAY_EXTENSION_FOOTER_LABEL)
             buttonString:
                 l10n_util::GetNSString(
                     IDS_IOS_PYSICAL_WEB_TURN_ON_TODAY_EXTENSION_FOOTER_LABEL)
                linkBlock:learnMore
              buttonBlock:turnOnBlock];
  return self;
}

@end

@implementation PWScanningFooterLabel

- (instancetype)initWithLearnMoreBlock:(LearnMoreBlock)learnMore
                          turnOffBlock:(EnableDisableBlock)turnOffBlock {
  self = [super
      initWithLabelString:
          l10n_util::GetNSString(
              IDS_IOS_PYSICAL_WEB_SCANNING_TODAY_EXTENSION_FOOTER_LABEL)
             buttonString:
                 l10n_util::GetNSString(
                     IDS_IOS_PYSICAL_WEB_TURN_OFF_TODAY_EXTENSION_FOOTER_LABEL)
                linkBlock:learnMore
              buttonBlock:turnOffBlock];
  return self;
}

@end

@implementation PWBTOffFooterLabel

- (instancetype)initWithLearnMoreBlock:(LearnMoreBlock)learnMore {
  self =
      [super initWithLabelString:
                 l10n_util::GetNSString(
                     IDS_IOS_PYSICAL_WEB_BLUETOOTH_TODAY_EXTENSION_FOOTER_LABEL)
                    buttonString:nil
                       linkBlock:learnMore
                     buttonBlock:nil];
  return self;
}

@end
