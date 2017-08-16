// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/google_landing_view_controller.h"

#include <algorithm>

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/start_voice_search_command.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"
#import "ios/chrome/browser/ui/ntp/google_landing_data_source.h"
#import "ios/chrome/browser/ui/ntp/most_visited_cell.h"
#import "ios/chrome/browser/ui/ntp/most_visited_layout.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view.h"
#import "ios/chrome/browser/ui/ntp/whats_new_header_view.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {

enum {
  SectionWithOmnibox,
  SectionWithMostVisited,
  NumberOfCollectionViewSections,
};

const UIEdgeInsets kSearchBoxStretchInsets = {3, 3, 3, 3};

const CGFloat kHintLabelSidePadding = 12;
const CGFloat kWhatsNewHeaderHiddenHeight = 8;
const NSInteger kMaxNumMostVisitedFaviconRows = 2;
const CGFloat kShiftTilesDownAnimationDuration = 0.2;

}  // namespace

@interface GoogleLandingViewController ()<OverscrollActionsControllerDelegate,
                                          UICollectionViewDataSource,
                                          UICollectionViewDelegate,
                                          UICollectionViewDelegateFlowLayout,
                                          UIGestureRecognizerDelegate,
                                          WhatsNewHeaderViewDelegate> {
  // Fake omnibox.
  UIButton* _searchTapTarget;

  // A collection view for the most visited sites.
  UICollectionView* _mostVisitedView;

  // The overscroll actions controller managing accelerators over the toolbar.
  OverscrollActionsController* _overscrollActionsController;

  // |YES| when notifications indicate the omnibox is focused.
  BOOL _omniboxFocused;

  // Tap and swipe gesture recognizers when the omnibox is focused.
  UITapGestureRecognizer* _tapGestureRecognizer;
  UISwipeGestureRecognizer* _swipeGestureRecognizer;

  // Handles displaying the context menu for all form factors.
  ContextMenuCoordinator* _contextMenuCoordinator;

  // URL of the last deleted most viewed entry. If present the UI to restore it
  // is shown.
  NSURL* _deletedUrl;

  // |YES| if the view has finished its first layout. This is useful when
  // determining if the view has sized itself for tablet.
  BOOL _viewLoaded;

  // |YES| if the fakebox header should be animated on scroll.
  BOOL _animateHeader;

  // |YES| if the collection scrollView is scrolled all the way to the top. Used
  // to lock this position in place on various frame changes.
  BOOL _scrolledToTop;

  // |YES| if this NTP panel is visible.  When set to |NO| various UI updates
  // are ignored.
  BOOL _isShowing;

  CFTimeInterval _shiftTilesDownStartTime;
  CGSize _mostVisitedCellSize;
  NSLayoutConstraint* _hintLabelLeadingConstraint;
  NSLayoutConstraint* _voiceTapTrailingConstraint;
  NSLayoutConstraint* _doodleHeightConstraint;
  NSLayoutConstraint* _doodleTopMarginConstraint;
  NSLayoutConstraint* _searchFieldWidthConstraint;
  NSLayoutConstraint* _searchFieldHeightConstraint;
  NSLayoutConstraint* _searchFieldTopMarginConstraint;
  NewTabPageHeaderView* _headerView;
  WhatsNewHeaderView* _promoHeaderView;
  __weak id<GoogleLandingDataSource> _dataSource;
  __weak id<ApplicationCommands, BrowserCommands, UrlLoader, OmniboxFocuser>
      _dispatcher;
}

// Whether the Google logo or doodle is being shown.
@property(nonatomic, assign) BOOL logoIsShowing;

// Exposes view and methods to drive the doodle.
@property(nonatomic, weak) id<LogoVendor> logoVendor;

// |YES| if this consumer is has voice search enabled.
@property(nonatomic, assign) BOOL voiceSearchIsEnabled;

// Gets the maximum number of sites shown.
@property(nonatomic, assign) NSUInteger maximumMostVisitedSitesShown;

// Gets the text of a what's new promo.
@property(nonatomic, strong) NSString* promoText;

// Gets the icon of a what's new promo.
// TODO(crbug.com/694750): This should not be WhatsNewIcon.
@property(nonatomic, assign) WhatsNewIcon promoIcon;

// |YES| if a what's new promo can be displayed.
@property(nonatomic, assign) BOOL promoCanShow;

// The number of tabs to show in the google landing fake toolbar.
@property(nonatomic, assign) int tabCount;

// |YES| if the google landing toolbar can show the forward arrow, cached and
// pushed into the header view.
@property(nonatomic, assign) BOOL canGoForward;

// |YES| if the google landing toolbar can show the back arrow, cached and
// pushed into the header view.
@property(nonatomic, assign) BOOL canGoBack;

// Left margin to center the items. Used for the inset.
@property(nonatomic, assign) CGFloat leftMargin;

// Returns the height to use for the What's New promo view.
- (CGFloat)promoHeaderHeight;
// Add fake search field and voice search microphone.
- (void)addSearchField;
// Add most visited collection view.
- (void)addMostVisited;
// Update the iPhone fakebox's frame based on the current scroll view offset.
- (void)updateSearchField;
// Scrolls  most visited to the top of the view when the omnibox is focused.
- (void)locationBarBecomesFirstResponder;
// Scroll the view back to 0,0 when the omnibox loses focus.
- (void)locationBarResignsFirstResponder;
// When the search field is tapped.
- (void)searchFieldTapped:(id)sender;
// Tells WebToolbarController to resign focus to the omnibox.
- (void)blurOmnibox;
// Called when a user does a long press on a most visited item.
- (void)handleMostVisitedLongPress:
    (UILongPressGestureRecognizer*)longPressGesture;
