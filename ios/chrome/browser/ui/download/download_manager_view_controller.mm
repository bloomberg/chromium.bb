// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/download/radial_progress_view.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kDownloadManagerNotStartedImage = @"download_file";
NSString* const kDownloadManagerInProgressImage = @"download_progress";
NSString* const kDownloadManagerSucceededImage = @"download_done";
NSString* const kDownloadManagerFailedImage = @"download_error";

namespace {
// Layout Guide name for action button UILayoutGuide.
GuideName* const kActionButtonGuide = @"kDownloadManagerActionButtonGuide";

// A margin for every control element (status label, action button, close
// button). Defines the minimal distance between elements.
const CGFloat kElementMargin = 16;

// Ideal download UI width if user interface has regular width. For compact
// user interface width download UI will take full width of the superview.
const CGFloat kPreferredWidthForRegularUIWidth = 500;

// Returns formatted size string.
NSString* GetSizeString(long long size_in_bytes) {
  return [NSByteCountFormatter
      stringFromByteCount:size_in_bytes
               countStyle:NSByteCountFormatterCountStyleFile];
}
}  // namespace

@interface DownloadManagerViewController () {
  UIButton* _closeButton;
  UIImageView* _statusIcon;
  UILabel* _statusLabel;
  UIButton* _actionButton;
  UIButton* _installDriveButton;
  UIImageView* _installDriveIcon;
  UILabel* _installDriveLabel;
  RadialProgressView* _progressView;

  NSString* _fileName;
  int64_t _countOfBytesReceived;
  int64_t _countOfBytesExpectedToReceive;
  float _progress;
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

// Grey line which separates downloadControlsRow and installDriveControlsRow.
@property(nonatomic, readonly) UIView* horizontalLine;

// Represents constraint for self.view.bottomAnchor, which can either be
// constrained to installDriveControlsRow's bottomAnchor or to
// downloadControlsRow's bottomAnchor. Stored in a property to allow
// deactivating the old constraint.
@property(nonatomic) NSLayoutConstraint* bottomConstraint;

// Represents constraint for self.view.widthAnchor, which can either be
// constrained to superview's width or to kPreferredWidthForRegularUIWidth
// constant. Stored in a property to allow deactivating the old constraint.
@property(nonatomic) NSLayoutConstraint* widthConstraint;

@end

@implementation DownloadManagerViewController

@synthesize delegate = _delegate;
@synthesize shadow = _shadow;
@synthesize background = _background;
@synthesize downloadControlsRow = _downloadControlsRow;
@synthesize installDriveControlsRow = _installDriveControlsRow;
@synthesize horizontalLine = _horizontalLine;
@synthesize bottomConstraint = _bottomConstraint;
@synthesize widthConstraint = _widthConstraint;

#pragma mark - UIViewController overrides

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.view addSubview:self.shadow];
  [self.view addSubview:self.background];
  [self.view addSubview:self.downloadControlsRow];
  [self.view addSubview:self.installDriveControlsRow];
  [self.downloadControlsRow addSubview:self.closeButton];
  [self.downloadControlsRow addSubview:self.statusIcon];
  [self.downloadControlsRow addSubview:self.statusLabel];
  [self.downloadControlsRow addSubview:self.progressView];
  [self.downloadControlsRow addSubview:self.actionButton];
  [self.installDriveControlsRow addSubview:self.installDriveButton];
  [self.installDriveControlsRow addSubview:self.installDriveIcon];
  [self.installDriveControlsRow addSubview:self.installDriveLabel];
  [self.installDriveControlsRow addSubview:self.horizontalLine];

  NamedGuide* actionButtonGuide =
      [[NamedGuide alloc] initWithName:kActionButtonGuide];
  [self.view addLayoutGuide:actionButtonGuide];
  actionButtonGuide.constrainedView = self.actionButton;
}

