// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid_cell.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/tab_grid/top_aligned_image_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the top bar containing the icon, title, and close button.
const CGFloat kTopBarHeight = 28.0f;
// Width and height of the icon.
const CGFloat kIconDiameter = 20.0f;
// Width and height of the close button.
const CGFloat kCloseButtonDiameter = 16.0f;
// Width of selection border.
const CGFloat kBorderWidth = 6.0f;
}  // namespace

@interface GridCell ()
// Visual components of the cell.
@property(nonatomic, weak) UIView* topBar;
@property(nonatomic, weak) UIView* line;
@property(nonatomic, weak) UIImageView* iconView;
@property(nonatomic, weak) TopAlignedImageView* snapshotView;
@property(nonatomic, weak) UILabel* titleLabel;
@property(nonatomic, weak) UIButton* closeButton;
@property(nonatomic, weak) UIView* border;
@end

@implementation GridCell
// Public properties.
@synthesize delegate = _delegate;
@synthesize theme = _theme;
@synthesize itemIdentifier = _itemIdentifier;
@synthesize icon = _icon;
@synthesize snapshot = _snapshot;
@synthesize title = _title;
// Private properties.
@synthesize topBar = _topBar;
@synthesize line = _line;
@synthesize iconView = _iconView;
@synthesize snapshotView = _snapshotView;
@synthesize titleLabel = _titleLabel;
@synthesize closeButton = _closeButton;
@synthesize border = _border;