// When the user removes a most visited a bubble pops up to undo the action.
- (void)showMostVisitedUndoForURL:(NSURL*)url;
// If Google is not the default search engine, hide the logo, doodle and
// fakebox.
- (void)updateLogoAndFakeboxDisplay;
// Instructs the UICollectionView and UIView to reload it's data and layout.
- (void)reloadData;
// Adds the constraints for the |logoView|, the |searchField| related to the
// |headerView|. It also creates the ivar constraints related to those views.
- (void)addConstraintsForLogoView:(UIView*)logoView
                      searchField:(UIView*)searchField
                    andHeaderView:(UIView*)headerView;
// Updates the constraints of the headers to fit |width|.
- (void)updateConstraintsForWidth:(CGFloat)width;
// Returns the size of |self.mostVisitedData|.
- (NSUInteger)numberOfItems;
// Returns the number of non empty tiles (as opposed to the placeholder tiles).
- (NSInteger)numberOfNonEmptyTilesShown;
// Returns the URL for the mosted visited item in |self.mostVisitedData|.
- (GURL)urlForIndex:(NSUInteger)index;
// Returns the nearest ancestor view that is kind of |aClass|.
- (UIView*)nearestAncestorOfView:(UIView*)view withClass:(Class)aClass;
// Updates the collection view's scroll view offset for the next frame of the
// shiftTilesDown animation.
- (void)shiftTilesDownAnimationDidFire:(CADisplayLink*)link;
// Returns the size to use for Most Visited cells in the NTP.
- (CGSize)mostVisitedCellSize;

@end

@implementation GoogleLandingViewController

@synthesize dispatcher = _dispatcher;
@synthesize logoVendor = _logoVendor;
// Property declared in NewTabPagePanelProtocol.
@synthesize delegate = _delegate;
@synthesize logoIsShowing = _logoIsShowing;
@synthesize promoText = _promoText;
@synthesize promoIcon = _promoIcon;
@synthesize promoCanShow = _promoCanShow;
@synthesize maximumMostVisitedSitesShown = _maximumMostVisitedSitesShown;
@synthesize tabCount = _tabCount;
@synthesize canGoForward = _canGoForward;
@synthesize canGoBack = _canGoBack;
@synthesize voiceSearchIsEnabled = _voiceSearchIsEnabled;
@synthesize leftMargin = _leftMargin;

- (void)viewDidLoad {
  [super viewDidLoad];
  [self.view setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                 UIViewAutoresizingFlexibleWidth];

  // Initialise |shiftTilesDownStartTime| to a sentinel value to indicate that
  // the animation has not yet started.
  _shiftTilesDownStartTime = -1;
  _mostVisitedCellSize = [self mostVisitedCellSize];
  _isShowing = YES;
  _scrolledToTop = NO;
  _animateHeader = YES;

  _tapGestureRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(blurOmnibox)];
  [_tapGestureRecognizer setDelegate:self];
  _swipeGestureRecognizer =
      [[UISwipeGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(blurOmnibox)];
  [_swipeGestureRecognizer setDirection:UISwipeGestureRecognizerDirectionDown];

  self.leftMargin =
      content_suggestions::centeredTilesMarginForWidth([self viewWidth]);

  [self addSearchField];
  [self addMostVisited];
  [self addOverscrollActions];
  [self reload];
  _viewLoaded = YES;
  [self.logoVendor fetchDoodle];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  self.leftMargin =
      content_suggestions::centeredTilesMarginForWidth(size.width);

  // Reload the data to have the right number of items for the new orientation.
  [self reloadData];

  void (^alongsideBlock)(id<UIViewControllerTransitionCoordinatorContext>) = ^(
      id<UIViewControllerTransitionCoordinatorContext> context) {
    if (IsIPadIdiom() && _scrolledToTop) {
      // Keep the most visited thumbnails scrolled to the top.
      [_mostVisitedView setContentOffset:CGPointMake(0, [self pinnedOffsetY])];
      return;
    };

    // Call -scrollViewDidScroll: so that the omnibox's frame is adjusted for
    // the scroll view's offset.
    [self scrollViewDidScroll:_mostVisitedView];

    // Updates the constraints.
    [self updateConstraintsForWidth:size.width];
    BOOL isScrollableNTP = !IsIPadIdiom() || IsCompactTablet();
    if (isScrollableNTP && _scrolledToTop) {
      // Set the scroll view's offset to the pinned offset to keep the omnibox
      // at the top of the screen if it isn't already.
      CGFloat pinnedOffsetY = [self pinnedOffsetY];
      if ([_mostVisitedView contentOffset].y < pinnedOffsetY) {
        [_mostVisitedView setContentOffset:CGPointMake(0, pinnedOffsetY)];
      } else {
        [self updateSearchField];
      }
    }

  };
  [coordinator animateAlongsideTransition:alongsideBlock completion:nil];
}

- (void)viewDidLayoutSubviews {
  self.leftMargin =
      content_suggestions::centeredTilesMarginForWidth([self viewWidth]);
  [self updateConstraintsForWidth:[self viewWidth]];
  [self updateSearchField];
  // Invalidate layout to handle the cases where the layout is changed when the
  // NTP is not presented (e.g. tab backgrounded).
  [[_mostVisitedView collectionViewLayout] invalidateLayout];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [_mostVisitedView setDelegate:nil];
  [_mostVisitedView setDataSource:nil];
  [_overscrollActionsController invalidate];
}

#pragma mark - Properties

- (id<GoogleLandingDataSource>)dataSource {
  return _dataSource;
}

- (void)setDataSource:(id<GoogleLandingDataSource>)dataSource {
  _dataSource = dataSource;
}

#pragma mark - Private

