// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/collected_cookies_mac.h"

#include <vector>

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#import "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#import "chrome/browser/ui/cocoa/vertical_gradient_view.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/apple/ImageAndTextCell.h"
#include "third_party/skia/include/core/SkBitmap.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

namespace {
// Colors for the infobar.
const double kBannerGradientColorTop[3] =
    {255.0 / 255.0, 242.0 / 255.0, 183.0 / 255.0};
const double kBannerGradientColorBottom[3] =
    {250.0 / 255.0, 230.0 / 255.0, 145.0 / 255.0};
const double kBannerStrokeColor = 135.0 / 255.0;

// Minimal height for the collected cookies dialog.
const CGFloat kMinCollectedCookiesViewHeight = 116;
} // namespace

#pragma mark Bridge between the constrained window delegate and the sheet

// The delegate used to forward the events from the sheet to the constrained
// window delegate.
@interface CollectedCookiesSheetBridge : NSObject {
  CollectedCookiesMac* collectedCookies_;  // weak
}
- (id)initWithCollectedCookiesMac:(CollectedCookiesMac*)collectedCookies;
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
@end

@implementation CollectedCookiesSheetBridge
- (id)initWithCollectedCookiesMac:(CollectedCookiesMac*)collectedCookies {
  if ((self = [super init])) {
    collectedCookies_ = collectedCookies;
  }
  return self;
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  collectedCookies_->OnSheetDidEnd(sheet);
}
@end

#pragma mark Constrained window delegate

CollectedCookiesMac::CollectedCookiesMac(NSWindow* parent,
                                         TabContents* tab_contents)
    : ConstrainedWindowMacDelegateCustomSheet(
        [[[CollectedCookiesSheetBridge alloc]
            initWithCollectedCookiesMac:this] autorelease],
        @selector(sheetDidEnd:returnCode:contextInfo:)),
      tab_contents_(tab_contents) {
  TabSpecificContentSettings* content_settings =
      tab_contents->GetTabSpecificContentSettings();
  registrar_.Add(this, NotificationType::COLLECTED_COOKIES_SHOWN,
                 Source<TabSpecificContentSettings>(content_settings));

  sheet_controller_ = [[CollectedCookiesWindowController alloc]
      initWithTabContents:tab_contents];

  set_sheet([sheet_controller_ window]);

  window_ = tab_contents->CreateConstrainedDialog(this);
}

CollectedCookiesMac::~CollectedCookiesMac() {
  NSWindow* window = [sheet_controller_ window];
  if (window_ && window && is_sheet_open()) {
    window_ = NULL;
    [NSApp endSheet:window];
  }
}

void CollectedCookiesMac::DeleteDelegate() {
  delete this;
}

void CollectedCookiesMac::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  DCHECK(type == NotificationType::COLLECTED_COOKIES_SHOWN);
  DCHECK_EQ(Source<TabSpecificContentSettings>(source).ptr(),
            tab_contents_->GetTabSpecificContentSettings());
  window_->CloseConstrainedWindow();
}

void CollectedCookiesMac::OnSheetDidEnd(NSWindow* sheet) {
  [sheet orderOut:sheet_controller_];
  if (window_)
    window_->CloseConstrainedWindow();
}

#pragma mark Window Controller

@interface CollectedCookiesWindowController(Private)
-(void)showInfoBarForDomain:(const string16&)domain
                    setting:(ContentSetting)setting;
-(void)showInfoBarForMultipleDomainsAndSetting:(ContentSetting)setting;
-(void)animateInfoBar;
@end

@implementation CollectedCookiesWindowController

@synthesize allowedCookiesButtonsEnabled =
    allowedCookiesButtonsEnabled_;
@synthesize blockedCookiesButtonsEnabled =
    blockedCookiesButtonsEnabled_;

@synthesize allowedTreeController = allowedTreeController_;
@synthesize blockedTreeController = blockedTreeController_;

- (id)initWithTabContents:(TabContents*)tabContents {
  DCHECK(tabContents);
  infoBarVisible_ = NO;
  tabContents_ = tabContents;

  NSString* nibpath =
      [base::mac::MainAppBundle() pathForResource:@"CollectedCookies"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    [self loadTreeModelFromTabContents];

    animation_.reset([[NSViewAnimation alloc] init]);
    [animation_ setAnimationBlockingMode:NSAnimationNonblocking];
  }
  return self;
}

