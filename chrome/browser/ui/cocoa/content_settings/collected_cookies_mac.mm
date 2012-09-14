// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/collected_cookies_mac.h"

#include <vector>

#include "base/mac/bundle_locations.h"
#import "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_server_bound_cert_helper.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/cocoa/constrained_window_mac.h"
#import "chrome/browser/ui/cocoa/content_settings/cookie_details_view_controller.h"
#import "chrome/browser/ui/cocoa/vertical_gradient_view.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/apple_sample_code/ImageAndTextCell.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {
// Colors for the infobar.
const double kBannerGradientColorTop[3] =
    {255.0 / 255.0, 242.0 / 255.0, 183.0 / 255.0};
const double kBannerGradientColorBottom[3] =
    {250.0 / 255.0, 230.0 / 255.0, 145.0 / 255.0};
const double kBannerStrokeColor = 135.0 / 255.0;

enum TabViewItemIndices {
  kAllowedCookiesTabIndex = 0,
  kBlockedCookiesTabIndex
};

} // namespace

namespace chrome {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowCollectedCookiesDialog(TabContents* tab_contents) {
  // Deletes itself on close.
  new CollectedCookiesMac(
      tab_contents->web_contents()->GetView()->GetTopLevelNativeWindow(),
      tab_contents);
}

}  // namespace chrome

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
        @selector(sheetDidEnd:returnCode:contextInfo:)) {
  TabSpecificContentSettings* content_settings =
      tab_contents->content_settings();
  registrar_.Add(this, chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN,
                 content::Source<TabSpecificContentSettings>(content_settings));

  sheet_controller_ = [[CollectedCookiesWindowController alloc]
      initWithTabContents:tab_contents];

  set_sheet([sheet_controller_ window]);

  window_ = new ConstrainedWindowMac(tab_contents, this);
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

void CollectedCookiesMac::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN);
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

- (id)initWithTabContents:(TabContents*)tab_contents {
  DCHECK(tab_contents);

  NSString* nibpath =
      [base::mac::FrameworkBundle() pathForResource:@"CollectedCookies"
                                             ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    tab_contents_ = tab_contents;
    [self loadTreeModelFromTabContents];

    animation_.reset([[NSViewAnimation alloc] init]);
    [animation_ setAnimationBlockingMode:NSAnimationNonblocking];
  }
  return self;
}

- (void)awakeFromNib {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSImage* infoIcon = rb.GetNativeImageNamed(IDR_INFO).ToNSImage();
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
  Profile* profile = tab_contents_->profile();
  if (profile->GetPrefs()->GetBoolean(prefs::kBlockThirdPartyCookies)) {
    [blockedCookiesText_ setStringValue:l10n_util::GetNSString(
        IDS_COLLECTED_COOKIES_BLOCKED_THIRD_PARTY_BLOCKING_ENABLED)];
    CGFloat textDeltaY = [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:blockedCookiesText_];

    // Shrink the blocked cookies outline view.
    NSRect frame = [blockedScrollView_ frame];
    frame.size.height -= textDeltaY;
    [blockedScrollView_ setFrame:frame];

    // Move the label down so it actually fits.
    frame = [blockedCookiesText_ frame];
    frame.origin.y -= textDeltaY;
    [blockedCookiesText_ setFrame:frame];
  }

  detailsViewController_.reset([[CookieDetailsViewController alloc] init]);

  NSView* detailView = [detailsViewController_.get() view];
  NSRect viewFrameRect = [cookieDetailsViewPlaceholder_ frame];
  [[detailsViewController_.get() view] setFrame:viewFrameRect];
  [[cookieDetailsViewPlaceholder_ superview]
      replaceSubview:cookieDetailsViewPlaceholder_
                with:detailView];

  [self tabView:tabView_ didSelectTabViewItem:[tabView_ selectedTabViewItem]];
}

- (void)windowWillClose:(NSNotification*)notif {
  if (contentSettingsChanged_) {
    InfoBarTabHelper* infobar_helper = tab_contents_->infobar_tab_helper();
    infobar_helper->AddInfoBar(
        new CollectedCookiesInfoBarDelegate(infobar_helper));
  }
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
        CookieTreeNode::DetailedInfo::TYPE_HOST) {
      continue;
    }
    Profile* profile = tab_contents_->profile();
    CookieTreeHostNode* host_node =
        static_cast<CookieTreeHostNode*>(cookie);
    host_node->CreateContentException(
        CookieSettings::Factory::GetForProfile(profile), setting);
    if (!lastDomain.empty())
      multipleDomainsChanged = YES;
    lastDomain = host_node->GetTitle();
  }
  if (multipleDomainsChanged)
    [self showInfoBarForMultipleDomainsAndSetting:setting];
  else
    [self showInfoBarForDomain:lastDomain setting:setting];
  contentSettingsChanged_ = YES;
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
        CookieTreeNode::DetailedInfo::TYPE_HOST) {
      continue;
    }
   CookieTreeHostNode* host_node =
       static_cast<CookieTreeHostNode*>(cookie);
   if (host_node->CanCreateContentException()) {
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
      tab_contents_->content_settings();

  const LocalSharedObjectsContainer& allowed_data =
      content_settings->allowed_local_shared_objects();
  allowedTreeModel_ = allowed_data.CreateCookiesTreeModel();

  const LocalSharedObjectsContainer& blocked_data =
      content_settings->blocked_local_shared_objects();
  blockedTreeModel_ = blocked_data.CreateCookiesTreeModel();

  // Convert the model's icons from Skia to Cocoa.
  std::vector<gfx::ImageSkia> skiaIcons;
  allowedTreeModel_->GetIcons(&skiaIcons);
  icons_.reset([[NSMutableArray alloc] init]);
  for (std::vector<gfx::ImageSkia>::iterator it = skiaIcons.begin();
       it != skiaIcons.end(); ++it) {
    [icons_ addObject:gfx::NSImageFromImageSkia(*it)];
  }

  // Default icon will be the last item in the array.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  // TODO(rsesek): Rename this resource now that it's in multiple places.
  [icons_ addObject:
      rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_FOLDER).ToNSImage()];

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
  NSRect tabViewFrame = [tabView_ frame];

  // Calculate the end position of the info bar and set it to its start
  // position.
  infoBarFrame.origin.y = NSHeight(sheetFrame);
  infoBarFrame.size.width = NSWidth(sheetFrame);
  [infoBar_ setFrame:infoBarFrame];
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
          nil]];
  // Make sure the tab view ends up in the right position.
  [animations addObject:
      [NSDictionary dictionaryWithObjectsAndKeys:
          tabView_, NSViewAnimationTargetKey,
          [NSValue valueWithRect:tabViewFrame],
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

- (void)         tabView:(NSTabView*)tabView
    didSelectTabViewItem:(NSTabViewItem*)tabViewItem {
  NSTreeController* treeController = nil;
  switch ([tabView indexOfTabViewItem:tabViewItem]) {
    case kAllowedCookiesTabIndex:
      treeController = allowedTreeController_;
      break;
    case kBlockedCookiesTabIndex:
      treeController = blockedTreeController_;
      break;
    default:
      NOTREACHED();
      return;
  }
  [detailsViewController_ configureBindingsForTreeController:treeController];
}

@end
