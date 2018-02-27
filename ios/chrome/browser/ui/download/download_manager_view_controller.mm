// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
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
  UIButton* _installDriveButton;

  NSString* _fileName;
  int64_t _countOfBytesReceived;
  int64_t _countOfBytesExpectedToReceive;
  DownloadManagerState _state;
  BOOL _installDriveButtonVisible;
}
// Shadow view sits on top of background view. Union of shadow and background
// views fills self.view area. The shadow is dropped to web page, not to white
// Download Manager background.
@property(nonatomic, readonly) UIImageView* shadow;
@property(nonatomic, readonly) UIView* background;

// Download Manager UI has 2 rows. First row is always visible and contains
// essential download controls: close button, action button and status label.
// Second row is hidden by default and constains Install Google Drive button.
// The second row is visible if |_installDriveButtonVisible| is set to YES.
// Each row is a UIView with controls as subviews, which allows to:
//   - vertically align all controls in the row
//   - hide all controls in a row altogether
//   - set proper constraits to size self.view
@property(nonatomic, readonly) UIView* downloadControlsRow;
@property(nonatomic, readonly) UIView* installDriveControlsRow;

// Represents constraint for self.view.bottomAnchor, which can either be
// constrained to installDriveControlsRow's bottomAnchor or to
// downloadControlsRow's bottomAnchor. Stored in a property to allow
// deactivating the old constraint.
@property(nonatomic) NSLayoutConstraint* bottomConstraint;

@end

@implementation DownloadManagerViewController

@synthesize delegate = _delegate;
@synthesize shadow = _shadow;
@synthesize background = _background;
@synthesize downloadControlsRow = _downloadControlsRow;
@synthesize installDriveControlsRow = _installDriveControlsRow;
@synthesize bottomConstraint = _bottomConstraint;

#pragma mark - UIViewController overrides

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.view addSubview:self.shadow];
  [self.view addSubview:self.background];
  [self.view addSubview:self.downloadControlsRow];
  [self.view addSubview:self.installDriveControlsRow];
  [self.downloadControlsRow addSubview:self.closeButton];
  [self.downloadControlsRow addSubview:self.statusLabel];
  [self.downloadControlsRow addSubview:self.actionButton];
  [self.installDriveControlsRow addSubview:self.installDriveButton];

  AddNamedGuide(kActionButtonGuide, self.view);
  ConstrainNamedGuideToView(kActionButtonGuide, self.actionButton);
}

- (void)updateViewConstraints {
  [super updateViewConstraints];

  // self.view constraints.
  UIView* view = self.view;
  [self updateBottomConstraints];

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

  // download controls row constraints.
  UIView* downloadRow = self.downloadControlsRow;
  UIButton* actionButton = self.actionButton;
  // Account for bottom white pixel on shadow image.
  CGFloat shadowHeight = CGRectGetHeight(shadow.frame) - 1;
  [NSLayoutConstraint activateConstraints:@[
    [downloadRow.leadingAnchor
        constraintEqualToAnchor:view.layoutMarginsGuide.leadingAnchor],
    [downloadRow.trailingAnchor
        constraintEqualToAnchor:view.layoutMarginsGuide.trailingAnchor],
    [downloadRow.topAnchor
        constraintEqualToAnchor:view.layoutMarginsGuide.topAnchor
                       constant:shadowHeight],
    [downloadRow.heightAnchor
        constraintEqualToAnchor:actionButton.heightAnchor],
  ]];

  // install drive controls row constraints.
  UIView* installDriveRow = self.installDriveControlsRow;
  UIButton* installDriveButton = self.installDriveButton;
  [NSLayoutConstraint activateConstraints:@[
    [installDriveRow.leadingAnchor
        constraintEqualToAnchor:view.layoutMarginsGuide.leadingAnchor],
    [installDriveRow.trailingAnchor
        constraintEqualToAnchor:view.layoutMarginsGuide.trailingAnchor],
    [installDriveRow.topAnchor
        constraintEqualToAnchor:downloadRow.bottomAnchor],
    [installDriveRow.heightAnchor
        constraintEqualToAnchor:installDriveButton.heightAnchor],
  ]];

  // close button constraints.
  UIButton* closeButton = self.closeButton;
  [NSLayoutConstraint activateConstraints:@[
    [closeButton.centerYAnchor
        constraintEqualToAnchor:downloadRow.centerYAnchor],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:downloadRow.trailingAnchor],
  ]];

  // status label constraints.
  UILabel* statusLabel = self.statusLabel;
  [NSLayoutConstraint activateConstraints:@[
    [statusLabel.centerYAnchor
        constraintEqualToAnchor:downloadRow.centerYAnchor],
    [statusLabel.leadingAnchor
        constraintEqualToAnchor:downloadRow.leadingAnchor],
    [statusLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:actionButton.leadingAnchor
                                 constant:-kElementMargin],
  ]];

  // action button constraints.
  [NSLayoutConstraint activateConstraints:@[
    [actionButton.centerYAnchor
        constraintEqualToAnchor:downloadRow.centerYAnchor],
    [actionButton.trailingAnchor
        constraintEqualToAnchor:closeButton.leadingAnchor
                       constant:-kElementMargin],
  ]];

  // install google drive button constraints.
  [NSLayoutConstraint activateConstraints:@[
    [installDriveButton.centerYAnchor
        constraintEqualToAnchor:installDriveRow.centerYAnchor],
    [installDriveButton.trailingAnchor
        constraintEqualToAnchor:closeButton.leadingAnchor
                       constant:-kElementMargin],
  ]];
}