- (CGSize)mostVisitedCellSize {
  if (IsIPadIdiom()) {
    // On iPads, split-screen and slide-over may require showing smaller cells.
    CGSize maximumCellSize = [MostVisitedCell maximumSize];
    CGSize viewSize = self.view.bounds.size;
    CGFloat smallestDimension =
        viewSize.height > viewSize.width ? viewSize.width : viewSize.height;
    CGFloat cellWidth = AlignValueToPixel(
        (smallestDimension - 3 * content_suggestions::spacingBetweenTiles()) /
        2);
    if (cellWidth < maximumCellSize.width) {
      return CGSizeMake(cellWidth, cellWidth);
    } else {
      return maximumCellSize;
    }
  } else {
    return [MostVisitedCell maximumSize];
  }
}

- (CGFloat)viewWidth {
  return [self.view frame].size.width;
}

- (int)numberOfColumns {
  CGFloat width = [self viewWidth];

  NSUInteger columns = content_suggestions::numberOfTilesForWidth(
      width - 2 * content_suggestions::spacingBetweenTiles());
  DCHECK(columns > 1);
  return columns;
}

- (CGFloat)promoHeaderHeight {
  CGFloat promoMaxWidth =
      [self viewWidth] -
      2 * content_suggestions::centeredTilesMarginForWidth([self viewWidth]);
  NSString* text = self.promoText;
  return [WhatsNewHeaderView heightToFitText:text inWidth:promoMaxWidth];
}

- (void)updateLogoAndFakeboxDisplay {
  if (self.logoVendor.showingLogo != self.logoIsShowing) {
    self.logoVendor.showingLogo = self.logoIsShowing;
    if (_viewLoaded) {
      [_doodleHeightConstraint
          setConstant:content_suggestions::doodleHeight(self.logoIsShowing)];
    }
    if (IsIPadIdiom())
      [_searchTapTarget setHidden:!self.logoIsShowing];
    [[_mostVisitedView collectionViewLayout] invalidateLayout];
  }
}

// Initialize and add a search field tap target and a voice search button.
- (void)addSearchField {
  _searchTapTarget = [[UIButton alloc] init];
  if (IsIPadIdiom()) {
    UIImage* searchBoxImage = [[UIImage imageNamed:@"ntp_google_search_box"]
        resizableImageWithCapInsets:kSearchBoxStretchInsets];
    [_searchTapTarget setBackgroundImage:searchBoxImage
                                forState:UIControlStateNormal];
  }
  [_searchTapTarget setAdjustsImageWhenHighlighted:NO];
  [_searchTapTarget addTarget:self
                       action:@selector(searchFieldTapped:)
             forControlEvents:UIControlEventTouchUpInside];
  [_searchTapTarget
      setAccessibilityLabel:l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)];
  // Set isAccessibilityElement to NO so that Voice Search button is accessible.
  [_searchTapTarget setIsAccessibilityElement:NO];

  // Set up fakebox hint label.
  UILabel* searchHintLabel = [[UILabel alloc] init];
  content_suggestions::configureSearchHintLabel(searchHintLabel,
                                                _searchTapTarget);

  _hintLabelLeadingConstraint = [searchHintLabel.leadingAnchor
      constraintEqualToAnchor:[_searchTapTarget leadingAnchor]
                     constant:kHintLabelSidePadding];
  [_hintLabelLeadingConstraint setActive:YES];

  // Add a voice search button.
  UIButton* voiceTapTarget = [[UIButton alloc] init];
  content_suggestions::configureVoiceSearchButton(voiceTapTarget,
                                                  _searchTapTarget);

  _voiceTapTrailingConstraint = [voiceTapTarget.trailingAnchor
      constraintEqualToAnchor:[_searchTapTarget trailingAnchor]];
  [NSLayoutConstraint activateConstraints:@[
    [searchHintLabel.trailingAnchor
        constraintEqualToAnchor:voiceTapTarget.leadingAnchor],
    _voiceTapTrailingConstraint
  ]];

  if (self.voiceSearchIsEnabled) {
    [voiceTapTarget addTarget:self
                       action:@selector(loadVoiceSearch:)
             forControlEvents:UIControlEventTouchUpInside];
    [voiceTapTarget addTarget:self
                       action:@selector(preloadVoiceSearch:)
             forControlEvents:UIControlEventTouchDown];
  } else {
    [voiceTapTarget setEnabled:NO];
  }
}

- (void)loadVoiceSearch:(id)sender {
  DCHECK(self.voiceSearchIsEnabled);
  base::RecordAction(UserMetricsAction("MobileNTPMostVisitedVoiceSearch"));
  UIView* view = base::mac::ObjCCastStrict<UIView>(sender);
  StartVoiceSearchCommand* command =
      [[StartVoiceSearchCommand alloc] initWithOriginView:view];
  [self.dispatcher startVoiceSearch:command];
}

- (void)preloadVoiceSearch:(id)sender {
  DCHECK(self.voiceSearchIsEnabled);
  [sender removeTarget:self
                action:@selector(preloadVoiceSearch:)
      forControlEvents:UIControlEventTouchDown];

  [self.dispatcher preloadVoiceSearch];
}

// Initialize and add a panel with most visited sites.
- (void)addMostVisited {
  CGRect mostVisitedFrame = [self.view bounds];
  UICollectionViewFlowLayout* flowLayout;
  if (IsIPadIdiom())
    flowLayout = [[UICollectionViewFlowLayout alloc] init];
  else
    flowLayout = [[MostVisitedLayout alloc] init];

  [flowLayout setScrollDirection:UICollectionViewScrollDirectionVertical];
  [flowLayout setItemSize:_mostVisitedCellSize];
  [flowLayout setMinimumInteritemSpacing:8];
  [flowLayout setMinimumLineSpacing:content_suggestions::spacingBetweenTiles()];
  DCHECK(!_mostVisitedView);
  _mostVisitedView = [[UICollectionView alloc] initWithFrame:mostVisitedFrame
                                        collectionViewLayout:flowLayout];
  [_mostVisitedView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                        UIViewAutoresizingFlexibleWidth];
  [_mostVisitedView setDelegate:self];
  [_mostVisitedView setDataSource:self];
  [_mostVisitedView registerClass:[MostVisitedCell class]
       forCellWithReuseIdentifier:@"classCell"];
  [_mostVisitedView setBackgroundColor:[UIColor clearColor]];
  [_mostVisitedView setBounces:YES];
  [_mostVisitedView setShowsHorizontalScrollIndicator:NO];
  [_mostVisitedView setShowsVerticalScrollIndicator:NO];
  [_mostVisitedView registerClass:[UICollectionReusableView class]
       forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
              withReuseIdentifier:@"header"];
  [_mostVisitedView setAccessibilityIdentifier:@"Google Landing"];

  [self.view addSubview:_mostVisitedView];
}

