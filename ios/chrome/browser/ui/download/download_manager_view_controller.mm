// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Layout Guide name for action button UILayoutGuide.
GuideName* const kActionButtonGuide = @"kDownloadManagerActionButtonGuide";

// A margin for every control element (status label, action button, close
// button). Defines the minimal distance between elements.
const CGFloat kElementMargin = 16;

// Returns formatted size string.
NSString* GetSizeString(long long size_in_bytes) {
  return [NSByteCountFormatter
      stringFromByteCount:size_in_bytes
               countStyle:NSByteCountFormatterCountStyleFile];
}
}  // namespace

@interface DownloadManagerViewController () {
  UIButton* _closeButton;
  UILabel* _statusLabel;
  UIButton* _actionButton;
}
// Shadow view sits on top of background view. Union of shadow and background
// views fills self.view area. The shadow is dropped to web page, not to white
// Download Manager background.
@property(nonatomic, readonly) UIImageView* shadow;
@property(nonatomic, readonly) UIView* background;
@end

@implementation DownloadManagerViewController

@synthesize delegate = _delegate;
@synthesize fileName = _fileName;
@synthesize countOfBytesReceived = _countOfBytesReceived;
@synthesize countOfBytesExpectedToReceive = _countOfBytesExpectedToReceive;
@synthesize state = _state;
@synthesize shadow = _shadow;
@synthesize background = _background;

#pragma mark - UIViewController overrides

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.view addSubview:self.shadow];
  [self.view addSubview:self.background];
  [self.view addSubview:self.closeButton];
  [self.view addSubview:self.statusLabel];
  [self.view addSubview:self.actionButton];

  AddNamedGuide(kActionButtonGuide, self.view);
  ConstrainNamedGuideToView(kActionButtonGuide, self.actionButton);

  if (@available(iOS 11, *)) {
    self.view.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
        kElementMargin, kElementMargin, kElementMargin, kElementMargin);
  } else {
    self.view.layoutMargins = UIEdgeInsetsMake(kElementMargin, kElementMargin,
                                               kElementMargin, kElementMargin);
  }
}

