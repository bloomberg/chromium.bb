// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/tab_grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_top_toolbar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridViewController ()
// Child view controllers.
@property(nonatomic, strong) GridViewController* regularTabsViewController;
@property(nonatomic, strong) GridViewController* incognitoTabsViewController;
@property(nonatomic, strong) UIViewController* remoteTabsViewController;
// Other UI components.
@property(nonatomic, weak) UIScrollView* scrollView;
@property(nonatomic, weak) UIView* scrollContentView;
@property(nonatomic, weak) TabGridTopToolbar* topToolbar;
@property(nonatomic, weak) TabGridBottomToolbar* bottomToolbar;
@property(nonatomic, weak) UIButton* closeAllButton;
@property(nonatomic, weak) UIButton* doneButton;
@property(nonatomic, weak) UIButton* floatingButton;
@end

@implementation TabGridViewController
// Public properties.
@synthesize regularTabsDelegate = _regularTabsDelegate;
@synthesize incognitoTabsDelegate = _incognitoTabsDelegate;
@synthesize regularTabsImageDataSource = _regularTabsImageDataSource;
@synthesize incognitoTabsImageDataSource = _incognitoTabsImageDataSource;
@synthesize currentPage = _currentPage;
// Private properties.
@synthesize regularTabsViewController = _regularTabsViewController;
@synthesize incognitoTabsViewController = _incognitoTabsViewController;
@synthesize remoteTabsViewController = _remoteTabsViewController;
@synthesize scrollView = _scrollView;
@synthesize scrollContentView = _scrollContentView;
@synthesize topToolbar = _topToolbar;
@synthesize bottomToolbar = _bottomToolbar;
@synthesize closeAllButton = _closeAllButton;
@synthesize doneButton = _doneButton;
@synthesize floatingButton = _floatingButton;

