// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/collected_cookies_mac.h"

#include <vector>

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#import "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/content_settings_dialog_controller.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/apple/ImageAndTextCell.h"
#include "third_party/skia/include/core/SkBitmap.h"

static const CGFloat kMinCollectedCookiesViewHeight = 116;

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

@implementation CollectedCookiesWindowController

@synthesize allowedCookiesButtonsEnabled =
    allowedCookiesButtonsEnabled_;
@synthesize blockedCookiesButtonsEnabled =
    blockedCookiesButtonsEnabled_;

@synthesize allowedTreeController = allowedTreeController_;
@synthesize blockedTreeController = blockedTreeController_;

- (id)initWithTabContents:(TabContents*)tabContents {
  DCHECK(tabContents);
  NSString* nibpath =
      [mac_util::MainAppBundle() pathForResource:@"CollectedCookies"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    tabContents_ = tabContents;

    [self loadTreeModelFromTabContents];
  }
  return self;
}

- (void)windowWillClose:(NSNotification*)notif {
  [allowedOutlineView_ setDelegate:nil];
  [blockedOutlineView_ setDelegate:nil];
  [self autorelease];
}

- (IBAction)closeSheet:(id)sender {
  [NSApp endSheet:[self window]];
}

- (void)addException:(ContentSetting)setting
   forTreeController:(NSTreeController*)controller {
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
    origin_node->CreateContentException(
        tabContents_->profile()->GetHostContentSettingsMap(),
        setting);
  }
  [[ContentSettingsDialogController
      showContentSettingsForType:CONTENT_SETTINGS_TYPE_COOKIES
                         profile:tabContents_->profile()]
      showCookieExceptions:self];
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
  [icons_ addObject:rb.GetNSImageNamed(IDR_BOOKMARK_BAR_FOLDER)];

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

@end
