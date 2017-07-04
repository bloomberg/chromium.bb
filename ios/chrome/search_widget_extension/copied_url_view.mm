// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/copied_url_view.h"

#include "base/ios/ios_util.h"
#import "ios/chrome/search_widget_extension/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kURLButtonMargin = 10;

}  // namespace

@interface CopiedURLView ()

// The copied URL label containing the URL or a placeholder text.
@property(nonatomic, strong) UILabel* copiedURLLabel;
// The copied URL title label containing the title of the copied URL button.
@property(nonatomic, strong) UILabel* openCopiedURLTitleLabel;
// The hairline view potentially shown at the top of the copied URL view.
@property(nonatomic, strong) UIView* hairlineView;
// The button shown when there is a copied URL to open.
@property(nonatomic, strong) UIButton* copiedButtonView;

// Updates the view to show the copied URL in a button.
- (void)updateUICopiedURL;

// Updates the view to show that there is no copied URL, with no button and a
// hairline at the top of the view.
- (void)updateUINoCopiedURL;

@end

@implementation CopiedURLView

- (void)setCopiedURLString:(NSString*)copiedURL {
  _copiedURLString = copiedURL;
  if (copiedURL) {
    [self updateUICopiedURL];
  } else {
    [self updateUINoCopiedURL];
  }
}

@synthesize copiedURLLabel = _copiedURLLabel;
@synthesize copiedURLString = _copiedURLString;
@synthesize openCopiedURLTitleLabel = _openCopiedURLTitleLabel;
@synthesize hairlineView = _hairlineView;
@synthesize copiedButtonView = _copiedButtonView;

- (instancetype)initWithActionTarget:(id)target
                      actionSelector:(SEL)actionSelector
                       primaryEffect:(UIVisualEffect*)primaryEffect
                     secondaryEffect:(UIVisualEffect*)secondaryEffect {
  DCHECK(target);
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;

    UIVisualEffectView* primaryEffectView =
        [[UIVisualEffectView alloc] initWithEffect:primaryEffect];
    UIVisualEffectView* secondaryEffectView =
        [[UIVisualEffectView alloc] initWithEffect:secondaryEffect];
    for (UIVisualEffectView* effectView in
         @[ primaryEffectView, secondaryEffectView ]) {
      [self addSubview:effectView];
      effectView.translatesAutoresizingMaskIntoConstraints = NO;
      [NSLayoutConstraint
          activateConstraints:ui_util::CreateSameConstraints(self, effectView)];
    }

    _hairlineView = [[UIView alloc] initWithFrame:CGRectZero];
    _hairlineView.backgroundColor = base::ios::IsRunningOnIOS10OrLater()
                                        ? [UIColor colorWithWhite:0 alpha:0.05]
                                        : [UIColor whiteColor];
    _hairlineView.translatesAutoresizingMaskIntoConstraints = NO;
    [secondaryEffectView.contentView addSubview:_hairlineView];

    _copiedButtonView = [[UIButton alloc] initWithFrame:CGRectZero];
    _copiedButtonView.backgroundColor = [UIColor colorWithWhite:0 alpha:0.05];
    _copiedButtonView.layer.cornerRadius = 5;
    _copiedButtonView.translatesAutoresizingMaskIntoConstraints = NO;
    _copiedButtonView.accessibilityLabel =
        NSLocalizedString(@"IDS_IOS_OPEN_COPIED_LINK", nil);
    [secondaryEffectView.contentView addSubview:_copiedButtonView];
    [_copiedButtonView addTarget:target
                          action:actionSelector
                forControlEvents:UIControlEventTouchUpInside];

    _openCopiedURLTitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _openCopiedURLTitleLabel.textAlignment = NSTextAlignmentCenter;
    _openCopiedURLTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _openCopiedURLTitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    [primaryEffectView.contentView addSubview:_openCopiedURLTitleLabel];

    _copiedURLLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _copiedURLLabel.textAlignment = NSTextAlignmentCenter;
    _copiedURLLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _copiedURLLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [secondaryEffectView.contentView addSubview:_copiedURLLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_hairlineView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_hairlineView.leftAnchor constraintEqualToAnchor:self.leftAnchor],
      [_hairlineView.rightAnchor constraintEqualToAnchor:self.rightAnchor],
      [_hairlineView.heightAnchor constraintEqualToConstant:0.5],

      [_copiedButtonView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:ui_util::kContentMargin],
      [_copiedButtonView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-ui_util::kContentMargin],
      [_copiedButtonView.topAnchor
          constraintEqualToAnchor:_hairlineView.bottomAnchor
                         constant:ui_util::kContentMargin],
      [_copiedButtonView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor
                         constant:-ui_util::kContentMargin],

      [_openCopiedURLTitleLabel.topAnchor
          constraintEqualToAnchor:_copiedButtonView.topAnchor
                         constant:kURLButtonMargin],
      [_openCopiedURLTitleLabel.leadingAnchor
          constraintEqualToAnchor:_copiedButtonView.leadingAnchor
                         constant:ui_util::kContentMargin],
      [_openCopiedURLTitleLabel.trailingAnchor
          constraintEqualToAnchor:_copiedButtonView.trailingAnchor
                         constant:-ui_util::kContentMargin],

      [_copiedURLLabel.topAnchor
          constraintEqualToAnchor:_openCopiedURLTitleLabel.bottomAnchor],
      [_copiedURLLabel.bottomAnchor
          constraintEqualToAnchor:_copiedButtonView.bottomAnchor
                         constant:-kURLButtonMargin],
      [_copiedURLLabel.leadingAnchor
          constraintEqualToAnchor:_openCopiedURLTitleLabel.leadingAnchor],
      [_copiedURLLabel.trailingAnchor
          constraintEqualToAnchor:_openCopiedURLTitleLabel.trailingAnchor],
    ]];
    [self updateUINoCopiedURL];
  }
  return self;
}

- (void)updateUICopiedURL {
  self.copiedButtonView.hidden = NO;
  self.hairlineView.hidden = YES;
  self.copiedURLLabel.text = self.copiedURLString;
  self.copiedURLLabel.accessibilityLabel = self.copiedURLString;
  self.openCopiedURLTitleLabel.alpha = 1;
  self.openCopiedURLTitleLabel.text =
      NSLocalizedString(@"IDS_IOS_OPEN_COPIED_LINK", nil);
  self.openCopiedURLTitleLabel.isAccessibilityElement = NO;
  self.copiedURLLabel.alpha = 1;
}

- (void)updateUINoCopiedURL {
  self.copiedButtonView.hidden = YES;
  self.hairlineView.hidden = NO;
  self.copiedURLLabel.text =
      NSLocalizedString(@"IDS_IOS_NO_COPIED_LINK_MESSAGE", nil);
  self.copiedURLLabel.accessibilityLabel =
      NSLocalizedString(@"IDS_IOS_NO_COPIED_LINK_MESSAGE", nil);
  self.openCopiedURLTitleLabel.text =
      NSLocalizedString(@"IDS_IOS_NO_COPIED_LINK_TITLE", nil);
  self.openCopiedURLTitleLabel.accessibilityLabel =
      NSLocalizedString(@"IDS_IOS_NO_COPIED_LINK_TITLE", nil);
  self.openCopiedURLTitleLabel.isAccessibilityElement = YES;

  if (base::ios::IsRunningOnIOS10OrLater()) {
    self.copiedURLLabel.alpha = 0.5;
    self.openCopiedURLTitleLabel.alpha = 0.5;
  }
}

@end
