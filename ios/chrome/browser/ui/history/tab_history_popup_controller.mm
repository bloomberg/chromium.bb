// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/tab_history_popup_controller.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/history/tab_history_view_controller.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_view.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/public/navigation_item.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#include "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
static const CGFloat kTabHistoryMinWidth = 250.0;
static const CGFloat kTabHistoryMaxWidthLandscapePhone = 350.0;
// x coordinate for the textLabel in a default table cell with an image.
static const CGFloat kCellTextXCoordinate = 60.0;
// Inset for the shadows of the contained views.
NS_INLINE UIEdgeInsets TabHistoryPopupMenuInsets() {
  return UIEdgeInsetsMakeDirected(9, 11, 12, 11);
}

// Layout for popup: shift twelve pixels towards the leading edge.
LayoutOffset kHistoryPopupLeadingOffset = -12;
CGFloat kHistoryPopupYOffset = 3;
// Occupation of the Tools Container as a percentage of the parent height.
static const CGFloat kHeightPercentage = 0.85;
}  // anonymous namespace

@interface TabHistoryPopupController () {
  // TableViewController for the table displaying tab history entries.
  TabHistoryViewController* tabHistoryTableViewController_;

  // Container view of the history entries table.
  UIView* tabHistoryTableViewContainer_;
}

// Determines the width for the popup depending on the device, orientation, and
// CRWSessionEntrys to display.
- (CGFloat)calculatePopupWidth:(NSArray*)entries;

@property(nonatomic, strong)
    TabHistoryViewController* tabHistoryTableViewController;
@property(nonatomic, strong) UIView* tabHistoryTableViewContainer;

@end

@implementation TabHistoryPopupController

@synthesize tabHistoryTableViewController = tabHistoryTableViewController_;
@synthesize tabHistoryTableViewContainer = tabHistoryTableViewContainer_;

- (id)initWithOrigin:(CGPoint)origin
          parentView:(UIView*)parent
             entries:(NSArray*)entries {
  DCHECK(parent);
  self = [super initWithParentView:parent];
  if (self) {
    tabHistoryTableViewController_ = [[TabHistoryViewController alloc] init];
    [tabHistoryTableViewController_ setSessionEntries:entries];

    UICollectionView* collectionView =
        [tabHistoryTableViewController_ collectionView];
    [collectionView setAccessibilityIdentifier:@"Tab History"];

    tabHistoryTableViewContainer_ = [[UIView alloc] initWithFrame:CGRectZero];
    [tabHistoryTableViewContainer_ layer].cornerRadius = 2;
    [tabHistoryTableViewContainer_ layer].masksToBounds = YES;
    [tabHistoryTableViewContainer_ addSubview:collectionView];

    LayoutOffset originOffset =
        kHistoryPopupLeadingOffset - TabHistoryPopupMenuInsets().left;
    CGPoint newOrigin = CGPointLayoutOffset(origin, originOffset);
    newOrigin.y += kHistoryPopupYOffset;

    CGFloat availableHeight =
        (CGRectGetHeight([parent bounds]) - origin.y) * kHeightPercentage;
    CGFloat optimalHeight =
        [tabHistoryTableViewController_ optimalHeight:availableHeight];
    CGFloat popupWidth = [self calculatePopupWidth:entries];
    [self setOptimalSize:CGSizeMake(popupWidth, optimalHeight)
                atOrigin:newOrigin];

    CGRect bounds = [[self popupContainer] bounds];
    CGRect frame = UIEdgeInsetsInsetRect(bounds, TabHistoryPopupMenuInsets());

    [tabHistoryTableViewContainer_ setFrame:frame];
    [collectionView setFrame:[tabHistoryTableViewContainer_ bounds]];

    [[self popupContainer] addSubview:tabHistoryTableViewContainer_];
    CGRect containerFrame = [[self popupContainer] frame];
    CGPoint destination = CGPointMake(CGRectGetLeadingEdge(containerFrame),
                                      CGRectGetMinY(containerFrame));
    [self fadeInPopupFromSource:origin toDestination:destination];
  }
  return self;
}

- (void)fadeInPopupFromSource:(CGPoint)source
                toDestination:(CGPoint)destination {
  [tabHistoryTableViewContainer_ setAlpha:0];
  [UIView animateWithDuration:ios::material::kDuration1
                   animations:^{
                     [tabHistoryTableViewContainer_ setAlpha:1];
                   }];
  [super fadeInPopupFromSource:source toDestination:destination];
}

- (void)dismissAnimatedWithCompletion:(void (^)(void))completion {
  [tabHistoryTableViewContainer_ setAlpha:0];
  [super dismissAnimatedWithCompletion:completion];
}

- (CGFloat)calculatePopupWidth:(NSArray*)entries {
  CGFloat maxWidth;

  // Determine the maximum width for the device and orientation.

  if (!IsIPadIdiom()) {
    UIInterfaceOrientation orientation =
        [[UIApplication sharedApplication] statusBarOrientation];
    // Phone in portrait has constant width.
    if (UIInterfaceOrientationIsPortrait(orientation))
      return kTabHistoryMinWidth;
    maxWidth = kTabHistoryMaxWidthLandscapePhone;
  } else {
    // On iPad use 85% of the available width.
    maxWidth = ui::AlignValueToUpperPixel(
        [UIApplication sharedApplication].keyWindow.frame.size.width * .85);
  }
  // Increase the width to fit the text to display but don't exceed maxWidth.
  CGFloat cellWidth = kTabHistoryMinWidth;
  UIFont* font = [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:16];
  for (CRWSessionEntry* sessionEntry in entries) {
    web::NavigationItem* item = sessionEntry.navigationItem;
    // TODO(rohitrao): Can this be replaced with GetTitleForDisplay()?
    NSString* cellText = item->GetTitle().empty()
                             ? base::SysUTF8ToNSString(item->GetURL().spec())
                             : base::SysUTF16ToNSString(item->GetTitle());
    CGFloat contentWidth = [cellText cr_pixelAlignedSizeWithFont:font].width +
                           kCellTextXCoordinate;

    // If contentWidth is larger than maxWidth, return maxWidth instead of
    // checking the rest of the entries.
    if (contentWidth > maxWidth)
      return maxWidth;
    if (contentWidth > cellWidth)
      cellWidth = contentWidth;
  }
  return cellWidth;
}

- (void)dealloc {
  [tabHistoryTableViewContainer_ removeFromSuperview];
}

@end