- (void)updateSearchField {
  NSArray* constraints =
      @[ _hintLabelLeadingConstraint, _voiceTapTrailingConstraint ];

  [_headerView updateSearchFieldWidth:_searchFieldWidthConstraint
                               height:_searchFieldHeightConstraint
                            topMargin:_searchFieldTopMarginConstraint
                   subviewConstraints:constraints
                        logoIsShowing:self.logoIsShowing
                            forOffset:[_mostVisitedView contentOffset].y
                                width:0];
}

- (void)addOverscrollActions {
  if (!IsIPadIdiom()) {
    _overscrollActionsController = [[OverscrollActionsController alloc]
        initWithScrollView:_mostVisitedView];
    [_overscrollActionsController setStyle:OverscrollStyle::NTP_NON_INCOGNITO];
    [_overscrollActionsController setDelegate:self];
  }
}

// Check to see if the promo label should be hidden.
- (void)hideWhatsNewIfNecessary {
  if (![_promoHeaderView isHidden] && !self.promoCanShow) {
    [_promoHeaderView setHidden:YES];
    [self.view setNeedsLayout];
  }
}

- (void)locationBarBecomesFirstResponder {
  if (!_isShowing)
    return;

  _omniboxFocused = YES;
  [self shiftTilesUp];
}

- (void)shiftTilesUp {
  _scrolledToTop = YES;
  // Add gesture recognizer to background |self.view| when omnibox is focused.
  [self.view addGestureRecognizer:_tapGestureRecognizer];
  [self.view addGestureRecognizer:_swipeGestureRecognizer];

  CGFloat pinnedOffsetY = [self pinnedOffsetY];
  _animateHeader = !IsIPadIdiom();

  [UIView animateWithDuration:0.25
      animations:^{
        if ([_mostVisitedView contentOffset].y < pinnedOffsetY) {
          [_mostVisitedView setContentOffset:CGPointMake(0, pinnedOffsetY)];
          [[_mostVisitedView collectionViewLayout] invalidateLayout];
        }
      }
      completion:^(BOOL finished) {
        // Check to see if we are still scrolled to the top -- it's possible
        // (and difficult) to resign the first responder and initiate a
        // -shiftTilesDown before the animation here completes.
        if (_scrolledToTop) {
          _animateHeader = NO;
          if (!IsIPadIdiom()) {
            [self.dispatcher onFakeboxAnimationComplete];
            [_headerView fadeOutShadow];
            [_searchTapTarget setHidden:YES];
          }
        }
      }];
}

- (void)searchFieldTapped:(id)sender {
  [self.dispatcher focusFakebox];
}

- (void)blurOmnibox {
  if (_omniboxFocused) {
    [self.dispatcher cancelOmniboxEdit];
  } else {
    [self locationBarResignsFirstResponder];
  }
}

- (void)locationBarResignsFirstResponder {
  if (!_isShowing && !_scrolledToTop)
    return;

  _omniboxFocused = NO;
  if ([_contextMenuCoordinator isVisible]) {
    return;
  }

  [self shiftTilesDown];
}

- (void)shiftTilesDown {
  _animateHeader = YES;
  _scrolledToTop = NO;
  if (!IsIPadIdiom()) {
    [_searchTapTarget setHidden:NO];
    [self.dispatcher onFakeboxBlur];
  }

  // Reload most visited sites in case the number of placeholder cells needs to
  // be updated after an orientation change.
  [_mostVisitedView reloadData];

  // Reshow views that are within range of the most visited collection view
  // (if necessary).
  [self.view removeGestureRecognizer:_tapGestureRecognizer];
  [self.view removeGestureRecognizer:_swipeGestureRecognizer];

  // CADisplayLink is used for this animation instead of the standard UIView
  // animation because the standard animation did not properly convert the
  // fakebox from its scrolled up mode to its scrolled down mode. Specifically,
  // calling |UICollectionView reloadData| adjacent to the standard animation
  // caused the fakebox's views to jump incorrectly. CADisplayLink avoids this
  // problem because it allows |shiftTilesDownAnimationDidFire| to directly
  // control each frame.
  CADisplayLink* link = [CADisplayLink
      displayLinkWithTarget:self
                   selector:@selector(shiftTilesDownAnimationDidFire:)];
  [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];

  // Dismisses modal UI elements if displayed. Must be called at the end of
  // -locationBarResignsFirstResponder since it could result in -dealloc being
  // called.
  [self dismissModals];
}

- (void)shiftTilesDownAnimationDidFire:(CADisplayLink*)link {
  // If this is the first frame of the animation, store the starting timestamp
  // and do nothing.
  if (_shiftTilesDownStartTime == -1) {
    _shiftTilesDownStartTime = link.timestamp;
    return;
  }

  CFTimeInterval timeElapsed = link.timestamp - _shiftTilesDownStartTime;
  double percentComplete = timeElapsed / kShiftTilesDownAnimationDuration;
  // Ensure that the percentage cannot be above 1.0.
  if (percentComplete > 1.0)
    percentComplete = 1.0;

  // Find how much the collection view should be scrolled up in the next frame.
  CGFloat yOffset = (1.0 - percentComplete) * [self pinnedOffsetY];
  [_mostVisitedView setContentOffset:CGPointMake(0, yOffset)];

  if (percentComplete == 1.0) {
    [link invalidate];
    // Reset |shiftTilesDownStartTime to its sentinal value.
    _shiftTilesDownStartTime = -1;
    [[_mostVisitedView collectionViewLayout] invalidateLayout];
  }
}