- (void)updateViewConstraints {
  [super updateViewConstraints];

  // self.view constraints.
  UIView* view = self.view;
  [self updateBottomConstraints];
  [self updateWidthConstraints];

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
  UIButton* closeButton = self.closeButton;
  // Account for bottom white pixel on shadow image.
  CGFloat shadowHeight = CGRectGetHeight(shadow.frame) - 1;
  [NSLayoutConstraint activateConstraints:@[
    [downloadRow.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [downloadRow.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
    [downloadRow.topAnchor constraintEqualToAnchor:view.topAnchor
                                          constant:shadowHeight],
    [downloadRow.layoutMarginsGuide.heightAnchor
        constraintEqualToAnchor:closeButton.heightAnchor],
  ]];

  // install drive controls row constraints.
  UIView* installDriveRow = self.installDriveControlsRow;
  UIButton* installDriveButton = self.installDriveButton;
  [NSLayoutConstraint activateConstraints:@[
    [installDriveRow.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [installDriveRow.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],
    [installDriveRow.topAnchor
        constraintEqualToAnchor:downloadRow.bottomAnchor],
    [installDriveRow.heightAnchor
        constraintEqualToAnchor:downloadRow.heightAnchor],
  ]];

  // close button constraints.
  [NSLayoutConstraint activateConstraints:@[
    [closeButton.centerYAnchor
        constraintEqualToAnchor:downloadRow.centerYAnchor],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:downloadRow.layoutMarginsGuide.trailingAnchor],
  ]];

  // status icon constraints.
  UIImageView* statusIcon = self.statusIcon;
  [NSLayoutConstraint activateConstraints:@[
    [statusIcon.centerYAnchor
        constraintEqualToAnchor:downloadRow.centerYAnchor],
    [statusIcon.leadingAnchor
        constraintEqualToAnchor:downloadRow.layoutMarginsGuide.leadingAnchor],
  ]];

  // progress view constraints.
  RadialProgressView* progressView = self.progressView;
  [NSLayoutConstraint activateConstraints:@[
    [progressView.leadingAnchor
        constraintEqualToAnchor:statusIcon.leadingAnchor],
    [progressView.trailingAnchor
        constraintEqualToAnchor:statusIcon.trailingAnchor],
    [progressView.topAnchor constraintEqualToAnchor:statusIcon.topAnchor],
    [progressView.bottomAnchor constraintEqualToAnchor:statusIcon.bottomAnchor],
  ]];

  // status label constraints.
  UILabel* statusLabel = self.statusLabel;
  UIButton* actionButton = self.actionButton;
  [NSLayoutConstraint activateConstraints:@[
    [statusLabel.centerYAnchor
        constraintEqualToAnchor:downloadRow.centerYAnchor],
    [statusLabel.leadingAnchor constraintEqualToAnchor:statusIcon.trailingAnchor
                                              constant:kElementMargin],
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

  // install google drive icon constraints.
  UIImageView* installDriveIcon = self.installDriveIcon;
  [NSLayoutConstraint activateConstraints:@[
    [installDriveIcon.centerYAnchor
        constraintEqualToAnchor:installDriveRow.centerYAnchor],
    [installDriveIcon.leadingAnchor
        constraintEqualToAnchor:installDriveRow.layoutMarginsGuide
                                    .leadingAnchor],
  ]];

  // install google drive label constraints.
  UILabel* installDriveLabel = self.installDriveLabel;
  [NSLayoutConstraint activateConstraints:@[
    [installDriveLabel.centerYAnchor
        constraintEqualToAnchor:installDriveRow.centerYAnchor],
    [installDriveLabel.leadingAnchor
        constraintEqualToAnchor:installDriveIcon.trailingAnchor
                       constant:kElementMargin],
    [installDriveLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:installDriveButton.leadingAnchor
                                 constant:-kElementMargin],
  ]];

  // constraint line which separates download controls and install drive rows.
  UIView* horizontalLine = self.horizontalLine;
  [NSLayoutConstraint activateConstraints:@[
    [horizontalLine.heightAnchor constraintEqualToConstant:1],
    [horizontalLine.topAnchor
        constraintEqualToAnchor:installDriveRow.topAnchor],
    [horizontalLine.leadingAnchor
        constraintEqualToAnchor:installDriveRow.leadingAnchor],
    [horizontalLine.trailingAnchor
        constraintEqualToAnchor:installDriveRow.trailingAnchor],
  ]];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [self updateWidthConstraints];
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

- (void)setProgress:(float)value {
  if (_progress != value) {
    _progress = value;
    [self updateProgressView];
  }
}

- (void)setState:(DownloadManagerState)state {
  if (_state != state) {
    _state = state;
    [self updateStatusIcon];
    [self updateStatusLabel];
    [self updateActionButton];
    [self updateProgressView];
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

- (UIImageView*)statusIcon {
  if (!_statusIcon) {
    _statusIcon = [[UIImageView alloc] initWithFrame:CGRectZero];
    _statusIcon.translatesAutoresizingMaskIntoConstraints = NO;
    [self updateStatusIcon];
  }
  return _statusIcon;
}

- (UILabel*)statusLabel {
  if (!_statusLabel) {
    _statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _statusLabel.font = [MDCTypography subheadFont];
    [_statusLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [self updateStatusLabel];
  }
  return _statusLabel;
}

- (UIButton*)actionButton {
  if (!_actionButton) {
    _actionButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _actionButton.translatesAutoresizingMaskIntoConstraints = NO;
    _actionButton.exclusiveTouch = YES;
    [_actionButton setTitleColor:[MDCPalette bluePalette].tint600
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
    [_installDriveButton setTitleColor:[MDCPalette bluePalette].tint600
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

- (UIImageView*)installDriveIcon {
  if (!_installDriveIcon) {
    _installDriveIcon = [[UIImageView alloc] initWithFrame:CGRectZero];
    _installDriveIcon.translatesAutoresizingMaskIntoConstraints = NO;
    _installDriveIcon.image = ios::GetChromeBrowserProvider()
                                  ->GetBrandedImageProvider()
                                  ->GetDownloadGoogleDriveImage();
  }
  return _installDriveIcon;
}

- (UILabel*)installDriveLabel {
  if (!_installDriveLabel) {
    _installDriveLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _installDriveLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _installDriveLabel.font = [MDCTypography subheadFont];
    _installDriveLabel.text =
        l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_GOOGLE_DRIVE);
    [_installDriveLabel sizeToFit];
  }
  return _installDriveLabel;
}

- (RadialProgressView*)progressView {
  if (!_progressView) {
    _progressView = [[RadialProgressView alloc] initWithFrame:CGRectZero];
    _progressView.translatesAutoresizingMaskIntoConstraints = NO;
    _progressView.lineWidth = 2;
    _progressView.tintColor = [MDCPalette bluePalette].tint500;
    [self updateProgressView];
  }
  return _progressView;
}

- (UIView*)horizontalLine {
  if (!_horizontalLine) {
    _horizontalLine = [[UIView alloc] init];
    _horizontalLine.translatesAutoresizingMaskIntoConstraints = NO;
    _horizontalLine.backgroundColor = [MDCPalette greyPalette].tint300;
  }
  return _horizontalLine;
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
    case kDownloadManagerStateSucceeded: {
      SEL selector = @selector
          (downloadManagerViewController:presentOpenInMenuWithLayoutGuide:);
      if ([_delegate respondsToSelector:selector]) {
        UILayoutGuide* guide =
            [NamedGuide guideWithName:kActionButtonGuide view:self.view];
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

// Updates and activates self.widthConstraint. self.widthConstraint
// constraints self.view.widthAnchor to superview's width if user interface has
// compact width or there is no room to use kPreferredWidthForRegularUIWidth.
// Otherwise self.widthConstraint constraints self.view.widthAnchor to
// kPreferredWidthForRegularUIWidth constant.
- (void)updateWidthConstraints {
  self.widthConstraint.active = NO;

  NSLayoutDimension* firstAnchor = self.view.widthAnchor;
  CGFloat superviewWidth = self.view.superview.frame.size.width;
  if (superviewWidth < kPreferredWidthForRegularUIWidth || IsCompactWidth()) {
    // Superview is too narrow to accomodate kPreferredWidthForRegularUIWidth or
    // user interface has compant width. In this case download view should
    // occupy all superview width.
    NSLayoutDimension* secondAnchor = self.view.superview.widthAnchor;
    self.widthConstraint = [firstAnchor constraintEqualToAnchor:secondAnchor];
  } else {
    // Superview is wide enough to accomodate kPreferredWidthForRegularUIWidth
    // or user interface has regular width. In this case download view width
    // should be constant. Otherwise the UI will jhave too much blank space.
    self.widthConstraint = [firstAnchor
        constraintEqualToConstant:kPreferredWidthForRegularUIWidth];
  }

  self.widthConstraint.active = YES;
}

// Updates status icon image depending on |state|.
- (void)updateStatusIcon {
  NSString* imageName = nil;
  switch (_state) {
    case kDownloadManagerStateNotStarted:
      imageName = kDownloadManagerNotStartedImage;
      break;
    case kDownloadManagerStateInProgress:
      imageName = kDownloadManagerInProgressImage;
      break;
    case kDownloadManagerStateSucceeded:
      imageName = kDownloadManagerSucceededImage;
      break;
    case kDownloadManagerStateFailed:
      imageName = kDownloadManagerFailedImage;
      break;
  }
  DCHECK(imageName);
  self.statusIcon.image = [UIImage imageNamed:imageName];
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
    case kDownloadManagerStateSucceeded:
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
    case kDownloadManagerStateSucceeded:
      title = l10n_util::GetNSString(IDS_IOS_OPEN_IN);
      break;
    case kDownloadManagerStateFailed:
      title = l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_TRY_AGAIN);
      break;
  }

  [self.actionButton setTitle:title forState:UIControlStateNormal];
  self.actionButton.hidden = _state == kDownloadManagerStateInProgress;
}

- (void)updateProgressView {
  self.progressView.hidden = _state != kDownloadManagerStateInProgress;
  self.progressView.progress = _progress;
}

// Updates alpha value for install google drive controls row.
// Makes whole installDriveControlsRow opaque if
// _installDriveButtonVisible is set to YES, otherwise makes the row
// fully transparent.
- (void)updateInstallDriveControlsRow {
  self.installDriveControlsRow.alpha = _installDriveButtonVisible ? 1.0f : 0.0f;
}

@end
