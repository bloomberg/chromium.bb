// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"

#include <stdint.h>

#include "base/ios/ios_util.h"
#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/field_trial.h"
#include "components/reading_list/core/reading_list_switches.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/animation_util.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_notification_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_notifier.h"
#import "ios/chrome/browser/ui/tools_menu/reading_list_menu_view_item.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_context.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_item.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_tools_cell.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#import "ios/third_party/material_components_ios/src/components/Ink/src/MaterialInk.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using ios::material::TimingFunction;

NSString* const kToolsMenuNewTabId = @"kToolsMenuNewTabId";
NSString* const kToolsMenuNewIncognitoTabId = @"kToolsMenuNewIncognitoTabId";
NSString* const kToolsMenuCloseAllTabsId = @"kToolsMenuCloseAllTabsId";
NSString* const kToolsMenuCloseAllIncognitoTabsId =
    @"kToolsMenuCloseAllIncognitoTabsId";
NSString* const kToolsMenuBookmarksId = @"kToolsMenuBookmarksId";
NSString* const kToolsMenuReadingListId = @"kToolsMenuReadingListId";
NSString* const kToolsMenuOtherDevicesId = @"kToolsMenuOtherDevicesId";
NSString* const kToolsMenuHistoryId = @"kToolsMenuHistoryId";
NSString* const kToolsMenuReportAnIssueId = @"kToolsMenuReportAnIssueId";
NSString* const kToolsMenuFindInPageId = @"kToolsMenuFindInPageId";
NSString* const kToolsMenuReaderMode = @"kToolsMenuReaderMode";
NSString* const kToolsMenuRequestDesktopId = @"kToolsMenuRequestDesktopId";
NSString* const kToolsMenuSettingsId = @"kToolsMenuSettingsId";
NSString* const kToolsMenuHelpId = @"kToolsMenuHelpId";
NSString* const kToolsMenuSuggestionsId = @"kToolsMenuSuggestionsId";

