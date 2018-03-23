// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_empty_state_view.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_top_toolbar.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Types of configurations of this view controller.
typedef NS_ENUM(NSUInteger, TabGridConfiguration) {
  TabGridConfigurationBottomToolbar = 1,
  TabGridConfigurationFloatingButton,
};
}  // namespace

@interface TabGridViewController ()<GridViewControllerDelegate,
                                    UIScrollViewAccessibilityDelegate>
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
// Clang does not allow property getters to start with the reserved word "new",
// but provides a workaround. The getter must be set before the property is
// declared.
- (TabGridNewTabButton*)newTabButton __attribute__((objc_method_family(none)));
@property(nonatomic, weak) TabGridNewTabButton* newTabButton;
@property(nonatomic, weak) TabGridNewTabButton* floatingButton;
@property(nonatomic, assign) TabGridConfiguration configuration;
// The page that was shown when entering the tab grid from the tab view.
// This is used to decide whether the Done button is enabled.
@property(nonatomic, assign) TabGridPage originalPage;
@end

@implementation TabGridViewController
// Public properties.
@synthesize tabPresentationDelegate = _tabPresentationDelegate;
@synthesize regularTabsDelegate = _regularTabsDelegate;
@synthesize incognitoTabsDelegate = _incognitoTabsDelegate;
@synthesize regularTabsImageDataSource = _regularTabsImageDataSource;
@synthesize incognitoTabsImageDataSource = _incognitoTabsImageDataSource;
// TabGridPaging property.
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
@synthesize newTabButton = _newTabButton;
@synthesize floatingButton = _floatingButton;
@synthesize configuration = _configuration;
@synthesize originalPage = _originalPage;

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
  [self setupBottomToolbar];
  [self setupFloatingButton];
}

- (void)viewWillAppear:(BOOL)animated {
  // Call the current page setter to sync the scroll view offset to the current
  // page value.
  self.currentPage = _currentPage;
  self.originalPage = _currentPage;
  [self.topToolbar.pageControl setSelectedPage:self.currentPage animated:YES];
  [self configureViewControllerForCurrentSizeClassesAndPage];
  if (animated && self.transitionCoordinator) {
    [self animateToolbarsForAppearance];
  }
  [super viewWillAppear:animated];
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

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  auto animate = ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    // Call the current page setter to sync the scroll view offset to the
    // current page value.
    self.currentPage = _currentPage;
    [self configureViewControllerForCurrentSizeClassesAndPage];
  };
  [coordinator animateAlongsideTransition:animate completion:nil];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if (scrollView.dragging || scrollView.decelerating) {
    CGFloat offsetWidth =
        self.scrollView.contentSize.width - self.scrollView.frame.size.width;
    CGFloat offset = scrollView.contentOffset.x / offsetWidth;
    self.topToolbar.pageControl.sliderPosition = offset;

    // Bookkeeping for the current page.
    // TODO(crbug.com/822328) : Fix for RTL.
    CGFloat pageWidth = scrollView.frame.size.width;
    float fractionalPage = scrollView.contentOffset.x / pageWidth;
    NSUInteger page = lround(fractionalPage);
    if (page != self.currentPage) {
      _currentPage = static_cast<TabGridPage>(page);
      [self configureButtonsForOriginalAndCurrentPage];
    }
  }
}

#pragma mark - UIScrollViewAccessibilityDelegate

- (NSString*)accessibilityScrollStatusForScrollView:(UIScrollView*)scrollView {
  // This reads the new page whenever the user scrolls in VoiceOver.
  int stringID;
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      stringID = IDS_IOS_TAB_GRID_INCOGNITO_TABS_TITLE;
      break;
    case TabGridPageRegularTabs:
      stringID = IDS_IOS_TAB_GRID_REGULAR_TABS_TITLE;
      break;
    case TabGridPageRemoteTabs:
      stringID = IDS_IOS_TAB_GRID_REMOTE_TABS_TITLE;
      break;
  }
  return l10n_util::GetNSString(stringID);
}