- (void)reloadData {
  // -reloadData updates from |self.mostVisitedData|.
  // -invalidateLayout is necessary because sometimes the flowLayout has the
  // wrong cached size and will throw an internal exception if the
  // -numberOfItems shrinks. -setNeedsLayout is needed in case
  // -numberOfItems increases enough to add a new row and change the height
  // of _mostVisitedView.
  [_mostVisitedView reloadData];
  [[_mostVisitedView collectionViewLayout] invalidateLayout];
  [self.view setNeedsLayout];
}

- (void)addConstraintsForLogoView:(UIView*)logoView
                      searchField:(UIView*)searchField
                    andHeaderView:(UIView*)headerView {
  _doodleTopMarginConstraint = [logoView.topAnchor
      constraintEqualToAnchor:headerView.topAnchor
                     constant:content_suggestions::doodleTopMargin(YES)];
  _doodleHeightConstraint = [logoView.heightAnchor
      constraintEqualToConstant:content_suggestions::doodleHeight(
                                    self.logoIsShowing)];
  _searchFieldWidthConstraint = [searchField.widthAnchor
      constraintEqualToConstant:content_suggestions::searchFieldWidth(
                                    [self viewWidth])];
  _searchFieldHeightConstraint = [searchField.heightAnchor
      constraintEqualToConstant:content_suggestions::kSearchFieldHeight];
  _searchFieldTopMarginConstraint = [searchField.topAnchor
      constraintEqualToAnchor:logoView.bottomAnchor
                     constant:content_suggestions::searchFieldTopMargin()];
  [NSLayoutConstraint activateConstraints:@[
    _doodleTopMarginConstraint,
    _doodleHeightConstraint,
    _searchFieldWidthConstraint,
    _searchFieldHeightConstraint,
    _searchFieldTopMarginConstraint,
    [logoView.widthAnchor constraintEqualToAnchor:headerView.widthAnchor],
    [logoView.leadingAnchor constraintEqualToAnchor:headerView.leadingAnchor],
    [searchField.centerXAnchor
        constraintEqualToAnchor:headerView.centerXAnchor],
  ]];
}

- (void)updateConstraintsForWidth:(CGFloat)width {
  [_promoHeaderView
      setSideMargin:content_suggestions::centeredTilesMarginForWidth(width)
           forWidth:width];
  [_doodleTopMarginConstraint
      setConstant:content_suggestions::doodleTopMargin(YES)];
  [_searchFieldWidthConstraint
      setConstant:content_suggestions::searchFieldWidth(width)];
  [_searchFieldTopMarginConstraint
      setConstant:content_suggestions::searchFieldTopMargin()];
}

#pragma mark - ToolbarOwner

- (ToolbarController*)relinquishedToolbarController {
  return [_headerView relinquishedToolbarController];
}

- (void)reparentToolbarController {
  [_headerView reparentToolbarController];
}

#pragma mark - UICollectionView Methods.

- (UIEdgeInsets)collectionView:(UICollectionView*)collectionView
                        layout:(UICollectionViewLayout*)collectionViewLayout
        insetForSectionAtIndex:(NSInteger)section {
  return UIEdgeInsetsMake(0, self.leftMargin, 0, self.leftMargin);
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  CGFloat headerHeight = 0;
  if (section == SectionWithOmnibox) {
    headerHeight = content_suggestions::heightForLogoHeader(
        self.logoIsShowing, self.promoCanShow, YES);
    ((UICollectionViewFlowLayout*)collectionViewLayout).headerReferenceSize =
        CGSizeMake(0, headerHeight);
  } else if (section == SectionWithMostVisited) {
    if (self.promoCanShow) {
      headerHeight = [self promoHeaderHeight];
    } else {
      headerHeight = kWhatsNewHeaderHiddenHeight;
    }
  }
  return CGSizeMake(0, headerHeight);
}

#pragma mark - UICollectionViewDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  return indexPath.row < static_cast<NSInteger>([self numberOfItems]);
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  MostVisitedCell* cell =
      (MostVisitedCell*)[collectionView cellForItemAtIndexPath:indexPath];

  // Keep the UICollectionView alive for one second while the screen
  // reader does its thing.
  // TODO(jif): This needs a radar, since it is almost certainly a
  // UIKit accessibility bug. crbug.com/529271
  if (UIAccessibilityIsVoiceOverRunning()) {
    __block UICollectionView* blockView = _mostVisitedView;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 static_cast<int64_t>(1 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     blockView = nil;
                   });
  }

  const NSUInteger visitedIndex = indexPath.row;
  [self blurOmnibox];
  DCHECK(visitedIndex < [self numberOfItems]);
  [self.dataSource logMostVisitedClick:visitedIndex tileType:cell.tileType];
  [self.dispatcher loadURL:[self urlForIndex:visitedIndex]
                  referrer:web::Referrer()
                transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
         rendererInitiated:NO];
}