namespace {

// Time for ink to fade into view.
static const NSTimeInterval kMDCInkTouchDelayInterval = 0.15;

static const CGFloat kMenuItemHeight = 48;

static NSString* const kToolsItemCellID = @"ToolsItemCellID";

// Menu items can be marked as visible or not when Incognito is enabled.
// The following bits are used for |visibility| field in |MenuItemInfo|.
const NSInteger kVisibleIncognitoOnly = 1 << 0;
const NSInteger kVisibleNotIncognitoOnly = 1 << 1;

// Initialization table for all possible commands to initialize the
// tools menu at run time. Data initialized into this structure is not mutable.
struct MenuItemInfo {
  int title_id;
  NSString* accessibility_id;
  int command_id;
  int toolbar_types;
  // |visibility| is applied if a menu item is included for a given
  // |toolbar_types|. A value of 0 means the menu item is always visible for
  // the given |toolbar_types|.
  int visibility;
  // Custom class, if any, for the menu item, or |nil|.
  Class item_class;
};

// Flags for different toolbar types
typedef NS_OPTIONS(NSUInteger, kToolbarType) {
  // clang-format off
    kToolbarTypeNone            = 0,
    kToolbarTypeWebiPhone       = 1 << 0,
    kToolbarTypeWebiPad         = 1 << 1,
    kToolbarTypeNoTabsiPad      = 1 << 2,
    kToolbarTypeSwitcheriPhone  = 1 << 3,
    kToolbarTypeWebAll          = kToolbarTypeWebiPhone | kToolbarTypeWebiPad,
    kToolbarTypeAll             = kToolbarTypeWebAll |
                                  kToolbarTypeSwitcheriPhone |
                                  kToolbarTypeNoTabsiPad,
  // clang-format on
};

// Declare all the possible items.
static MenuItemInfo itemInfoList[] = {
    // clang-format off
  { IDS_IOS_TOOLS_MENU_NEW_TAB,           kToolsMenuNewTabId,
    IDC_NEW_TAB,                          kToolbarTypeAll,
    0,                                    nil },
  { IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, kToolsMenuNewIncognitoTabId,
    IDC_NEW_INCOGNITO_TAB,                kToolbarTypeAll,
    0,                                    nil },
  { IDS_IOS_TOOLS_MENU_CLOSE_ALL_TABS,    kToolsMenuCloseAllTabsId,
    IDC_CLOSE_ALL_TABS,                   kToolbarTypeSwitcheriPhone,
    kVisibleNotIncognitoOnly,             nil },
  { IDS_IOS_TOOLS_MENU_CLOSE_ALL_INCOGNITO_TABS,
    kToolsMenuCloseAllIncognitoTabsId,
    IDC_CLOSE_ALL_INCOGNITO_TABS,         kToolbarTypeSwitcheriPhone,
    kVisibleIncognitoOnly,                nil },
  { IDS_IOS_TOOLS_MENU_BOOKMARKS,         kToolsMenuBookmarksId,
    IDC_SHOW_BOOKMARK_MANAGER,            kToolbarTypeWebAll,
    0,                                    nil },
  { IDS_IOS_TOOLS_MENU_READING_LIST,      kToolsMenuReadingListId,
    IDC_SHOW_READING_LIST,                kToolbarTypeWebAll,
    0,                                    [ReadingListMenuViewItem class] },
  { IDS_IOS_TOOLS_MENU_SUGGESTIONS,       kToolsMenuSuggestionsId,
    IDC_SHOW_SUGGESTIONS,                 kToolbarTypeWebAll,
    0,                                    nil },
  { IDS_IOS_TOOLS_MENU_RECENT_TABS,       kToolsMenuOtherDevicesId,
    IDC_SHOW_OTHER_DEVICES,               kToolbarTypeWebAll,
    kVisibleNotIncognitoOnly,             nil },
  { IDS_HISTORY_SHOW_HISTORY,             kToolsMenuHistoryId,
    IDC_SHOW_HISTORY,                     kToolbarTypeWebAll,
    0,                                    nil },
  { IDS_IOS_OPTIONS_REPORT_AN_ISSUE,      kToolsMenuReportAnIssueId,
    IDC_REPORT_AN_ISSUE,                  kToolbarTypeAll,
    0,                                    nil },
  { IDS_IOS_TOOLS_MENU_FIND_IN_PAGE,      kToolsMenuFindInPageId,
    IDC_FIND,                             kToolbarTypeWebAll,
    0,                                    nil },
  { IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE, kToolsMenuRequestDesktopId,
    IDC_REQUEST_DESKTOP_SITE,             kToolbarTypeWebAll,
    0,                                    nil },
  { IDS_IOS_TOOLS_MENU_READER_MODE,       kToolsMenuReaderMode,
    IDC_READER_MODE,                      kToolbarTypeWebAll,
    0,                                    nil },
  { IDS_IOS_TOOLS_MENU_SETTINGS,          kToolsMenuSettingsId,
    IDC_OPTIONS,                          kToolbarTypeAll,
    0,                                    nil },
  { IDS_IOS_TOOLS_MENU_HELP_MOBILE,           kToolsMenuHelpId,
    IDC_HELP_PAGE_VIA_MENU,               kToolbarTypeWebAll,
    0,                                    nil },
    // clang-format on
};

NS_INLINE BOOL ItemShouldBeVisible(const MenuItemInfo& item,
                                   BOOL incognito,
                                   kToolbarType toolbarType) {
  if (!(item.toolbar_types & toolbarType))
    return NO;

  if (incognito && (item.visibility & kVisibleNotIncognitoOnly))
    return NO;

  if (!incognito && (item.visibility & kVisibleIncognitoOnly))
    return NO;

  if (item.title_id == IDS_IOS_TOOLBAR_SHOW_TABS) {
    if (!IsIPadIdiom()) {
      return NO;
    }
  }

  if (item.title_id == IDS_IOS_TOOLS_MENU_READER_MODE) {
    if (!experimental_flags::IsReaderModeEnabled()) {
      return NO;
    }
  }

  if (item.title_id == IDS_IOS_TOOLS_MENU_READING_LIST) {
    if (!reading_list::switches::IsReadingListEnabled()) {
      return NO;
    }
  }

  if (item.title_id == IDS_IOS_TOOLS_MENU_SUGGESTIONS) {
    if (!experimental_flags::IsSuggestionsUIEnabled()) {
      return NO;
    }
  }

  if (item.title_id == IDS_IOS_OPTIONS_REPORT_AN_ISSUE) {
    if (!ios::GetChromeBrowserProvider()
             ->GetUserFeedbackProvider()
             ->IsUserFeedbackEnabled()) {
      return NO;
    }
  }

  return YES;
}

NS_INLINE void AnimateInViews(NSArray* views,
                              CGFloat initialX,
                              CGFloat initialY) {
  [views enumerateObjectsUsingBlock:^(UIView* view, NSUInteger index,
                                      BOOL* stop) {
    CGFloat beginTime = index * .035;
    CABasicAnimation* transformAnimation =
        [CABasicAnimation animationWithKeyPath:@"transform"];
    [transformAnimation
        setFromValue:[NSValue
                         valueWithCATransform3D:CATransform3DMakeTranslation(
                                                    initialX, initialY, 0)]];
    [transformAnimation
        setToValue:[NSValue valueWithCATransform3D:CATransform3DIdentity]];
    [transformAnimation setDuration:ios::material::kDuration1];
    [transformAnimation setBeginTime:beginTime];

    CAAnimation* fadeAnimation = OpacityAnimationMake(0, 1);
    [fadeAnimation setDuration:ios::material::kDuration1];
    [fadeAnimation
        setTimingFunction:TimingFunction(ios::material::CurveEaseOut)];
    [fadeAnimation setBeginTime:beginTime];

    [[view layer]
        addAnimation:AnimationGroupMake(@[ transformAnimation, fadeAnimation ])
              forKey:@"animateIn"];
  }];
}
}  // anonymous namespace