#pragma mark - GridTransitionStateProviding properties

- (BOOL)isSelectedCellVisible {
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      return self.incognitoTabsViewController.selectedCellVisible;
    case TabGridPageRegularTabs:
      return self.regularTabsViewController.selectedCellVisible;
    case TabGridPageRemoteTabs:
      return NO;
  }
}

- (GridTransitionLayout*)layoutForTransitionContext:
    (id<UIViewControllerContextTransitioning>)context {
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      return [self.incognitoTabsViewController transitionLayout];
    case TabGridPageRegularTabs:
      return [self.regularTabsViewController transitionLayout];
    case TabGridPageRemoteTabs:
      return nil;
  }
}

- (UIView*)proxyContainerForTransitionContext:
    (id<UIViewControllerContextTransitioning>)context {
  return self.view;
}

- (UIView*)proxyPositionForTransitionContext:
    (id<UIViewControllerContextTransitioning>)context {
  return self.scrollView;
}

#pragma mark - Public

- (id<GridConsumer>)regularTabsConsumer {
  return self.regularTabsViewController;
}

- (void)setRegularTabsImageDataSource:
    (id<GridImageDataSource>)regularTabsImageDataSource {
  self.regularTabsViewController.imageDataSource = regularTabsImageDataSource;
  _regularTabsImageDataSource = regularTabsImageDataSource;
}

- (id<GridConsumer>)incognitoTabsConsumer {
  return self.incognitoTabsViewController;
}

- (void)setIncognitoTabsImageDataSource:
    (id<GridImageDataSource>)incognitoTabsImageDataSource {
  self.incognitoTabsViewController.imageDataSource =
      incognitoTabsImageDataSource;
  _incognitoTabsImageDataSource = incognitoTabsImageDataSource;
}

#pragma mark - TabGridPaging

- (void)setCurrentPage:(TabGridPage)currentPage {
  // This method should never early return if |currentPage| == |_currentPage|;
  // the ivar may have been set before the scroll view could be updated. Calling
  // this method should always update the scroll view's offset if possible.

  // If the view isn't loaded yet, just do bookkeeping on _currentPage.
  if (!self.viewLoaded) {
    _currentPage = currentPage;
    return;
  }
  CGFloat pageWidth = self.scrollView.frame.size.width;
  NSUInteger page = static_cast<NSUInteger>(currentPage);
  CGPoint offset = CGPointMake(page * pageWidth, 0);
  // If the view is visible, animate the change. Otherwise don't.
  if (self.view.window == nil) {
    self.scrollView.contentOffset = offset;
    _currentPage = currentPage;
  } else {
    [self.scrollView setContentOffset:offset animated:YES];
    // _currentPage is set in scrollViewDidEndDecelerating:
  }
}

#pragma mark - Private