- (void)updateViewConstraints {
  [super updateViewConstraints];

  // self.view constraints.
  UIView* view = self.view;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  UILabel* statusLabel = self.statusLabel;
  [NSLayoutConstraint activateConstraints:@[
    [view.layoutMarginsGuide.heightAnchor
        constraintEqualToAnchor:statusLabel.heightAnchor],
  ]];

  // shadow constraints.
  UIImageView* shadow = self.shadow;
  [NSLayoutConstraint activateConstraints:@[
    [shadow.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [shadow.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
    [shadow.topAnchor constraintEqualToAnchor:view.topAnchor],
  ]];

  // background constraints.
  UIView* background = self.background;
  [NSLayoutConstraint activateConstraints:@[
    [background.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [background.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
    [background.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [background.topAnchor constraintEqualToAnchor:shadow.bottomAnchor],
  ]];

  // close button constraints.
  UIButton* closeButton = self.closeButton;
  [NSLayoutConstraint activateConstraints:@[
    [closeButton.centerYAnchor constraintEqualToAnchor:view.centerYAnchor],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:view.layoutMarginsGuide.trailingAnchor],
  ]];

  // status label constraints.
  UIButton* actionButton = self.actionButton;
  [NSLayoutConstraint activateConstraints:@[
    [statusLabel.centerYAnchor constraintEqualToAnchor:view.centerYAnchor],
    [statusLabel.leadingAnchor
        constraintEqualToAnchor:view.layoutMarginsGuide.leadingAnchor],
    [statusLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:actionButton.leadingAnchor
                                 constant:-kElementMargin],
  ]];

  // action button constraints.
  [NSLayoutConstraint activateConstraints:@[
    [actionButton.centerYAnchor constraintEqualToAnchor:view.centerYAnchor],
    [actionButton.trailingAnchor
        constraintEqualToAnchor:closeButton.leadingAnchor
                       constant:-kElementMargin],
  ]];
}

#pragma mark - Public Properties

- (void)setFileName:(NSString*)fileName {
  if (![_fileName isEqual:fileName]) {
    _fileName = [fileName copy];
    [self updateStatusLabel];
  }
}

- (void)setCountOfBytesReceived:(int64_t)value {
  if (_countOfBytesReceived != value) {
    _countOfBytesReceived = value;
    [self updateStatusLabel];
  }
}

- (void)setCountOfBytesExpectedToReceive:(int64_t)value {
  if (_countOfBytesExpectedToReceive != value) {
    _countOfBytesExpectedToReceive = value;
    [self updateStatusLabel];
  }
}

- (void)setState:(DownloadManagerState)state {
  if (_state != state) {
    _state = state;
    [self updateStatusLabel];
    [self updateActionButton];
  }
}

#pragma mark - UI elements

- (UIImageView*)shadow {
  if (!_shadow) {
    UIImage* shadowImage = [UIImage imageNamed:@"infobar_shadow"];
    _shadow = [[UIImageView alloc] initWithImage:shadowImage];
    _shadow.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _shadow;
}

- (UIView*)background {
  if (!_background) {
    _background = [[UIView alloc] initWithFrame:CGRectZero];
    _background.translatesAutoresizingMaskIntoConstraints = NO;
    _background.backgroundColor = [UIColor whiteColor];
  }
  return _background;
}

- (UIButton*)closeButton {
  if (!_closeButton) {
    _closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    _closeButton.exclusiveTouch = YES;
    _closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_CLOSE);

    // TODO(crbug.com/228611): Add IDR_ constant and use GetNativeImageNamed().
    UIImage* image = [UIImage imageNamed:@"infobar_close"];
    [_closeButton setImage:image forState:UIControlStateNormal];

    [_closeButton addTarget:self
                     action:@selector(didTapCloseButton)
           forControlEvents:UIControlEventTouchUpInside];
  }
  return _closeButton;
}

- (UILabel*)statusLabel {
  if (!_statusLabel) {
    _statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _statusLabel.font = [MDCTypography subheadFont];
    [self updateStatusLabel];
  }
  return _statusLabel;
}

- (UIButton*)actionButton {
  if (!_actionButton) {
    _actionButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _actionButton.translatesAutoresizingMaskIntoConstraints = NO;
    _actionButton.exclusiveTouch = YES;
    [_actionButton setTitleColor:[MDCPalette bluePalette].tint500
                        forState:UIControlStateNormal];

    [_actionButton addTarget:self
                      action:@selector(didTapActionButton)
            forControlEvents:UIControlEventTouchUpInside];
    [self updateActionButton];
  }
  return _actionButton;
}

#pragma mark - Actions

- (void)didTapCloseButton {
  SEL selector = @selector(downloadManagerViewControllerDidClose:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate downloadManagerViewControllerDidClose:self];
  }
}

- (void)didTapActionButton {
  switch (self.state) {
    case kDownloadManagerStateNotStarted: {
      SEL selector = @selector(downloadManagerViewControllerDidStartDownload:);
      if ([_delegate respondsToSelector:selector]) {
        [_delegate downloadManagerViewControllerDidStartDownload:self];
      }
      break;
    }
    case kDownloadManagerStateInProgress: {
      // The button should not be visible.
      NOTREACHED();
      break;
    }
    case kDownloadManagerStateSuceeded: {
      SEL selector = @selector
          (downloadManagerViewController:presentOpenInMenuWithLayoutGuide:);
      if ([_delegate respondsToSelector:selector]) {
        UILayoutGuide* guide = FindNamedGuide(kActionButtonGuide, self.view);
        [_delegate downloadManagerViewController:self
                presentOpenInMenuWithLayoutGuide:guide];
      }
      break;
    }
    case kDownloadManagerStateFailed: {
      SEL selector = @selector(downloadManagerViewControllerDidStartDownload:);
      if ([_delegate respondsToSelector:selector]) {
        [_delegate downloadManagerViewControllerDidStartDownload:self];
      }
      break;
    }
  }
}

#pragma mark - UI Updates

// Updates status label text depending on |state|.
- (void)updateStatusLabel {
  NSString* statusText = nil;
  switch (self.state) {
    case kDownloadManagerStateNotStarted:
      statusText = self.fileName;
      if (self.countOfBytesExpectedToReceive != -1) {
        statusText = [statusText
            stringByAppendingFormat:@" - %@",
                                    GetSizeString(
                                        self.countOfBytesExpectedToReceive)];
      }
      break;
    case kDownloadManagerStateInProgress:
      // TODO(crbug.com/805533): Localize this string.
      statusText =
          [NSString stringWithFormat:@"Downloading... %@",
                                     GetSizeString(self.countOfBytesReceived)];
      if (self.countOfBytesExpectedToReceive != -1) {
        statusText = [statusText
            stringByAppendingFormat:@"/%@",
                                    GetSizeString(
                                        self.countOfBytesExpectedToReceive)];
      }
      break;
    case kDownloadManagerStateSuceeded:
      statusText = self.fileName;
      break;
    case kDownloadManagerStateFailed:
      // TODO(crbug.com/805533): Localize this string.
      statusText = @"Couldn't Download";
      break;
  }

  self.statusLabel.text = statusText;
  [self.statusLabel sizeToFit];
}

// Updates title and hidden state for action button depending on |state|.
- (void)updateActionButton {
  NSString* title = nil;
  switch (self.state) {
    case kDownloadManagerStateNotStarted:
      // TODO(crbug.com/805533): Localize this string.
      title = @"Download";
      break;
    case kDownloadManagerStateInProgress:
      break;
    case kDownloadManagerStateSuceeded:
      // TODO(crbug.com/805533): Localize this string.
      title = @"Open In...";
      break;
    case kDownloadManagerStateFailed:
      // TODO(crbug.com/805533): Localize this string.
      title = @"Try Again";
      break;
  }

  [self.actionButton setTitle:title forState:UIControlStateNormal];
  self.actionButton.hidden = self.state == kDownloadManagerStateInProgress;
}

@end
