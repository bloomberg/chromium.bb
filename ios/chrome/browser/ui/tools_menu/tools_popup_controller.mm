// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_view.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/shared/chrome/browser/ui/tools_menu/tools_menu_configuration.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

NSString* const kToolsMenuTableViewId = @"kToolsMenuTableViewId";

namespace {

const CGFloat kToolsPopupMenuWidth = 280.0;
const CGFloat kToolsPopupMenuTrailingOffset = 4;

// Inset for the shadows of the contained views.
NS_INLINE UIEdgeInsets TabHistoryPopupMenuInsets() {
  return UIEdgeInsetsMake(9, 11, 12, 11);
}

}  // namespace

@interface ToolsPopupController ()<ToolsPopupTableDelegate> {
  ToolsMenuViewController* _toolsMenuViewController;
  // Container view of the menu items table.
  UIView* _toolsTableViewContainer;
}
@end

@implementation ToolsPopupController
@synthesize isCurrentPageBookmarked = _isCurrentPageBookmarked;

- (instancetype)initWithConfiguration:(ToolsMenuConfiguration*)configuration {
  DCHECK(configuration.displayView);
  self = [super initWithParentView:configuration.displayView];
  if (self) {
    _toolsMenuViewController = [[ToolsMenuViewController alloc] init];
    _toolsTableViewContainer = [_toolsMenuViewController view];
    [_toolsTableViewContainer layer].cornerRadius = 2;
    [_toolsTableViewContainer layer].masksToBounds = YES;
    [_toolsMenuViewController initializeMenuWithConfiguration:configuration];

    UIEdgeInsets popupInsets = TabHistoryPopupMenuInsets();
    CGFloat popupWidth = kToolsPopupMenuWidth;

    CGPoint origin = CGPointMake(CGRectGetMidX(configuration.sourceRect),
                                 CGRectGetMidY(configuration.sourceRect));

    CGRect containerBounds = [configuration.displayView bounds];
    CGFloat minY = CGRectGetMinY(configuration.sourceRect) - popupInsets.top;

    // The tools popup appears trailing- aligned, but because
    // kToolsPopupMenuTrailingOffset is smaller than the popupInsets's trailing
    // value, destination needs to be shifted a bit.
    CGFloat trailingShift =
        UIEdgeInsetsGetTrailing(popupInsets) - kToolsPopupMenuTrailingOffset;
    if (UseRTLLayout())
      trailingShift = -trailingShift;

    CGPoint destination = CGPointMake(
        CGRectGetTrailingEdge(containerBounds) + trailingShift, minY);

    CGFloat availableHeight =
        CGRectGetHeight([configuration.displayView bounds]) - minY -
        popupInsets.bottom;
    CGFloat optimalHeight =
        [_toolsMenuViewController optimalHeight:availableHeight];
    [self setOptimalSize:CGSizeMake(popupWidth, optimalHeight)
                atOrigin:destination];

    CGRect bounds = [[self popupContainer] bounds];
    CGRect frame = UIEdgeInsetsInsetRect(bounds, popupInsets);

    [_toolsTableViewContainer setFrame:frame];
    [[self popupContainer] addSubview:_toolsTableViewContainer];

    [_toolsMenuViewController setDelegate:self];
    [self fadeInPopupFromSource:origin toDestination:destination];

    // Insert |toolsButton| above |popupContainer| so it appears stationary.
    // Otherwise the tools button will animate with the tools popup.
    UIButton* toolsButton = [_toolsMenuViewController toolsButton];
    if (toolsButton) {
      UIView* outsideAnimationView = [[self popupContainer] superview];
      const CGFloat buttonWidth = 48;
      // |origin| is the center of the tools menu icon in the toolbar; use
      // that to determine where the tools button should be placed.
      CGPoint buttonCenter =
          [configuration.displayView convertPoint:origin
                                           toView:outsideAnimationView];
      CGRect frame = CGRectMake(buttonCenter.x - buttonWidth / 2.0,
                                buttonCenter.y - buttonWidth / 2.0, buttonWidth,
                                buttonWidth);
      [toolsButton setFrame:frame];
      [toolsButton setImageEdgeInsets:configuration.toolsButtonInsets];
      [outsideAnimationView addSubview:toolsButton];
    }
  }
  return self;
}

- (void)dealloc {
  [_toolsTableViewContainer removeFromSuperview];
  [_toolsMenuViewController setDelegate:nil];
}

- (void)fadeInPopupFromSource:(CGPoint)source
                toDestination:(CGPoint)destination {
  [_toolsMenuViewController animateContentIn];
  [super fadeInPopupFromSource:source toDestination:destination];
}