// Adds the scroll view and sets constraints.
- (void)setupScrollView {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.scrollEnabled = YES;
  scrollView.pagingEnabled = YES;
  scrollView.delegate = self;
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
  viewController.emptyStateView =
      [[TabGridEmptyStateView alloc] initWithPage:TabGridPageIncognitoTabs];
  viewController.emptyStateView.accessibilityIdentifier =
      kTabGridIncognitoTabsEmptyStateIdentifier;
  viewController.theme = GridThemeDark;
  viewController.delegate = self;
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
  viewController.emptyStateView =
      [[TabGridEmptyStateView alloc] initWithPage:TabGridPageRegularTabs];
  viewController.emptyStateView.accessibilityIdentifier =
      kTabGridRegularTabsEmptyStateIdentifier;
  viewController.theme = GridThemeLight;
  viewController.delegate = self;
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
  // TODO(crbug.com/804588) : Create remote tabs.
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
  // Configure and initialize the page control.
  [self.topToolbar.pageControl addTarget:self
                                  action:@selector(pageControlChanged:)
                        forControlEvents:UIControlEventValueChanged];
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

// Adds floating button and constraints.
- (void)setupFloatingButton {
  TabGridNewTabButton* button = [TabGridNewTabButton
      buttonWithSizeClass:TabGridNewTabButtonSizeClassLarge];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:button];
  self.floatingButton = button;
  NSArray* constraints = @[
    [button.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kTabGridFloatingButtonHorizontalInset],
    [button.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor
                       constant:-kTabGridFloatingButtonVerticalInset]
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

- (void)configureViewControllerForCurrentSizeClassesAndPage {
  self.configuration = TabGridConfigurationFloatingButton;
  if (self.traitCollection.verticalSizeClass ==
          UIUserInterfaceSizeClassRegular &&
      self.traitCollection.horizontalSizeClass ==
          UIUserInterfaceSizeClassCompact) {
    // The only bottom toolbar configuration is when the UI is narrow but
    // vertically long.
    self.configuration = TabGridConfigurationBottomToolbar;
  }
  switch (self.configuration) {
    case TabGridConfigurationBottomToolbar:
      self.topToolbar.leadingButton.hidden = YES;
      self.topToolbar.trailingButton.hidden = YES;
      self.bottomToolbar.hidden = NO;
      self.floatingButton.hidden = YES;
      self.doneButton = self.bottomToolbar.trailingButton;
      self.closeAllButton = self.bottomToolbar.leadingButton;
      self.newTabButton = self.bottomToolbar.centerButton;
      break;
    case TabGridConfigurationFloatingButton:
      self.topToolbar.leadingButton.hidden = NO;
      self.topToolbar.trailingButton.hidden = NO;
      self.bottomToolbar.hidden = YES;
      self.floatingButton.hidden = NO;
      self.doneButton = self.topToolbar.leadingButton;
      self.closeAllButton = self.topToolbar.trailingButton;
      self.newTabButton = self.floatingButton;
      break;
  }
  if (self.traitCollection.verticalSizeClass ==
          UIUserInterfaceSizeClassRegular &&
      self.traitCollection.horizontalSizeClass ==
          UIUserInterfaceSizeClassRegular) {
    // There is enough space for the tab view to have a tab strip,
    // so put the done button on the trailing side where the tab
    // switcher button is located on the tab view. This puts the buttons for
    // entering and leaving the tab grid on the same corner.
    self.doneButton = self.topToolbar.trailingButton;
    self.closeAllButton = self.topToolbar.leadingButton;
  }

  [self.doneButton setTitle:l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON)
                   forState:UIControlStateNormal];
  [self.closeAllButton
      setTitle:l10n_util::GetNSString(IDS_IOS_TAB_GRID_CLOSE_ALL_BUTTON)
      forState:UIControlStateNormal];
  self.doneButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  self.closeAllButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.doneButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.closeAllButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.doneButton.accessibilityIdentifier = kTabGridDoneButtonIdentifier;
  self.closeAllButton.accessibilityIdentifier =
      kTabGridCloseAllButtonIdentifier;
  [self.doneButton addTarget:self
                      action:@selector(doneButtonTapped:)
            forControlEvents:UIControlEventTouchUpInside];
  [self.closeAllButton addTarget:self
                          action:@selector(closeAllButtonTapped:)
                forControlEvents:UIControlEventTouchUpInside];
  [self.newTabButton addTarget:self
                        action:@selector(newTabButtonTapped:)
              forControlEvents:UIControlEventTouchUpInside];
  [self configureButtonsForOriginalAndCurrentPage];
}

- (void)configureButtonsForOriginalAndCurrentPage {
  self.newTabButton.page = self.currentPage;
  switch (self.originalPage) {
    case TabGridPageIncognitoTabs:
      self.doneButton.enabled = !self.incognitoTabsViewController.isGridEmpty;
      break;
    case TabGridPageRegularTabs:
      self.doneButton.enabled = !self.regularTabsViewController.isGridEmpty;
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is not possible to have entered tab grid directly "
                      "into remote tabs.";
      break;
  }
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      self.closeAllButton.enabled =
          !self.incognitoTabsViewController.isGridEmpty;
      break;
    case TabGridPageRegularTabs:
      self.closeAllButton.enabled = !self.regularTabsViewController.isGridEmpty;
      break;
    case TabGridPageRemoteTabs:
      self.closeAllButton.enabled = NO;
      break;
  }
}

