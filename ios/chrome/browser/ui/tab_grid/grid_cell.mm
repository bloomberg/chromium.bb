// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid_cell.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/tab_grid/grid_item.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GridCell ()
// A boolean to ensure setup is completed once.
@property(nonatomic, assign) BOOL isSetup;
// Current theme of the cell.
@property(nonatomic, assign) GridCellTheme currentTheme;
// Visual components of the cell.
@property(nonatomic, strong) UIView* topBar;
@property(nonatomic, strong) UIView* line;
@property(nonatomic, strong) UIImageView* icon;
@property(nonatomic, strong) UIImageView* snapshot;
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UIButton* closeButton;
@end

@implementation GridCell
@synthesize isSetup = _isSetup;
@synthesize currentTheme = _currentTheme;
@synthesize topBar = _topBar;
@synthesize line = _line;
@synthesize icon = _icon;
@synthesize snapshot = _snapshot;
@synthesize titleLabel = _titleLabel;
@synthesize closeButton = _closeButton;

#pragma mark - Public

- (void)configureWithItem:(GridItem*)item theme:(GridCellTheme)theme {
  [self setupContentViewIfNeeded];
  [self updateThemeIfNeeded:theme];
  self.titleLabel.text = item.title;
}

#pragma mark - Private

// Adds all the subviews onto the contentView. Activates all constraints. The
// property |isSetup| ensures that this method is only called once.
- (void)setupContentViewIfNeeded {
  if (self.isSetup)
    return;

  self.contentView.layer.cornerRadius = 11.0f;
  self.contentView.layer.masksToBounds = YES;

  self.topBar = [self setupTopBar];
  self.line = [[UIView alloc] init];
  self.line.translatesAutoresizingMaskIntoConstraints = NO;
  self.snapshot = [[UIImageView alloc] initWithFrame:CGRectZero];
  self.snapshot.translatesAutoresizingMaskIntoConstraints = NO;

  [self.contentView addSubview:self.topBar];
  [self.contentView addSubview:self.line];
  [self.contentView addSubview:self.snapshot];
  NSDictionary* views = @{
    @"bar" : self.topBar,
    @"line" : self.line,
    @"snapshot" : self.snapshot,
  };
  NSArray* constraints = @[
    @"H:|-0-[bar]-0-|",
    @"H:|-0-[line]-0-|",
    @"H:|-0-[snapshot]-0-|",
    @"V:|-0-[bar(==28)]-0-[line(==0.5)]-0-[snapshot]-0-|",
  ];
  ApplyVisualConstraints(constraints, views, self.contentView);
  self.isSetup = YES;
}

// Sets up the top bar with icon, title, and close button.
- (UIView*)setupTopBar {
  UIView* topBar = [[UIView alloc] initWithFrame:CGRectZero];
  topBar.translatesAutoresizingMaskIntoConstraints = NO;

  self.icon = [[UIImageView alloc] initWithFrame:CGRectZero];
  self.icon.translatesAutoresizingMaskIntoConstraints = NO;
  self.icon.layer.cornerRadius = 2.0f;
  self.icon.layer.masksToBounds = YES;

  self.titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.titleLabel.font = [UIFont boldSystemFontOfSize:12.0f];

  self.closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
  self.closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.closeButton.layer.cornerRadius = 8.0f;
  self.closeButton.layer.masksToBounds = YES;
  UIImage* closeImage = [[UIImage imageNamed:@"card_close_button"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [self.closeButton setImage:closeImage forState:UIControlStateNormal];

  [topBar addSubview:self.icon];
  [topBar addSubview:self.titleLabel];
  [topBar addSubview:self.closeButton];
  NSDictionary* views = @{
    @"icon" : self.icon,
    @"title" : self.titleLabel,
    @"close" : self.closeButton,
  };
  NSArray* constraints = @[
    @"H:|-4-[icon(==20)]-4-[title]-0-[close(==16)]-6-|",
    @"V:[icon(==20)]",
    @"V:[close(==16)]",
  ];
  ApplyVisualConstraints(constraints, views, topBar);
  AddSameCenterYConstraint(topBar, self.icon);
  AddSameCenterYConstraint(topBar, self.titleLabel);
  AddSameCenterYConstraint(topBar, self.closeButton);
  return topBar;
}

// Updates the theme to either dark or light. Updating is only done if the
// current theme is not the desired theme.
- (void)updateThemeIfNeeded:(GridCellTheme)theme {
  if (self.currentTheme == theme)
    return;
  switch (theme) {
    case GridCellThemeLight:
      self.topBar.backgroundColor = [UIColor whiteColor];
      self.icon.backgroundColor = [UIColor colorWithWhite:0.9f alpha:1.0f];
      self.titleLabel.textColor = [UIColor blackColor];
      self.line.backgroundColor = [UIColor colorWithWhite:0.6f alpha:1.0f];
      self.snapshot.backgroundColor = [UIColor whiteColor];
      self.closeButton.backgroundColor =
          [UIColor colorWithWhite:0.6f alpha:1.0f];
      self.closeButton.tintColor = [UIColor whiteColor];
      break;
    case GridCellThemeDark:
      self.topBar.backgroundColor = [UIColor colorWithWhite:0.2f alpha:1.0f];
      self.icon.backgroundColor = [UIColor colorWithWhite:0.9f alpha:1.0f];
      self.titleLabel.textColor = [UIColor whiteColor];
      self.line.backgroundColor = [UIColor colorWithWhite:0.2f alpha:1.0f];
      self.snapshot.backgroundColor = [UIColor colorWithWhite:0.4f alpha:1.0f];
      self.closeButton.backgroundColor =
          [UIColor colorWithWhite:0.4f alpha:1.0f];
      self.closeButton.tintColor = [UIColor colorWithWhite:0.2f alpha:1.0f];
      break;
    default:
      NOTREACHED() << "Invalid GridCellTheme.";
      break;
  }
  self.currentTheme = theme;
}

@end