- (void)awakeFromNib {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* infoIcon = rb.GetNativeImageNamed(IDR_INFO);
  DCHECK(infoIcon);
  [infoBarIcon_ setImage:infoIcon];

  // Initialize the banner gradient and stroke color.
  NSColor* bannerStartingColor =
      [NSColor colorWithCalibratedRed:kBannerGradientColorTop[0]
                                green:kBannerGradientColorTop[1]
                                 blue:kBannerGradientColorTop[2]
                                alpha:1.0];
  NSColor* bannerEndingColor =
      [NSColor colorWithCalibratedRed:kBannerGradientColorBottom[0]
                                green:kBannerGradientColorBottom[1]
                                 blue:kBannerGradientColorBottom[2]
                                alpha:1.0];
  scoped_nsobject<NSGradient> bannerGradient(
      [[NSGradient alloc] initWithStartingColor:bannerStartingColor
                                    endingColor:bannerEndingColor]);
  [infoBar_ setGradient:bannerGradient];

  NSColor* bannerStrokeColor =
      [NSColor colorWithCalibratedWhite:kBannerStrokeColor
                                  alpha:1.0];
  [infoBar_ setStrokeColor:bannerStrokeColor];

  // Change the label of the blocked cookies part if necessary.
  if (tabContents_->profile()->GetHostContentSettingsMap()->
      BlockThirdPartyCookies()) {
    [blockedCookiesText_ setStringValue:l10n_util::GetNSString(
        IDS_COLLECTED_COOKIES_BLOCKED_THIRD_PARTY_BLOCKING_ENABLED)];
    CGFloat textDeltaY = [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:blockedCookiesText_];

    // Shrink the upper custom view.
    NSView* upperContentView = [[splitView_ subviews] objectAtIndex:0];
    NSRect frame = [upperContentView frame];
    [splitView_ setPosition:(frame.size.height - textDeltaY/2.0)
           ofDividerAtIndex:0];

    // Shrink the lower outline view.
    frame = [lowerScrollView_ frame];
    frame.size.height -= textDeltaY;
    [lowerScrollView_ setFrame:frame];

    // Move the label down so it actually fits.
    frame = [blockedCookiesText_ frame];
    frame.origin.y -= textDeltaY;
    [blockedCookiesText_ setFrame:frame];
  }
}

- (void)windowWillClose:(NSNotification*)notif {
  [allowedOutlineView_ setDelegate:nil];
  [blockedOutlineView_ setDelegate:nil];
  [animation_ stopAnimation];
  [self autorelease];
}

- (IBAction)closeSheet:(id)sender {
  [NSApp endSheet:[self window]];
}

- (void)addException:(ContentSetting)setting
   forTreeController:(NSTreeController*)controller {
  NSArray* nodes = [controller selectedNodes];
  BOOL multipleDomainsChanged = NO;
  string16 lastDomain;
  for (NSTreeNode* treeNode in nodes) {
    CocoaCookieTreeNode* node = [treeNode representedObject];
    CookieTreeNode* cookie = static_cast<CookieTreeNode*>([node treeNode]);
    if (cookie->GetDetailedInfo().node_type !=
        CookieTreeNode::DetailedInfo::TYPE_ORIGIN) {
      continue;
    }
    CookieTreeOriginNode* origin_node =
        static_cast<CookieTreeOriginNode*>(cookie);
    origin_node->CreateContentException(
        tabContents_->profile()->GetHostContentSettingsMap(),
        setting);
    if (!lastDomain.empty())
      multipleDomainsChanged = YES;
    lastDomain = origin_node->GetTitle();
  }
  if (multipleDomainsChanged)
    [self showInfoBarForMultipleDomainsAndSetting:setting];
  else
    [self showInfoBarForDomain:lastDomain setting:setting];
}

- (IBAction)allowOrigin:(id)sender {
  [self    addException:CONTENT_SETTING_ALLOW
      forTreeController:blockedTreeController_];
}

- (IBAction)allowForSessionFromOrigin:(id)sender {
  [self    addException:CONTENT_SETTING_SESSION_ONLY
      forTreeController:blockedTreeController_];
}

- (IBAction)blockOrigin:(id)sender {
  [self    addException:CONTENT_SETTING_BLOCK
      forTreeController:allowedTreeController_];
}

- (CGFloat)    splitView:(NSSplitView *)sender
  constrainMinCoordinate:(CGFloat)proposedMin
             ofSubviewAt:(NSInteger)offset {
  return proposedMin + kMinCollectedCookiesViewHeight;
}
- (CGFloat)    splitView:(NSSplitView *)sender
  constrainMaxCoordinate:(CGFloat)proposedMax
             ofSubviewAt:(NSInteger)offset {
  return proposedMax - kMinCollectedCookiesViewHeight;
}
- (BOOL)splitView:(NSSplitView *)sender canCollapseSubview:(NSView *)subview {
  return YES;
}