// Translates the toolbar views offscreen and then animates them back in using
// the transition coordinator. Transitions are preferred here since they don't
// interact with the layout system at all.
- (void)animateToolbarsForAppearance {
  DCHECK(self.transitionCoordinator);
  // TODO(crbug.com/820410): Tune the timing of these animations.

  // Capture the current toolbar transforms.
  CGAffineTransform topToolbarBaseTransform = self.topToolbar.transform;
  CGAffineTransform bottomToolbarBaseTransform = self.bottomToolbar.transform;
  // Translate the top toolbar up offscreen by shifting it up by its height.
  self.topToolbar.transform = CGAffineTransformTranslate(
      self.topToolbar.transform, /*tx=*/0,
      /*ty=*/-(self.topToolbar.bounds.size.height * 0.5));
  // Translate the bottom toolbar down offscreen by shifting it down by its
  // height.
  self.bottomToolbar.transform = CGAffineTransformTranslate(
      self.bottomToolbar.transform, /*tx=*/0,
      /*ty=*/(self.topToolbar.bounds.size.height * 0.5));

  // Block that restores the toolbar transforms, suitable for using with the
  // transition coordinator.
  auto animation = ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    self.topToolbar.transform = topToolbarBaseTransform;
    self.bottomToolbar.transform = bottomToolbarBaseTransform;
  };

  // Also hide the scroll view (and thus the tab grids) until the transition
  // completes.
  self.scrollView.hidden = YES;
  auto cleanup = ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    self.scrollView.hidden = NO;
  };

  // Animate the toolbars into place alongside the current transition.
  [self.transitionCoordinator animateAlongsideTransition:animation
                                              completion:cleanup];
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(GridViewController*)gridViewController
      didSelectItemAtIndex:(NSUInteger)index {
  if (gridViewController == self.regularTabsViewController) {
    [self.regularTabsDelegate selectItemAtIndex:index];
  } else if (gridViewController == self.incognitoTabsViewController) {
    [self.incognitoTabsDelegate selectItemAtIndex:index];
  }
  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage];
}

- (void)gridViewController:(GridViewController*)gridViewController
       didCloseItemAtIndex:(NSUInteger)index {
  if (gridViewController == self.regularTabsViewController) {
    [self.regularTabsDelegate closeItemAtIndex:index];
  } else if (gridViewController == self.incognitoTabsViewController) {
    [self.incognitoTabsDelegate closeItemAtIndex:index];
  }
}

- (void)gridViewController:(GridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count {
  [self configureButtonsForOriginalAndCurrentPage];
  if (gridViewController == self.regularTabsViewController) {
    self.topToolbar.pageControl.regularTabCount = count;
  }
}

#pragma mark - Control actions

- (void)doneButtonTapped:(id)sender {
  [self.tabPresentationDelegate showActiveTabInPage:self.originalPage];
}

- (void)closeAllButtonTapped:(id)sender {
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      [self.incognitoTabsDelegate closeAllItems];
      break;
    case TabGridPageRegularTabs:
      [self.regularTabsDelegate closeAllItems];
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is invalid to call close all tabs on remote tabs.";
      break;
  }
}

- (void)newTabButtonTapped:(id)sender {
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      [self.incognitoTabsDelegate addNewItem];
      break;
    case TabGridPageRegularTabs:
      [self.regularTabsDelegate addNewItem];
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is invalid to call insert new tab on remote tabs.";
      break;
  }
  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage];
}

- (void)pageControlChanged:(id)sender {
  self.currentPage = self.topToolbar.pageControl.selectedPage;
}

@end