@interface ToolsMenuCollectionView : UICollectionView
@property(nonatomic, assign) CGPoint touchBeginPoint;
@property(nonatomic, assign) CGPoint touchEndPoint;
@end

@implementation ToolsMenuCollectionView

@synthesize touchBeginPoint = _touchBeginPoint;
@synthesize touchEndPoint = _touchEndPoint;

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  _touchBeginPoint = [[touches anyObject] locationInView:self];
  [super touchesBegan:touches withEvent:event];
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  _touchEndPoint = [[touches anyObject] locationInView:self];
  [super touchesEnded:touches withEvent:event];
}

@end

// Class Extension for private methods.
@interface ToolsMenuViewController ()<UICollectionViewDelegateFlowLayout,
                                      UICollectionViewDataSource,
                                      ReadingListMenuNotificationDelegate> {
  base::mac::ObjCPropertyReleaser _propertyReleaser_ToolsMenuViewController;
  BOOL _waitForInk;
  // Weak pointer to ReadingListMenuNotifier, used to set the starting values
  // for the reading list badge.
  base::WeakNSObject<ReadingListMenuNotifier> _readingListMenuNotifier;
}
@property(nonatomic, retain) ToolsMenuCollectionView* menuView;
@property(nonatomic, retain) MDCInkView* touchFeedbackView;
@property(nonatomic, retain) NSMutableArray* menuItems;
@property(nonatomic, assign) kToolbarType toolbarType;

// Get the reading list cell.
- (ReadingListMenuViewCell*)readingListCell;
@end

@implementation ToolsMenuViewController

@synthesize menuView = _menuView;
@synthesize isCurrentPageBookmarked = _isCurrentPageBookmarked;
@synthesize touchFeedbackView = _touchFeedbackView;
@synthesize isTabLoading = _isTabLoading;
@synthesize toolbarType = _toolbarType;
@synthesize menuItems = _menuItems;
@synthesize delegate = _delegate;

#pragma mark Public methods

- (CGFloat)optimalHeight:(CGFloat)suggestedHeight {
  NSInteger numberOfItems = [self.menuItems count];
  if (_toolbarType == kToolbarTypeWebiPhone) {
    // Account for the height of the first row, not included in |menuItems|.
    numberOfItems++;
  }
  CGFloat maxItems = suggestedHeight / kMenuItemHeight;
  if (maxItems >= numberOfItems) {
    return numberOfItems * kMenuItemHeight;
  } else {
    const CGFloat halfHeight = kMenuItemHeight / 2;
    return round(maxItems) * kMenuItemHeight - halfHeight;
  }
}