- (void)dismissAnimatedWithCompletion:(void (^)(void))completion {
  [_toolsMenuViewController hideContent];
  [super dismissAnimatedWithCompletion:completion];
}

- (void)setIsCurrentPageBookmarked:(BOOL)value {
  _isCurrentPageBookmarked = value;
  [_toolsMenuViewController setIsCurrentPageBookmarked:value];
}

- (void)setCanUseReaderMode:(BOOL)enabled {
  [_toolsMenuViewController setCanUseReaderMode:enabled];
}

- (void)setCanShowFindBar:(BOOL)enabled {
  [_toolsMenuViewController setCanShowFindBar:enabled];
}

- (void)setCanShowShareMenu:(BOOL)enabled {
  [_toolsMenuViewController setCanShowShareMenu:enabled];
}

- (void)setIsTabLoading:(BOOL)isTabLoading {
  [_toolsMenuViewController setIsTabLoading:isTabLoading];
}

#pragma mark - ToolsPopupTableDelegate methods

- (void)commandWasSelected:(int)commandID {
  // Record the corresponding metric.
  switch (commandID) {
    case IDC_TEMP_EDIT_BOOKMARK:
      base::RecordAction(UserMetricsAction("MobileMenuEditBookmark"));
      break;
    case IDC_BOOKMARK_PAGE:
      base::RecordAction(UserMetricsAction("MobileMenuAddToBookmarks"));
      break;
    case IDC_CLOSE_ALL_TABS:
      base::RecordAction(UserMetricsAction("MobileMenuCloseAllTabs"));
      break;
    case IDC_CLOSE_ALL_INCOGNITO_TABS:
      base::RecordAction(UserMetricsAction("MobileMenuCloseAllIncognitoTabs"));
      break;
    case IDC_FIND:
      base::RecordAction(UserMetricsAction("MobileMenuFindInPage"));
      break;
    case IDC_HELP_PAGE_VIA_MENU:
      base::RecordAction(UserMetricsAction("MobileMenuHelp"));
      break;
    case IDC_NEW_INCOGNITO_TAB:
      base::RecordAction(UserMetricsAction("MobileMenuNewIncognitoTab"));
      break;
    case IDC_NEW_TAB:
      base::RecordAction(UserMetricsAction("MobileMenuNewTab"));
      break;
    case IDC_OPTIONS:
      base::RecordAction(UserMetricsAction("MobileMenuSettings"));
      break;
    case IDC_RELOAD:
      base::RecordAction(UserMetricsAction("MobileMenuReload"));
      break;
    case IDC_SHARE_PAGE:
      base::RecordAction(UserMetricsAction("MobileMenuShare"));
      break;
    case IDC_REQUEST_DESKTOP_SITE:
      base::RecordAction(UserMetricsAction("MobileMenuRequestDesktopSite"));
      break;
    case IDC_REQUEST_MOBILE_SITE:
      base::RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
      break;
    case IDC_READER_MODE:
      base::RecordAction(UserMetricsAction("MobileMenuRequestReaderMode"));
      break;
    case IDC_SHOW_BOOKMARK_MANAGER:
      base::RecordAction(UserMetricsAction("MobileMenuAllBookmarks"));
      break;
    case IDC_SHOW_HISTORY:
      base::RecordAction(UserMetricsAction("MobileMenuHistory"));
      break;
    case IDC_SHOW_OTHER_DEVICES:
      base::RecordAction(UserMetricsAction("MobileMenuRecentTabs"));
      break;
    case IDC_STOP:
      base::RecordAction(UserMetricsAction("MobileMenuStop"));
      break;
    case IDC_PRINT:
      base::RecordAction(UserMetricsAction("MobileMenuPrint"));
      break;
    case IDC_REPORT_AN_ISSUE:
      self.containerView.hidden = YES;
      base::RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
      break;
    case IDC_VIEW_SOURCE:
      // Debug only; no metric.
      break;
    case IDC_SHOW_TOOLS_MENU:
      // Do nothing when tapping the tools menu a second time.
      break;
    case IDC_SHOW_READING_LIST:
      base::RecordAction(UserMetricsAction("MobileMenuReadingList"));
      break;
    case IDC_SHOW_SUGGESTIONS:
      // TODO(crbug.com/682174): Move it out of the tool menu or add metrics.
      break;
    default:
      NOTREACHED();
      break;
  }

  // Close the menu.
  [self.delegate dismissPopupMenu:self];
}

@end