- (CocoaCookieTreeNode*)cocoaAllowedTreeModel {
  return cocoaAllowedTreeModel_.get();
}
- (void)setCocoaAllowedTreeModel:(CocoaCookieTreeNode*)model {
  cocoaAllowedTreeModel_.reset([model retain]);
}

- (CookiesTreeModel*)allowedTreeModel {
  return allowedTreeModel_.get();
}

- (CocoaCookieTreeNode*)cocoaBlockedTreeModel {
  return cocoaBlockedTreeModel_.get();
}
- (void)setCocoaBlockedTreeModel:(CocoaCookieTreeNode*)model {
  cocoaBlockedTreeModel_.reset([model retain]);
}

- (CookiesTreeModel*)blockedTreeModel {
  return blockedTreeModel_.get();
}

- (void)outlineView:(NSOutlineView*)outlineView
    willDisplayCell:(id)cell
     forTableColumn:(NSTableColumn*)tableColumn
               item:(id)item {
  CocoaCookieTreeNode* node = [item representedObject];
  int index;
  if (outlineView == allowedOutlineView_)
    index = allowedTreeModel_->GetIconIndex([node treeNode]);
  else
    index = blockedTreeModel_->GetIconIndex([node treeNode]);
  NSImage* icon = nil;
  if (index >= 0)
    icon = [icons_ objectAtIndex:index];
  else
    icon = [icons_ lastObject];
  DCHECK([cell isKindOfClass:[ImageAndTextCell class]]);
  [static_cast<ImageAndTextCell*>(cell) setImage:icon];
}

- (void)outlineViewSelectionDidChange:(NSNotification*)notif {
  BOOL isAllowedOutlineView;
  if ([notif object] == allowedOutlineView_) {
    isAllowedOutlineView = YES;
  } else if ([notif object] == blockedOutlineView_) {
    isAllowedOutlineView = NO;
  } else {
    NOTREACHED();
    return;
  }
  NSTreeController* controller =
      isAllowedOutlineView ? allowedTreeController_ : blockedTreeController_;

  NSArray* nodes = [controller selectedNodes];
  for (NSTreeNode* treeNode in nodes) {
    CocoaCookieTreeNode* node = [treeNode representedObject];
    CookieTreeNode* cookie = static_cast<CookieTreeNode*>([node treeNode]);
    if (cookie->GetDetailedInfo().node_type !=
        CookieTreeNode::DetailedInfo::TYPE_ORIGIN) {
      continue;
    }
   CookieTreeOriginNode* origin_node =
       static_cast<CookieTreeOriginNode*>(cookie);
   if (origin_node->CanCreateContentException()) {
      if (isAllowedOutlineView) {
        [self setAllowedCookiesButtonsEnabled:YES];
      } else {
        [self setBlockedCookiesButtonsEnabled:YES];
      }
      return;
    }
  }
  if (isAllowedOutlineView) {
    [self setAllowedCookiesButtonsEnabled:NO];
  } else {
    [self setBlockedCookiesButtonsEnabled:NO];
  }
}

// Initializes the |allowedTreeModel_| and |blockedTreeModel_|, and builds
// the |cocoaAllowedTreeModel_| and |cocoaBlockedTreeModel_|.
- (void)loadTreeModelFromTabContents {
  TabSpecificContentSettings* content_settings =
      tabContents_->GetTabSpecificContentSettings();
  allowedTreeModel_.reset(content_settings->GetAllowedCookiesTreeModel());
  blockedTreeModel_.reset(content_settings->GetBlockedCookiesTreeModel());

  // Convert the model's icons from Skia to Cocoa.
  std::vector<SkBitmap> skiaIcons;
  allowedTreeModel_->GetIcons(&skiaIcons);
  icons_.reset([[NSMutableArray alloc] init]);
  for (std::vector<SkBitmap>::iterator it = skiaIcons.begin();
       it != skiaIcons.end(); ++it) {
    [icons_ addObject:gfx::SkBitmapToNSImage(*it)];
  }

  // Default icon will be the last item in the array.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  // TODO(rsesek): Rename this resource now that it's in multiple places.
  [icons_ addObject:rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_FOLDER)];

  // Create the Cocoa model.
  CookieTreeNode* root =
      static_cast<CookieTreeNode*>(allowedTreeModel_->GetRoot());
  scoped_nsobject<CocoaCookieTreeNode> model(
      [[CocoaCookieTreeNode alloc] initWithNode:root]);
  [self setCocoaAllowedTreeModel:model.get()];  // Takes ownership.
  root = static_cast<CookieTreeNode*>(blockedTreeModel_->GetRoot());
  model.reset(
      [[CocoaCookieTreeNode alloc] initWithNode:root]);
  [self setCocoaBlockedTreeModel:model.get()];  // Takes ownership.
}