- (void)setItemEnabled:(BOOL)enabled withTag:(NSInteger)tag {
  for (ToolsMenuViewItem* item in _menuItems) {
    if ([item tag] == tag) {
      [item setActive:enabled];
      break;
    }
  }

  for (ToolsMenuViewCell* cell in [_menuView visibleCells]) {
    if (![cell isKindOfClass:[ToolsMenuViewCell class]])
      continue;

    if ([cell tag] != tag)
      continue;

    NSIndexPath* path = [_menuView indexPathForCell:cell];
    NSInteger itemIndex = [self dataIndexForIndexPath:path];
    [cell configureForMenuItem:[_menuItems objectAtIndex:itemIndex]];
  }
}

- (void)setIsCurrentPageBookmarked:(BOOL)value {
  _isCurrentPageBookmarked = value;

  ToolsMenuViewToolsCell* toolsCell = [self toolsCell];
  [[toolsCell starButton] setHidden:_isCurrentPageBookmarked];
  [[toolsCell starredButton] setHidden:!_isCurrentPageBookmarked];
}

- (void)setCanUseReaderMode:(BOOL)enabled {
  [self setItemEnabled:enabled withTag:IDC_READER_MODE];
}

- (void)setCanUseDesktopUserAgent:(BOOL)enabled {
  [self setItemEnabled:enabled withTag:IDC_REQUEST_DESKTOP_SITE];
}

- (void)setCanShowFindBar:(BOOL)enabled {
  [self setItemEnabled:enabled withTag:IDC_FIND];
}

- (void)setCanShowShareMenu:(BOOL)enabled {
  ToolsMenuViewToolsCell* toolsCell = [self toolsCell];
  [[toolsCell shareButton] setEnabled:enabled];
  [self setItemEnabled:enabled withTag:IDC_SHARE_PAGE];
}

- (UIButton*)toolsButton {
  UIButton* toolsButton = [[self toolsCell] toolsButton];
  [toolsButton addTarget:self
                  action:@selector(buttonPressed:)
        forControlEvents:UIControlEventTouchUpInside];
  [toolsButton setTranslatesAutoresizingMaskIntoConstraints:YES];
  [toolsButton setOpaque:NO];
  [toolsButton setBackgroundColor:[UIColor clearColor]];
  return toolsButton;
}

- (void)setIsTabLoading:(BOOL)isTabLoading {
  _isTabLoading = isTabLoading;

  ToolsMenuViewToolsCell* toolsCell = [self toolsCell];
  [[toolsCell stopButton] setHidden:!isTabLoading];
  [[toolsCell reloadButton] setHidden:isTabLoading];
}

- (void)initializeMenu:(ToolsMenuContext*)context {
  if (context.readingListMenuNotifier) {
    _readingListMenuNotifier.reset(context.readingListMenuNotifier);
    [context.readingListMenuNotifier setDelegate:self];
  }

  if (IsIPadIdiom()) {
    _toolbarType = context.hasNoOpenedTabs
                       ? kToolbarTypeNoTabsiPad
                       : (!IsCompactTablet() ? kToolbarTypeWebiPad
                                             : kToolbarTypeWebiPhone);
  } else {
    // kOptionInTabSwitcher option must be enabled on iPhone with
    // no opened tabs.
    DCHECK(!context.hasNoOpenedTabs || context.isInTabSwitcher);
    _toolbarType = context.isInTabSwitcher ? kToolbarTypeSwitcheriPhone
                                           : kToolbarTypeWebiPhone;
  }

  // Build the menu, adding all relevant items.
  NSMutableArray* menu = [NSMutableArray array];

  for (size_t i = 0; i < arraysize(itemInfoList); ++i) {
    const MenuItemInfo& item = itemInfoList[i];
    if (!ItemShouldBeVisible(item, context.isInIncognito, _toolbarType))
      continue;

    NSString* title = l10n_util::GetNSStringWithFixup(item.title_id);
    Class itemClass =
        item.item_class ? item.item_class : [ToolsMenuViewItem class];
    // Sanity check that the class is a useful one.
    DCHECK([itemClass respondsToSelector:@selector(menuItemWithTitle:
                                             accessibilityIdentifier:
                                                             command:)]);
    [menu addObject:[itemClass menuItemWithTitle:title
                         accessibilityIdentifier:item.accessibility_id
                                         command:item.command_id]];
  }

#if !defined(NDEBUG)
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  if ((_toolbarType & kToolbarTypeWebAll) &&
      [standardDefaults boolForKey:@"DevViewSource"]) {
    // Debug menu, not localized, only visible if turned on by a default.
    [menu addObject:[self createViewSourceItem]];
  }
#endif  // !defined(NDEBUG)

  [self setMenuItems:menu];

  // Disable IDC_CLOSE_ALL_TABS menu item if on phone with no tabs.
  if (!IsIPadIdiom()) {
    [self setItemEnabled:!context.hasNoOpenedTabs withTag:IDC_CLOSE_ALL_TABS];
  }
}