#pragma mark - Public

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

- (void)setInstallDriveButtonVisible:(BOOL)visible animated:(BOOL)animated {
  if (visible == _installDriveButtonVisible)
    return;

  _installDriveButtonVisible = visible;
  __weak DownloadManagerViewController* weakSelf = self;
  [UIView animateWithDuration:animated ? 0.2 : 0.0
                   animations:^{
                     DownloadManagerViewController* strongSelf = weakSelf;
                     [strongSelf updateInstallDriveControlsRow];
                     [strongSelf updateBottomConstraints];
                     [strongSelf.view.superview layoutIfNeeded];
                   }];
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

- (UIView*)downloadControlsRow {
  if (!_downloadControlsRow) {
    _downloadControlsRow = [[UIView alloc] initWithFrame:CGRectZero];
    _downloadControlsRow.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _downloadControlsRow;
}

- (UIView*)installDriveControlsRow {
  if (!_installDriveControlsRow) {
    _installDriveControlsRow = [[UIView alloc] initWithFrame:CGRectZero];
    _installDriveControlsRow.translatesAutoresizingMaskIntoConstraints = NO;
    [self updateInstallDriveControlsRow];
  }
  return _installDriveControlsRow;
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

- (UIButton*)installDriveButton {
  if (!_installDriveButton) {
    _installDriveButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _installDriveButton.translatesAutoresizingMaskIntoConstraints = NO;
    _installDriveButton.exclusiveTouch = YES;
    [_installDriveButton setTitleColor:[MDCPalette bluePalette].tint500
                              forState:UIControlStateNormal];

    [_installDriveButton addTarget:self
                            action:@selector(didTapInstallDriveButton)
                  forControlEvents:UIControlEventTouchUpInside];
    [_installDriveButton
        setTitle:l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_INSTALL)
        forState:UIControlStateNormal];
  }
  return _installDriveButton;
}

#pragma mark - Actions

- (void)didTapCloseButton {
  SEL selector = @selector(downloadManagerViewControllerDidClose:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate downloadManagerViewControllerDidClose:self];
  }
}

- (void)didTapActionButton {
  switch (_state) {
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

- (void)didTapInstallDriveButton {
  SEL selector = @selector(installDriveForDownloadManagerViewController:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate installDriveForDownloadManagerViewController:self];
  }
}

#pragma mark - UI Updates

// Updates and activates self.bottomConstraint. self.bottomConstraint
// constraints self.view.bottomAnchor to installDriveControlsRow's bottom
// if |_installDriveButtonVisible| is set to YES, otherwise
// self.view.bottomAnchor is constrained to downloadControlsRow's botttom. This
// resizes self.view to show or hide installDriveControlsRow view.
- (void)updateBottomConstraints {
  self.bottomConstraint.active = NO;

  NSLayoutYAxisAnchor* firstAnchor = self.view.layoutMarginsGuide.bottomAnchor;
  NSLayoutYAxisAnchor* secondAnchor =
      _installDriveButtonVisible ? self.installDriveControlsRow.bottomAnchor
                                 : self.downloadControlsRow.bottomAnchor;

  self.bottomConstraint = [firstAnchor constraintEqualToAnchor:secondAnchor];

  self.bottomConstraint.active = YES;
}

// Updates status label text depending on |state|.
- (void)updateStatusLabel {
  NSString* statusText = nil;
  switch (_state) {
    case kDownloadManagerStateNotStarted:
      statusText = _fileName;
      if (_countOfBytesExpectedToReceive != -1) {
        statusText = [statusText
            stringByAppendingFormat:@" - %@",
                                    GetSizeString(
                                        _countOfBytesExpectedToReceive)];
      }
      break;
    case kDownloadManagerStateInProgress: {
      base::string16 size =
          base::SysNSStringToUTF16(GetSizeString(_countOfBytesReceived));
      statusText = l10n_util::GetNSStringF(
          IDS_IOS_DOWNLOAD_MANAGER_DOWNLOADING_ELIPSIS, size);
      if (_countOfBytesExpectedToReceive != -1) {
        statusText = [statusText
            stringByAppendingFormat:@"/%@",
                                    GetSizeString(
                                        _countOfBytesExpectedToReceive)];
      }
      break;
    }
    case kDownloadManagerStateSuceeded:
      statusText = _fileName;
      break;
    case kDownloadManagerStateFailed:
      statusText =
          l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_COULDNT_DOWNLOAD);
      break;
  }

  self.statusLabel.text = statusText;
  [self.statusLabel sizeToFit];
}

// Updates title and hidden state for action button depending on |state|.
- (void)updateActionButton {
  NSString* title = nil;
  switch (_state) {
    case kDownloadManagerStateNotStarted:
      title = l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD);
      break;
    case kDownloadManagerStateInProgress:
      break;
    case kDownloadManagerStateSuceeded:
      title = l10n_util::GetNSString(IDS_IOS_OPEN_IN);
      break;
    case kDownloadManagerStateFailed:
      title = l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_TRY_AGAIN);
      break;
  }

  [self.actionButton setTitle:title forState:UIControlStateNormal];
  self.actionButton.hidden = _state == kDownloadManagerStateInProgress;
}

// Updates alpha value for install google drive controls row.
// Makes whole installDriveControlsRow opaque if
// _installDriveButtonVisible is set to YES, otherwise makes the row
// fully transparent.
- (void)updateInstallDriveControlsRow {
  self.installDriveControlsRow.alpha = _installDriveButtonVisible ? 1.0f : 0.0f;
}

@end