#pragma mark - UICollectionViewDataSource

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  DCHECK(kind == UICollectionElementKindSectionHeader);

  if (indexPath.section == SectionWithOmnibox) {
    UICollectionReusableView* reusableView =
        [collectionView dequeueReusableSupplementaryViewOfKind:
                            UICollectionElementKindSectionHeader
                                           withReuseIdentifier:@"header"
                                                  forIndexPath:indexPath];
    if (!_headerView) {
      _headerView = [[NewTabPageHeaderView alloc] init];
      [_headerView addSubview:[self.logoVendor view]];
      [_headerView addSubview:_searchTapTarget];
      self.logoVendor.view.translatesAutoresizingMaskIntoConstraints = NO;
      [_searchTapTarget setTranslatesAutoresizingMaskIntoConstraints:NO];

      [self addConstraintsForLogoView:self.logoVendor.view
                          searchField:_searchTapTarget
                        andHeaderView:_headerView];

      [_headerView addViewsToSearchField:_searchTapTarget];

      if (!IsIPadIdiom()) {
        // iPhone header also contains a toolbar since the normal toolbar is
        // hidden.
        [_headerView
            addToolbarWithReadingListModel:[self.dataSource readingListModel]
                                dispatcher:self.dispatcher];
        [_headerView setToolbarTabCount:self.tabCount];
        [_headerView setCanGoForward:self.canGoForward];
        [_headerView setCanGoBack:self.canGoBack];
      }
      [_headerView setTranslatesAutoresizingMaskIntoConstraints:NO];
    }
    [reusableView addSubview:_headerView];
    AddSameConstraints(reusableView, _headerView);

    return reusableView;
  }

  if (indexPath.section == SectionWithMostVisited) {
    UICollectionReusableView* reusableView =
        [collectionView dequeueReusableSupplementaryViewOfKind:
                            UICollectionElementKindSectionHeader
                                           withReuseIdentifier:@"header"
                                                  forIndexPath:indexPath];
    if (!_promoHeaderView) {
      _promoHeaderView = [[WhatsNewHeaderView alloc] init];
      [_promoHeaderView
          setSideMargin:content_suggestions::centeredTilesMarginForWidth(
                            [self viewWidth])
               forWidth:[self viewWidth]];
      [_promoHeaderView setDelegate:self];
      if (self.promoCanShow) {
        [_promoHeaderView setText:self.promoText];
        [_promoHeaderView setIcon:self.promoIcon];
        [self.dataSource promoViewed];
      }
    }
    [reusableView addSubview:_promoHeaderView];
    AddSameConstraints(reusableView, _promoHeaderView);
    return reusableView;
  }

  NOTREACHED();
  return nil;
}

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return NumberOfCollectionViewSections;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  // The first section only contains a header view and no items.
  if (section == SectionWithOmnibox)
    return 0;

  // Phone always contains the maximum number of cells. Cells in excess of the
  // number of thumbnails are used solely for layout/sizing.
  if (!IsIPadIdiom())
    return self.maximumMostVisitedSitesShown;

  return [self numberOfNonEmptyTilesShown];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  MostVisitedCell* cell = (MostVisitedCell*)[collectionView
      dequeueReusableCellWithReuseIdentifier:@"classCell"
                                forIndexPath:indexPath];
  BOOL isPlaceholder = indexPath.row >= (int)[self numberOfItems];
  if (isPlaceholder) {
    [cell showPlaceholder];
    for (UIGestureRecognizer* ges in cell.gestureRecognizers) {
      [cell removeGestureRecognizer:ges];
    }

    // When -numberOfItems is 0, always remove the placeholder.
    if (indexPath.row >= [self numberOfColumns] || [self numberOfItems] == 0) {
      // This cell is completely empty and only exists for layout/sizing
      // purposes.
      [cell removePlaceholderImage];
    }
    return cell;
  }

  const ntp_tiles::NTPTile& ntpTile =
      [self.dataSource mostVisitedAtIndex:indexPath.row];
  NSString* title = base::SysUTF16ToNSString(ntpTile.title);

  [cell setupWithURL:ntpTile.url title:title dataSource:self.dataSource];

  UILongPressGestureRecognizer* longPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleMostVisitedLongPress:)];
  [cell addGestureRecognizer:longPress];

  return cell;
}

#pragma mark - Context Menu