-(void)showInfoBarForMultipleDomainsAndSetting:(ContentSetting)setting {
  NSString* label;
  switch (setting) {
    case CONTENT_SETTING_BLOCK:
      label = l10n_util::GetNSString(
          IDS_COLLECTED_COOKIES_MULTIPLE_BLOCK_RULES_CREATED);
      break;

    case CONTENT_SETTING_ALLOW:
      label = l10n_util::GetNSString(
          IDS_COLLECTED_COOKIES_MULTIPLE_ALLOW_RULES_CREATED);
      break;

    case CONTENT_SETTING_SESSION_ONLY:
      label = l10n_util::GetNSString(
          IDS_COLLECTED_COOKIES_MULTIPLE_SESSION_RULES_CREATED);
      break;

    default:
      NOTREACHED();
      label = [[[NSString alloc] init] autorelease];
  }
  [infoBarText_ setStringValue:label];
  [self animateInfoBar];
}

-(void)showInfoBarForDomain:(const string16&)domain
                    setting:(ContentSetting)setting {
  NSString* label;
  switch (setting) {
    case CONTENT_SETTING_BLOCK:
      label = l10n_util::GetNSStringF(
          IDS_COLLECTED_COOKIES_BLOCK_RULE_CREATED,
          domain);
      break;

    case CONTENT_SETTING_ALLOW:
      label = l10n_util::GetNSStringF(
          IDS_COLLECTED_COOKIES_ALLOW_RULE_CREATED,
          domain);
      break;

    case CONTENT_SETTING_SESSION_ONLY:
      label = l10n_util::GetNSStringF(
          IDS_COLLECTED_COOKIES_SESSION_RULE_CREATED,
          domain);
      break;

    default:
      NOTREACHED();
      label = [[[NSString alloc] init] autorelease];
  }
  [infoBarText_ setStringValue:label];
  [self animateInfoBar];
}

-(void)animateInfoBar {
  if (infoBarVisible_)
    return;

  infoBarVisible_ = YES;

  NSMutableArray* animations = [NSMutableArray arrayWithCapacity:3];

  NSWindow* sheet = [self window];
  NSRect sheetFrame = [sheet frame];
  NSRect infoBarFrame = [infoBar_ frame];
  NSRect splitViewFrame = [splitView_ frame];

  // Calculate the end position of the info bar and set it to its start
  // position.
  infoBarFrame.origin.y = NSHeight(sheetFrame);
  infoBarFrame.size.width = NSWidth(sheetFrame);
  NSRect infoBarStartFrame = infoBarFrame;
  infoBarStartFrame.origin.y += NSHeight(infoBarFrame);
  infoBarStartFrame.size.height = 0.0;
  [infoBar_ setFrame:infoBarStartFrame];
  [[[self window] contentView] addSubview:infoBar_];

  // Calculate the new position of the sheet.
  sheetFrame.origin.y -= NSHeight(infoBarFrame);
  sheetFrame.size.height += NSHeight(infoBarFrame);

  // Slide the infobar in.
  [animations addObject:
      [NSDictionary dictionaryWithObjectsAndKeys:
          infoBar_, NSViewAnimationTargetKey,
          [NSValue valueWithRect:infoBarFrame],
              NSViewAnimationEndFrameKey,
          [NSValue valueWithRect:infoBarStartFrame],
              NSViewAnimationStartFrameKey,
          nil]];
  // Make sure the split view ends up in the right position.
  [animations addObject:
      [NSDictionary dictionaryWithObjectsAndKeys:
          splitView_, NSViewAnimationTargetKey,
          [NSValue valueWithRect:splitViewFrame],
              NSViewAnimationEndFrameKey,
          nil]];

  // Grow the sheet.
  [animations addObject:
      [NSDictionary dictionaryWithObjectsAndKeys:
          sheet, NSViewAnimationTargetKey,
          [NSValue valueWithRect:sheetFrame],
              NSViewAnimationEndFrameKey,
          nil]];
  [animation_ setViewAnimations:animations];
  // The default duration is 0.5s, which actually feels slow in here, so speed
  // it up a bit.
  [animation_ gtm_setDuration:0.2
                    eventMask:NSLeftMouseUpMask];
  [animation_ startAnimation];
}

@end