#if !defined(NDEBUG)
- (ToolsMenuViewItem*)createViewSourceItem {
  return [ToolsMenuViewItem menuItemWithTitle:@"View Source"
                      accessibilityIdentifier:@"View Source"
                                      command:IDC_VIEW_SOURCE];
}
#endif  // !defined(NDEBUG)

#pragma mark - Data handling utilities

- (ToolsMenuViewToolsCell*)toolsCell {
  for (ToolsMenuViewToolsCell* visibleCell in [_menuView visibleCells]) {
    if ([visibleCell isKindOfClass:[ToolsMenuViewToolsCell class]])
      return visibleCell;
  }

  return nil;
}

- (ReadingListMenuViewCell*)readingListCell {
  for (ReadingListMenuViewCell* visibleCell in [_menuView visibleCells]) {
    if ([visibleCell isKindOfClass:[ReadingListMenuViewCell class]])
      return visibleCell;
  }

  return nil;
}

- (NSInteger)dataIndexForIndexPath:(NSIndexPath*)path {
  NSInteger item = [path item];

  if (_toolbarType == kToolbarTypeWebiPhone)
    --item;

  return item;
}

#pragma mark - UIViewController Overrides

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil {
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
  if (self)
    [self commonInitialization];

  return self;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  self = [super initWithCoder:aDecoder];
  if (self)
    [self commonInitialization];

  return self;
}

- (void)commonInitialization {
  _propertyReleaser_ToolsMenuViewController.Init(
      self, [ToolsMenuViewController class]);
  _readingListMenuNotifier.reset();
}

- (void)loadView {
  [super loadView];

  UIView* rootView = [self view];
  [rootView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                UIViewAutoresizingFlexibleWidth];
  [rootView setBackgroundColor:[UIColor whiteColor]];

  _touchFeedbackView = [[MDCInkView alloc] initWithFrame:CGRectZero];

  base::scoped_nsobject<UICollectionViewFlowLayout> menuItemsLayout(
      [[UICollectionViewFlowLayout alloc] init]);

  _menuView = [[ToolsMenuCollectionView alloc] initWithFrame:[rootView bounds]
                                        collectionViewLayout:menuItemsLayout];
  [_menuView setAccessibilityLabel:l10n_util::GetNSString(IDS_IOS_TOOLS_MENU)];
  [_menuView setAccessibilityIdentifier:kToolsMenuTableViewId];
  [_menuView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_menuView setBackgroundColor:[UIColor whiteColor]];
  [_menuView setDataSource:self];
  [_menuView setDelegate:self];
  [_menuView setOpaque:YES];

  [rootView addSubview:_menuView];
  [self updateViewConstraints];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [_menuView registerClass:[ToolsMenuViewToolsCell class]
      forCellWithReuseIdentifier:kToolsItemCellID];

  [_menuView registerClass:[ToolsMenuViewItem cellClass]
      forCellWithReuseIdentifier:[ToolsMenuViewItem cellID]];

  // Register each custom class.
  NSMutableSet* registeredClasses = [NSMutableSet set];
  for (size_t i = 0; i < arraysize(itemInfoList); ++i) {
    const MenuItemInfo& item = itemInfoList[i];
    if (!item.item_class || [registeredClasses containsObject:item.item_class])
      continue;
    [_menuView registerClass:[item.item_class cellClass]
        forCellWithReuseIdentifier:[item.item_class cellID]];
    [registeredClasses addObject:item.item_class];
  }
}

- (void)updateViewConstraints {
  [super updateViewConstraints];

  UIView* rootView = [self view];
  NSDictionary* view = @{ @"menu" : _menuView };
  NSArray* constraints = @[ @"V:|-(0)-[menu]-(0)-|", @"H:|-(0)-[menu]-(0)-|" ];
  ApplyVisualConstraints(constraints, view, rootView);
}