// Called when a user does a long press on a most visited item.
- (void)handleMostVisitedLongPress:(UILongPressGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateBegan) {
    // Only one long press at a time.
    if ([_contextMenuCoordinator isVisible]) {
      return;
    }

    NSIndexPath* indexPath = [_mostVisitedView
        indexPathForCell:static_cast<UICollectionViewCell*>(sender.view)];
    const NSUInteger index = indexPath.row;

    // A long press occurred on one of the most visited button. Popup a context
    // menu.
    DCHECK(index < [self numberOfItems]);

    web::ContextMenuParams params;
    // Get view coordinates in local space.
    params.location = [sender locationInView:self.view];
    params.view.reset(self.view);

    // Present sheet/popover using controller that is added to view hierarchy.
    UIViewController* topController = [params.view window].rootViewController;
    while (topController.presentedViewController)
      topController = topController.presentedViewController;

    _contextMenuCoordinator =
        [[ContextMenuCoordinator alloc] initWithBaseViewController:topController
                                                            params:params];

    ProceduralBlock action;

    // Open In New Tab.
    GURL url = [self urlForIndex:index];
    __weak GoogleLandingViewController* weakSelf = self;
    action = ^{
      GoogleLandingViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      MostVisitedCell* cell = (MostVisitedCell*)sender.view;
      [[strongSelf dataSource] logMostVisitedClick:index
                                          tileType:cell.tileType];
      // GoogleLandingViewController is only displayed in non-incognito tabs,
      // so |inIncognito| can be assumed to be NO. If it were displayed in an
      // incognito state, then passing NO to |inIncognito| would open a tab in
      // the wrong browser state.
      [[strongSelf dispatcher] webPageOrderedOpen:url
                                         referrer:web::Referrer()
                                      inIncognito:NO
                                     inBackground:YES
                                         appendTo:kCurrentTab];
    };
    [_contextMenuCoordinator
        addItemWithTitle:l10n_util::GetNSStringWithFixup(
                             IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                  action:action];

    // Open in Incognito Tab.
    action = ^{
      GoogleLandingViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;
      MostVisitedCell* cell = (MostVisitedCell*)sender.view;
      [[strongSelf dataSource] logMostVisitedClick:index
                                          tileType:cell.tileType];
      [[strongSelf dispatcher] webPageOrderedOpen:url
                                         referrer:web::Referrer()
                                      inIncognito:YES
                                     inBackground:NO
                                         appendTo:kCurrentTab];
    };
    [_contextMenuCoordinator
        addItemWithTitle:l10n_util::GetNSStringWithFixup(
                             IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                  action:action];

    // Remove the most visited url.
    NSString* title =
        l10n_util::GetNSStringWithFixup(IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK);
    action = ^{
      GoogleLandingViewController* strongSelf = weakSelf;
      // Early return if the controller has been deallocated.
      if (!strongSelf)
        return;
      base::RecordAction(UserMetricsAction("MostVisited_UrlBlacklisted"));
      [[strongSelf dataSource] addBlacklistedURL:url];
      [strongSelf showMostVisitedUndoForURL:net::NSURLWithGURL(url)];
    };
    [_contextMenuCoordinator addItemWithTitle:title action:action];

    [_contextMenuCoordinator start];

    if (IsIPadIdiom())
      [self blurOmnibox];
  }
}

- (void)showMostVisitedUndoForURL:(NSURL*)url {
  _deletedUrl = url;

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  __weak GoogleLandingViewController* weakSelf = self;
  action.handler = ^{
    GoogleLandingViewController* strongSelf = weakSelf;
    if (!strongSelf)
      return;
    [[strongSelf dataSource]
        removeBlacklistedURL:net::GURLWithNSURL(_deletedUrl)];
  };
  action.title = l10n_util::GetNSString(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE);
  action.accessibilityIdentifier = @"Undo";

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage
      messageWithText:l10n_util::GetNSString(
                          IDS_IOS_NEW_TAB_MOST_VISITED_ITEM_REMOVED)];
  message.action = action;
  message.category = @"MostVisitedUndo";
  [MDCSnackbarManager showMessage:message];
}

- (void)onPromoLabelTapped {
  [self.dispatcher cancelOmniboxEdit];
  [_promoHeaderView setHidden:YES];
  [self.view setNeedsLayout];
  [self.dataSource promoTapped];
}

// Returns the Y value to use for the scroll view's contentOffset when scrolling
// the omnibox to the top of the screen.
- (CGFloat)pinnedOffsetY {
  CGFloat headerHeight = content_suggestions::heightForLogoHeader(
      self.logoIsShowing, self.promoCanShow, YES);
  CGFloat offsetY =
      headerHeight - ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsIPadIdiom())
    offsetY -= ntp_header::kToolbarHeight;

  return offsetY;
}

#pragma mark - NewTabPagePanelProtocol

- (void)reload {
  // Fetch the doodle after the view finishes laying out. Otherwise, tablet
  // may fetch the wrong sized doodle.
  if (_viewLoaded)
    [self.logoVendor fetchDoodle];
  [self updateLogoAndFakeboxDisplay];
  [self hideWhatsNewIfNecessary];
}

- (void)wasShown {
  _isShowing = YES;
  [_headerView hideToolbarViewsForNewTabPage];
  // The view is not loaded with the width it is displayed with. Reloading the
  // data after being displayed ensure that we got the right number of items.
  [self reloadData];
}

- (void)wasHidden {
  _isShowing = NO;
}

- (void)dismissModals {
  [_contextMenuCoordinator stop];
}

- (void)dismissKeyboard {
}

- (void)setScrollsToTop:(BOOL)enable {
}

- (CGFloat)alphaForBottomShadow {
  // Get the frame of the bottommost cell in |self.view|'s coordinate system.
  NSInteger section = SectionWithMostVisited;
  // Account for the fact that the tableview may not yet contain
  // |numberOfNonEmptyTilesShown| tiles because it hasn't been updated yet.
  NSUInteger lastItemIndex =
      std::min([_mostVisitedView numberOfItemsInSection:SectionWithMostVisited],
               [self numberOfNonEmptyTilesShown]) -
      1;
  DCHECK(lastItemIndex >= 0);
  NSIndexPath* lastCellIndexPath =
      [NSIndexPath indexPathForItem:lastItemIndex inSection:section];
  UICollectionViewLayoutAttributes* attributes =
      [_mostVisitedView layoutAttributesForItemAtIndexPath:lastCellIndexPath];
  CGRect lastCellFrame = attributes.frame;
  CGRect cellFrameInSuperview =
      [_mostVisitedView convertRect:lastCellFrame toView:self.view];

  // Calculate when the bottom of the cell passes through the bottom of
  // |self.view|.
  CGFloat maxY = CGRectGetMaxY(cellFrameInSuperview);
  CGFloat viewHeight = CGRectGetHeight(self.view.frame);

  CGFloat pixelsBelowFrame = maxY - viewHeight;
  CGFloat alpha = pixelsBelowFrame / kNewTabPageDistanceToFadeShadow;
  alpha = MIN(MAX(alpha, 0), 1);
  return alpha;
}

- (void)willUpdateSnapshot {
  [_overscrollActionsController clear];
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [self.logoVendor logoAnimationControllerOwner];
}