// |-dequeueReusableCellWithReuseIdentifier:forIndexPath:| calls this method to
// initialize a cell.
- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setupSelectedBackgroundView];
    UIView* contentView = self.contentView;
    contentView.layer.cornerRadius = 11.0f;
    contentView.layer.masksToBounds = YES;
    UIView* topBar = [self setupTopBar];
    UIView* line = [[UIView alloc] init];
    line.translatesAutoresizingMaskIntoConstraints = NO;
    TopAlignedImageView* snapshotView = [[TopAlignedImageView alloc] init];
    snapshotView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:topBar];
    [contentView addSubview:line];
    [contentView addSubview:snapshotView];
    _topBar = topBar;
    _line = line;
    _snapshotView = snapshotView;
    NSArray* constraints = @[
      [topBar.topAnchor constraintEqualToAnchor:contentView.topAnchor],
      [topBar.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor],
      [topBar.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [topBar.heightAnchor constraintEqualToConstant:kTopBarHeight],
      [line.topAnchor constraintEqualToAnchor:topBar.bottomAnchor],
      [line.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor],
      [line.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor],
      [line.heightAnchor constraintEqualToConstant:0.5f],
      [snapshotView.topAnchor constraintEqualToAnchor:line.bottomAnchor],
      [snapshotView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor],
      [snapshotView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [snapshotView.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

#pragma mark - UICollectionViewCell

- (void)setHighlighted:(BOOL)highlighted {
  // NO-OP to disable highlighting and only allow selection.
}

#pragma mark - Public

// Updates the theme to either dark or light. Updating is only done if the
// current theme is not the desired theme.
- (void)setTheme:(GridTheme)theme {
  if (_theme == theme)
    return;
  switch (theme) {
    case GridThemeLight:
      self.topBar.backgroundColor = [UIColor whiteColor];
      self.iconView.backgroundColor = [UIColor colorWithWhite:0.9f alpha:1.0f];
      self.titleLabel.textColor = [UIColor blackColor];
      self.line.backgroundColor = [UIColor colorWithWhite:0.6f alpha:1.0f];
      self.snapshotView.backgroundColor = [UIColor whiteColor];
      self.closeButton.backgroundColor =
          [UIColor colorWithWhite:0.6f alpha:1.0f];
      self.closeButton.tintColor = [UIColor whiteColor];
      self.tintColor =
          [UIColor colorWithRed:0.0 green:122.0 / 255.0 blue:1.0 alpha:1.0];
      self.border.layer.borderColor = self.tintColor.CGColor;
      break;
    case GridThemeDark:
      self.topBar.backgroundColor = [UIColor colorWithWhite:0.2f alpha:1.0f];
      self.iconView.backgroundColor = [UIColor colorWithWhite:0.9f alpha:1.0f];
      self.titleLabel.textColor = [UIColor whiteColor];
      self.line.backgroundColor = [UIColor colorWithWhite:0.2f alpha:1.0f];
      self.snapshotView.backgroundColor =
          [UIColor colorWithWhite:0.4f alpha:1.0f];
      self.closeButton.backgroundColor =
          [UIColor colorWithWhite:0.4f alpha:1.0f];
      self.closeButton.tintColor = [UIColor colorWithWhite:0.2f alpha:1.0f];
      self.tintColor = [UIColor colorWithWhite:0.9f alpha:1.0f];
      self.border.layer.borderColor = self.tintColor.CGColor;
      break;
    default:
      NOTREACHED() << "Invalid GridTheme.";
      break;
  }
  _theme = theme;
}

- (void)setIcon:(UIImage*)icon {
  self.iconView.image = icon;
  _icon = icon;
}

- (void)setSnapshot:(UIImage*)snapshot {
  self.snapshotView.image = snapshot;
  _snapshot = snapshot;
}

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
  _title = title;
}

#pragma mark - Private

// Sets up the top bar with icon, title, and close button.
- (UIView*)setupTopBar {
  UIView* topBar = [[UIView alloc] init];
  topBar.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageView* iconView = [[UIImageView alloc] init];
  iconView.translatesAutoresizingMaskIntoConstraints = NO;
  iconView.contentMode = UIViewContentModeScaleAspectFill;
  iconView.layer.cornerRadius = 4.0f;
  iconView.layer.masksToBounds = YES;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont boldSystemFontOfSize:12.0f];

  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  closeButton.layer.cornerRadius = 8.0f;
  closeButton.layer.masksToBounds = YES;
  UIImage* closeImage = [[UIImage imageNamed:@"card_close_button"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [closeButton setImage:closeImage forState:UIControlStateNormal];
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped:)
        forControlEvents:UIControlEventTouchUpInside];

  [topBar addSubview:iconView];
  [topBar addSubview:titleLabel];
  [topBar addSubview:closeButton];
  _iconView = iconView;
  _titleLabel = titleLabel;
  _closeButton = closeButton;

  NSArray* constraints = @[
    [iconView.leadingAnchor constraintEqualToAnchor:topBar.leadingAnchor
                                           constant:4.0f],
    [iconView.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],
    [iconView.widthAnchor constraintEqualToConstant:kIconDiameter],
    [iconView.heightAnchor constraintEqualToConstant:kIconDiameter],
    [titleLabel.leadingAnchor constraintEqualToAnchor:iconView.trailingAnchor
                                             constant:4.0f],
    [titleLabel.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],
    [closeButton.leadingAnchor
        constraintEqualToAnchor:titleLabel.trailingAnchor],
    [closeButton.centerYAnchor constraintEqualToAnchor:topBar.centerYAnchor],
    [closeButton.widthAnchor constraintEqualToConstant:kCloseButtonDiameter],
    [closeButton.heightAnchor constraintEqualToConstant:kCloseButtonDiameter],
    [closeButton.trailingAnchor constraintEqualToAnchor:topBar.trailingAnchor
                                               constant:-6.0f],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
  return topBar;
}

// Sets up the selection border. The tint color is set when the theme is
// selected.
- (void)setupSelectedBackgroundView {
  self.selectedBackgroundView = [[UIView alloc] init];
  self.selectedBackgroundView.backgroundColor = [UIColor blackColor];
  UIView* border = [[UIView alloc] init];
  border.translatesAutoresizingMaskIntoConstraints = NO;
  border.backgroundColor = [UIColor blackColor];
  border.layer.cornerRadius = 17.0f;
  border.layer.borderWidth = 4.0f;
  [self.selectedBackgroundView addSubview:border];
  _border = border;
  [NSLayoutConstraint activateConstraints:@[
    [border.topAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.topAnchor
                       constant:-kBorderWidth],
    [border.leadingAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.leadingAnchor
                       constant:-kBorderWidth],
    [border.trailingAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.trailingAnchor
                       constant:kBorderWidth],
    [border.bottomAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.bottomAnchor
                       constant:kBorderWidth]
  ]];
}

// Selector registered to the close button.
- (void)closeButtonTapped:(id)sender {
  [self.delegate closeButtonTappedForCell:self];
}

@end