#pragma mark - Content Animation Stuff

- (void)animateContentIn {
  // Make sure that the collection view has laid out before trying to animate
  // the contents.
  [_menuView layoutIfNeeded];

  NSArray* visibleCells =
      [[_menuView visibleCells] sortedArrayUsingComparator:^NSComparisonResult(
                                    UIView* view1, UIView* view2) {
        CGPoint center1 = [view1 center];
        CGPoint center2 = [view2 center];

        if (center1.y < center2.y)
          return NSOrderedAscending;

        if (center1.y > center2.y)
          return NSOrderedDescending;

        return NSOrderedSame;
      }];

  ToolsMenuViewToolsCell* toolsCell = nil;
  if (_toolbarType == kToolbarTypeWebiPhone) {
    toolsCell = [visibleCells firstObject];
    if ([toolsCell isKindOfClass:[ToolsMenuViewToolsCell class]]) {
      visibleCells = [visibleCells
          subarrayWithRange:NSMakeRange(1, [visibleCells count] - 1)];
    } else {
      toolsCell = nil;
    }
  }

  [CATransaction begin];
  [CATransaction
      setAnimationTimingFunction:TimingFunction(ios::material::CurveEaseInOut)];
  [CATransaction setAnimationDuration:ios::material::kDuration5];
  AnimateInViews([toolsCell allButtons], 10, 0);
  AnimateInViews(visibleCells, 0, -10);
  [CATransaction commit];

  [[self readingListCell]
      updateBadgeCount:_readingListMenuNotifier.get().readingListUnreadCount
              animated:YES];
  [[self readingListCell]
      updateSeenState:_readingListMenuNotifier.get().readingListUnseenItemsExist
             animated:YES];
}

- (void)hideContent {
  _menuView.alpha = 0;

  // Remove the target/action for touching the buttons. VoiceOver may hold on
  // to these buttons after |self| has been released, causing a crash if a
  // button is activated (see http://crbug.com/480974 ).
  UIButton* toolsButton = [[self toolsCell] toolsButton];
  [toolsButton removeTarget:self
                     action:@selector(buttonPressed:)
           forControlEvents:UIControlEventTouchUpInside];
  for (UIButton* button in [[self toolsCell] allButtons]) {
    [button removeTarget:self
                  action:@selector(buttonPressed:)
        forControlEvents:UIControlEventTouchUpInside];
  }
}

#pragma mark - Button event handling

- (IBAction)buttonPressed:(id)sender {
  int commandId = [sender tag];
  DCHECK(commandId);
  // The bookmark command workaround is only needed for metrics; remap it
  // to the real command for the dispatch. This is very hacky, but it will go
  // away soon.  See crbug/228521
  DCHECK([sender respondsToSelector:@selector(setTag:)]);
  if (commandId == IDC_TEMP_EDIT_BOOKMARK)
    [sender setTag:IDC_BOOKMARK_PAGE];
  // Do nothing when tapping the tools menu a second time.
  if (commandId != IDC_SHOW_TOOLS_MENU) {
    [self chromeExecuteCommand:sender];
  }
  if (commandId == IDC_TEMP_EDIT_BOOKMARK)
    [sender setTag:IDC_TEMP_EDIT_BOOKMARK];

  [_delegate commandWasSelected:commandId];
}

#pragma mark - UICollectionViewDelegate Implementation

- (BOOL)collectionView:(ToolsMenuCollectionView*)view
    shouldHighlightItemAtIndexPath:(NSIndexPath*)path {
  if (view.tracking)
    return NO;
  NSInteger item = [self dataIndexForIndexPath:path];
  return (item >= 0);
}