#pragma mark - UIScrollViewDelegate Methods.

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self.delegate updateNtpBarShadowForPanelController:self];
  [_overscrollActionsController scrollViewDidScroll:scrollView];

  // Blur the omnibox when the scroll view is scrolled below the pinned offset.
  CGFloat pinnedOffsetY = [self pinnedOffsetY];
  if (_omniboxFocused && scrollView.dragging &&
      scrollView.contentOffset.y < pinnedOffsetY) {
    [self.dispatcher cancelOmniboxEdit];
  }

  if (IsIPadIdiom()) {
    return;
  }

  if (_animateHeader) {
    [self updateSearchField];
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [_overscrollActionsController scrollViewWillBeginDragging:scrollView];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  [_overscrollActionsController scrollViewDidEndDragging:scrollView
                                          willDecelerate:decelerate];
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  [_overscrollActionsController scrollViewWillEndDragging:scrollView
                                             withVelocity:velocity
                                      targetContentOffset:targetContentOffset];

  if (IsIPadIdiom() || _omniboxFocused)
    return;

  CGFloat pinnedOffsetY = [self pinnedOffsetY];
  CGFloat offsetY = scrollView.contentOffset.y;
  CGFloat targetY = targetContentOffset->y;
  if (offsetY > 0 && offsetY < pinnedOffsetY) {
    // Omnibox is currently between middle and top of screen.
    if (velocity.y > 0) {  // scrolling upwards
      if (targetY < pinnedOffsetY) {
        // Scroll the omnibox up to |pinnedOffsetY| if velocity is upwards but
        // scrolling will stop before reaching |pinnedOffsetY|.
        targetContentOffset->y = offsetY;
        [_mostVisitedView setContentOffset:CGPointMake(0, pinnedOffsetY)
                                  animated:YES];
      }
      _scrolledToTop = YES;
    } else {  // scrolling downwards
      if (targetY > 0) {
        // Scroll the omnibox down to zero if velocity is downwards or 0 but
        // scrolling will stop before reaching 0.
        targetContentOffset->y = offsetY;
        [_mostVisitedView setContentOffset:CGPointZero animated:YES];
      }
      _scrolledToTop = NO;
    }
  } else if (offsetY > pinnedOffsetY &&
             targetContentOffset->y < pinnedOffsetY) {
    // Most visited cells are currently scrolled up past the omnibox but will
    // end the scroll below the omnibox. Stop the scroll at just below the
    // omnibox.
    targetContentOffset->y = offsetY;
    [_mostVisitedView setContentOffset:CGPointMake(0, pinnedOffsetY)
                              animated:YES];
    _scrolledToTop = YES;
  } else if (offsetY >= pinnedOffsetY) {
    _scrolledToTop = YES;
  } else if (offsetY <= 0) {
    _scrolledToTop = NO;
  }
}

#pragma mark - Most visited / Suggestions service wrapper methods.

- (NSUInteger)numberOfItems {
  NSUInteger numItems = [self.dataSource mostVisitedSize];
  NSUInteger maxItems = [self numberOfColumns] * kMaxNumMostVisitedFaviconRows;
  return MIN(maxItems, numItems);
}

- (NSInteger)numberOfNonEmptyTilesShown {
  NSInteger numCells =
      MIN([self numberOfItems], self.maximumMostVisitedSitesShown);
  return MAX(numCells, [self numberOfColumns]);
}

- (GURL)urlForIndex:(NSUInteger)index {
  return [self.dataSource mostVisitedAtIndex:index].url;
}

#pragma mark - GoogleLandingViewController (ExposedForTesting) methods.

- (BOOL)scrolledToTop {
  return _scrolledToTop;
}

- (BOOL)animateHeader {
  return _animateHeader;
}

#pragma mark - OverscrollActionsControllerDelegate

- (void)overscrollActionsController:(OverscrollActionsController*)controller
                   didTriggerAction:(OverscrollAction)action {
  switch (action) {
    case OverscrollAction::NEW_TAB: {
      OpenNewTabCommand* command = [OpenNewTabCommand command];
      [self.dispatcher openNewTab:command];
    } break;
    case OverscrollAction::CLOSE_TAB:
      [self.dispatcher closeCurrentTab];
      break;
    case OverscrollAction::REFRESH:
      [self reload];
      break;
    case OverscrollAction::NONE:
      NOTREACHED();
      break;
  }
}

- (BOOL)shouldAllowOverscrollActions {
  return YES;
}

- (UIView*)toolbarSnapshotView {
  return [[_headerView toolBarView] snapshotViewAfterScreenUpdates:NO];
}

- (UIView*)headerView {
  return self.view;
}

- (CGFloat)overscrollActionsControllerHeaderInset:
    (OverscrollActionsController*)controller {
  return 0;
}

- (CGFloat)overscrollHeaderHeight {
  return [_headerView toolBarView].bounds.size.height;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  return [self nearestAncestorOfView:touch.view
                           withClass:[MostVisitedCell class]] == nil;
}

- (UIView*)nearestAncestorOfView:(UIView*)view withClass:(Class)aClass {
  if (!view) {
    return nil;
  }
  if ([view isKindOfClass:aClass]) {
    return view;
  }
  return [self nearestAncestorOfView:[view superview] withClass:aClass];
}

#pragma mark - GoogleLandingConsumer

- (void)setLogoIsShowing:(BOOL)logoIsShowing {
  _logoIsShowing = logoIsShowing;
  [self updateLogoAndFakeboxDisplay];
}

- (void)mostVisitedDataUpdated {
  [self reloadData];
}

- (void)mostVisitedIconMadeAvailableAtIndex:(NSUInteger)index {
  if (index >= [self numberOfItems])
    return;

  NSIndexPath* indexPath =
      [NSIndexPath indexPathForRow:index inSection:SectionWithMostVisited];
  [_mostVisitedView reloadItemsAtIndexPaths:@[ indexPath ]];
}

- (void)setTabCount:(int)tabCount {
  _tabCount = tabCount;
  [_headerView setToolbarTabCount:self.tabCount];
}

- (void)setCanGoForward:(BOOL)canGoForward {
  _canGoForward = canGoForward;
  [_headerView setCanGoForward:self.canGoForward];
}

- (void)setCanGoBack:(BOOL)canGoBack {
  _canGoBack = canGoBack;
  [_headerView setCanGoBack:self.canGoBack];
}
@end