- (instancetype)init {
  if (self = [super init]) {
    _regularTabsViewController = [[GridViewController alloc] init];
    _incognitoTabsViewController = [[GridViewController alloc] init];
    _remoteTabsViewController = [[UIViewController alloc] init];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setupScrollView];
  [self setupIncognitoTabsViewController];
  [self setupRegularTabsViewController];
  [self setupRemoteTabsViewController];
  [self setupTopToolbar];
  [self setupTopToolbarButtons];
  [self setupBottomToolbar];
  [self setupBottomToolbarButtons];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  // The content inset of the tab grids must be modified so that the toolbars
  // do not obscure the tabs. This may change depending on orientation.
  UIEdgeInsets contentInset = UIEdgeInsetsZero;
  if (@available(iOS 11, *)) {
    // Beginning with iPhoneX, there could be unsafe areas on the side margins
    // and bottom.
    contentInset = self.view.safeAreaInsets;
  } else {
    // Previously only the top had unsafe areas.
    contentInset.top = self.topLayoutGuide.length;
  }
  contentInset.top += self.topToolbar.intrinsicContentSize.height;
  if (self.view.frame.size.width < self.view.frame.size.height) {
    contentInset.bottom += self.bottomToolbar.intrinsicContentSize.height;
  }
  self.incognitoTabsViewController.gridView.contentInset = contentInset;
  self.regularTabsViewController.gridView.contentInset = contentInset;
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

#pragma mark - TabGridTransitionStateProvider properties

- (BOOL)selectedTabVisible {
  return NO;
}

#pragma mark - Public

- (id<GridConsumer>)regularTabsConsumer {
  return self.regularTabsViewController;
}

- (void)setRegularTabsDelegate:
    (id<GridViewControllerDelegate>)regularTabsDelegate {
  self.regularTabsViewController.delegate = regularTabsDelegate;
  _regularTabsDelegate = regularTabsDelegate;
}

- (void)setRegularTabsImageDataSource:
    (id<GridImageDataSource>)regularTabsImageDataSource {
  self.regularTabsViewController.imageDataSource = regularTabsImageDataSource;
  _regularTabsImageDataSource = regularTabsImageDataSource;
}

- (id<GridConsumer>)incognitoTabsConsumer {
  return self.incognitoTabsViewController;
}

- (void)setIncognitoTabsDelegate:
    (id<GridViewControllerDelegate>)incognitoTabsDelegate {
  self.incognitoTabsViewController.delegate = incognitoTabsDelegate;
  _incognitoTabsDelegate = incognitoTabsDelegate;
}

- (void)setIncognitoTabsImageDataSource:
    (id<GridImageDataSource>)incognitoTabsImageDataSource {
  self.incognitoTabsViewController.imageDataSource =
      incognitoTabsImageDataSource;
  _incognitoTabsImageDataSource = incognitoTabsImageDataSource;
}

#pragma mark - Private

// Adds the scroll view and sets constraints.
- (void)setupScrollView {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.scrollEnabled = YES;
  scrollView.pagingEnabled = YES;
  if (@available(iOS 11, *)) {
    // Ensures that scroll view does not add additional margins based on safe
    // areas.
    scrollView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
  }
  UIView* contentView = [[UIView alloc] init];
  contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:contentView];
  [self.view addSubview:scrollView];
  self.scrollContentView = contentView;
  self.scrollView = scrollView;
  NSArray* constraints = @[
    [contentView.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
    [contentView.bottomAnchor constraintEqualToAnchor:scrollView.bottomAnchor],
    [contentView.leadingAnchor
        constraintEqualToAnchor:scrollView.leadingAnchor],
    [contentView.trailingAnchor
        constraintEqualToAnchor:scrollView.trailingAnchor],
    [contentView.heightAnchor constraintEqualToAnchor:self.view.heightAnchor],
    [scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds the incognito tabs GridViewController as a contained view controller,
// and sets constraints.
- (void)setupIncognitoTabsViewController {
  UIView* contentView = self.scrollContentView;
  GridViewController* viewController = self.incognitoTabsViewController;
  viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:viewController];
  [contentView addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
  viewController.theme = GridThemeDark;
  if (@available(iOS 11, *)) {
    // Adjustments are made in |-viewWillLayoutSubviews|. Automatic adjustments
    // do not work well with the scrollview.
    viewController.gridView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
  }
  NSArray* constraints = @[
    [viewController.view.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [viewController.view.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [viewController.view.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [viewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor]
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds the regular tabs GridViewController as a contained view controller,
// and sets constraints.
- (void)setupRegularTabsViewController {
  UIView* contentView = self.scrollContentView;
  GridViewController* viewController = self.regularTabsViewController;
  viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:viewController];
  [contentView addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
  viewController.theme = GridThemeLight;
  if (@available(iOS 11, *)) {
    // Adjustments are made in |-viewWillLayoutSubviews|. Automatic adjustments
    // do not work well with the scrollview.
    viewController.gridView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
  }
  NSArray* constraints = @[
    [viewController.view.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [viewController.view.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [viewController.view.leadingAnchor
        constraintEqualToAnchor:self.incognitoTabsViewController.view
                                    .trailingAnchor],
    [viewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor]
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds the remote tabs view controller as a contained view controller, and
// sets constraints.
- (void)setupRemoteTabsViewController {
  UIView* contentView = self.scrollContentView;
  UIViewController* viewController = self.remoteTabsViewController;
  viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  viewController.view.backgroundColor = [UIColor greenColor];
  [self addChildViewController:viewController];
  [contentView addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
  NSArray* constraints = @[
    [viewController.view.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [viewController.view.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [viewController.view.leadingAnchor
        constraintEqualToAnchor:self.regularTabsViewController.view
                                    .trailingAnchor],
    [viewController.view.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
    [viewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor]
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds the top toolbar and sets constraints.
- (void)setupTopToolbar {
  TabGridTopToolbar* topToolbar = [[TabGridTopToolbar alloc] init];
  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:topToolbar];
  self.topToolbar = topToolbar;
  NSArray* constraints = @[
    [topToolbar.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [topToolbar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [topToolbar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
  // Set the height of the toolbar, including unsafe areas.
  if (@available(iOS 11, *)) {
    // SafeArea is only available in iOS  11+.
    [topToolbar.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:topToolbar.intrinsicContentSize.height]
        .active = YES;
  } else {
    // Top and bottom layout guides are deprecated starting in iOS 11.
    [topToolbar.bottomAnchor
        constraintEqualToAnchor:self.topLayoutGuide.bottomAnchor
                       constant:topToolbar.intrinsicContentSize.height]
        .active = YES;
  }
}

// Adds the bottom toolbar and sets constraints.
- (void)setupBottomToolbar {
  TabGridBottomToolbar* bottomToolbar = [[TabGridBottomToolbar alloc] init];
  bottomToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:bottomToolbar];
  self.bottomToolbar = bottomToolbar;
  NSArray* constraints = @[
    [bottomToolbar.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [bottomToolbar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [bottomToolbar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
  // Adds the height of the toolbar above the bottom safe area.
  if (@available(iOS 11, *)) {
    // SafeArea is only available in iOS  11+.
    [self.view.safeAreaLayoutGuide.bottomAnchor
        constraintEqualToAnchor:bottomToolbar.topAnchor
                       constant:bottomToolbar.intrinsicContentSize.height]
        .active = YES;
  } else {
    // Top and bottom layout guides are deprecated starting in iOS 11.
    [bottomToolbar.topAnchor
        constraintEqualToAnchor:self.bottomLayoutGuide.topAnchor
                       constant:-bottomToolbar.intrinsicContentSize.height]
        .active = YES;
  }
}

// Adds the top toolbar buttons.
- (void)setupTopToolbarButtons {
  self.doneButton = self.topToolbar.leadingButton;
  self.closeAllButton = self.topToolbar.trailingButton;
  [self.doneButton setTitle:@"Done" forState:UIControlStateNormal];
  [self.closeAllButton setTitle:@"Close All" forState:UIControlStateNormal];
  [self.doneButton addTarget:self
                      action:@selector(doneButtonTapped:)
            forControlEvents:UIControlEventTouchUpInside];
  [self.closeAllButton addTarget:self
                          action:@selector(closeAllButtonTapped:)
                forControlEvents:UIControlEventTouchUpInside];
}

// Adds the bottom toolbar buttons.
- (void)setupBottomToolbarButtons {
  [self.bottomToolbar.leadingButton setTitle:@"Done"
                                    forState:UIControlStateNormal];
  [self.bottomToolbar.trailingButton setTitle:@"Close All"
                                     forState:UIControlStateNormal];
  [self.bottomToolbar.roundButton setTitle:@"New"
                                  forState:UIControlStateNormal];
  [self.bottomToolbar.leadingButton addTarget:self
                                       action:@selector(doneButtonTapped:)
                             forControlEvents:UIControlEventTouchUpInside];
  [self.bottomToolbar.trailingButton addTarget:self
                                        action:@selector(closeAllButtonTapped:)
                              forControlEvents:UIControlEventTouchUpInside];
  [self.bottomToolbar.roundButton addTarget:self
                                     action:@selector(newTabButtonTapped:)
                           forControlEvents:UIControlEventTouchUpInside];
}

#pragma mark - Button actions

- (void)doneButtonTapped:(id)sender {
  // TODO(crbug.com/804503) : Placeholder alerts.
}

- (void)closeAllButtonTapped:(id)sender {
  // TODO(crbug.com/804503) : Placeholder alerts.
}

- (void)newTabButtonTapped:(id)sender {
  // TODO(crbug.com/804503) : Placeholder alerts.
}

@end