- (void)collectionView:(ToolsMenuCollectionView*)view
    didHighlightItemAtIndexPath:(NSIndexPath*)path {
  ToolsMenuViewCell* cell =
      (ToolsMenuViewCell*)[view cellForItemAtIndexPath:path];

  NSInteger item = [self dataIndexForIndexPath:path];
  DCHECK_GE(item, 0);
  ToolsMenuViewItem* menuItem = [_menuItems objectAtIndex:item];
  DCHECK(menuItem);
  if ([menuItem active]) {
    [_touchFeedbackView setFrame:cell.bounds];
    [cell addSubview:_touchFeedbackView];

    CGPoint touchPoint = [view touchBeginPoint];
    touchPoint = [view convertPoint:touchPoint toView:_touchFeedbackView];
    [_touchFeedbackView startTouchBeganAnimationAtPoint:touchPoint
                                             completion:nil];
  }
}

- (void)collectionView:(ToolsMenuCollectionView*)view
    didUnhighlightItemAtIndexPath:(NSIndexPath*)path {
  CGPoint touchPoint = [view touchEndPoint];
  touchPoint = [view convertPoint:touchPoint toView:_touchFeedbackView];
  base::WeakNSObject<MDCInkView> inkView(_touchFeedbackView);
  _waitForInk = YES;
  [_touchFeedbackView startTouchEndedAnimationAtPoint:touchPoint
                                           completion:^{
                                             _waitForInk = NO;
                                             [inkView removeFromSuperview];
                                           }];
}

- (BOOL)collectionView:(UICollectionView*)view
    shouldSelectItemAtIndexPath:(NSIndexPath*)path {
  NSInteger item = [self dataIndexForIndexPath:path];
  if (item < 0)
    return NO;

  return [[_menuItems objectAtIndex:item] active];
}

- (void)collectionView:(UICollectionView*)view
    didSelectItemAtIndexPath:(NSIndexPath*)path {
  [view deselectItemAtIndexPath:path animated:YES];

  NSInteger item = [self dataIndexForIndexPath:path];
  if (item < 0)
    return;

  dispatch_time_t delayTime = dispatch_time(
      DISPATCH_TIME_NOW, (int64_t)(NSEC_PER_SEC * kMDCInkTouchDelayInterval));
  dispatch_after(
      _waitForInk ? delayTime : 0, dispatch_get_main_queue(), ^(void) {
        ToolsMenuViewItem* menuItem = [_menuItems objectAtIndex:item];
        DCHECK([menuItem tag]);
        [_delegate commandWasSelected:[menuItem tag]];
        [self chromeExecuteCommand:menuItem];
      });
}

#pragma mark - UICollectionViewDataSource Implementation

- (NSInteger)collectionView:(UICollectionView*)view
     numberOfItemsInSection:(NSInteger)section {
  NSInteger numberOfItems = [_menuItems count];
  if (_toolbarType == kToolbarTypeWebiPhone)
    ++numberOfItems;

  return numberOfItems;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)view
                 cellForItemAtIndexPath:(NSIndexPath*)path {
  NSInteger item = [self dataIndexForIndexPath:path];
  if (item < 0) {
    ToolsMenuViewToolsCell* cell =
        [view dequeueReusableCellWithReuseIdentifier:kToolsItemCellID
                                        forIndexPath:path];
    for (UIButton* button in [cell allButtons]) {
      [button addTarget:self
                    action:@selector(buttonPressed:)
          forControlEvents:UIControlEventTouchUpInside];
    }
    return cell;
  }

  ToolsMenuViewItem* menuItem = [_menuItems objectAtIndex:item];
  ToolsMenuViewCell* menuItemCell =
      [view dequeueReusableCellWithReuseIdentifier:[[menuItem class] cellID]
                                      forIndexPath:path];
  [menuItemCell configureForMenuItem:menuItem];

  return menuItemCell;
}

#pragma mark - UICollectionViewDelegateFlowLayout Implementation

- (CGSize)collectionView:(UICollectionView*)view
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)path {
  return CGSizeMake(CGRectGetWidth([_menuView bounds]), kMenuItemHeight);
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
                                 layout:(UICollectionViewLayout*)
                                            collectionViewLayout
    minimumLineSpacingForSectionAtIndex:(NSInteger)section {
  return 0;
}

#pragma mark - ReadingListMenuNotificationDelegate Implementation

- (void)unreadCountChanged:(NSInteger)unreadCount {
  [[self readingListCell] updateBadgeCount:unreadCount animated:YES];
}

- (void)unseenStateChanged:(BOOL)unseenItemsExist {
  [[self readingListCell] updateSeenState:unseenItemsExist animated:YES];
}

@end
