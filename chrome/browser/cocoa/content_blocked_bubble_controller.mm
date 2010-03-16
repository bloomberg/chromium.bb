// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/content_blocked_bubble_controller.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/blocked_popup_container.h"
#import "chrome/browser/cocoa/content_settings_dialog_controller.h"
#import "chrome/browser/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/cocoa/info_bubble_view.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

namespace {

// Must match the tag of the unblock radio button in the xib files.
const int kAllowTag = 1;

// Must match the tag of the block radio button in the xib files.
const int kBlockTag = 2;

// Height of one link in the popup list.
const int kLinkHeight = 16;

// Space between two popup links.
const int kLinkPadding = 4;

// Space taken in total by one popup link.
const int kLinkLineHeight = kLinkHeight + kLinkPadding;

// Space between popup list and surrounding UI elements.
const int kLinkOuterPadding = 8;

}  // namespace

// If the bubble's tab contents get destroyed, tells the bubble about it.
class BubbleCloser : public NotificationObserver {
 public:
  BubbleCloser(ContentBlockedBubbleController* controller,
               TabContents* tab_contents)
      : controller_(controller) {
    registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents));
  }
  virtual ~BubbleCloser() {}
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
    DCHECK(source == Source<TabContents>([controller_ tabContents]));
    [controller_ setTabContents:NULL];
  }
 private:
  ContentBlockedBubbleController* controller_;  // weak, owns us
  NotificationRegistrar registrar_;
};


// Like |ReplaceStringPlaceholders(const string16&, const string16&, size_t*)|,
// but for a NSString formatString.
static NSString* ReplaceNSStringPlaceholders(NSString* formatString,
                                             const string16& a,
                                             size_t* offset) {
  return base::SysUTF16ToNSString(
        ReplaceStringPlaceholders(base::SysNSStringToUTF16(formatString),
                                  a,
                                  offset));
}


@interface ContentBlockedBubbleController(Private)
- (id)initWithType:(ContentSettingsType)settingsType
      parentWindow:(NSWindow*)parentWindow
        anchoredAt:(NSPoint)anchoredAt
              host:(const std::string&)host
       displayHost:(NSString*)displayHost
       tabContents:(TabContents*)tabContents
           profile:(Profile*)profile;
- (NSButton*)hyperlinkButtonWithFrame:(NSRect)frame
                                title:(NSString*)title
                                 icon:(NSImage*)icon
                       referenceFrame:(NSRect)referenceFrame;
- (void)initializeTitle;
- (void)initializeRadioGroup;
- (void)initializePopupList;
- (void)popupLinkClicked:(id)sender;
@end


@implementation ContentBlockedBubbleController

+ (ContentBlockedBubbleController*)showForType:(ContentSettingsType)settingsType
                                  parentWindow:(NSWindow*)parentWindow
                                    anchoredAt:(NSPoint)anchor
                                          host:(const std::string&)host
                                   displayHost:(NSString*)displayHost
                                   tabContents:(TabContents*)tabContents
                                       profile:(Profile*)profile {
  // Autoreleases itself on bubble close.
  return [[ContentBlockedBubbleController alloc] initWithType:settingsType
                                                  parentWindow:parentWindow
                                                    anchoredAt:anchor
                                                          host:host
                                                   displayHost:displayHost
                                                   tabContents:tabContents
                                                       profile:profile];
}

@synthesize tabContents = tabContents_;

- (id)initWithType:(ContentSettingsType)settingsType
      parentWindow:(NSWindow*)parentWindow
        anchoredAt:(NSPoint)anchoredAt
              host:(const std::string&)host
       displayHost:(NSString*)displayHost
       tabContents:(TabContents*)tabContents
           profile:(Profile*)profile {
  NSString* const nibPaths[CONTENT_SETTINGS_NUM_TYPES] = {
    @"ContentBlockedCookies",
    @"ContentBlockedImages",
    @"ContentBlockedJavaScript",
    @"ContentBlockedPlugins",
    @"ContentBlockedPopups",
  };
  DCHECK_EQ(arraysize(nibPaths),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  NSString* nibPath =
      [mac_util::MainAppBundle() pathForResource:nibPaths[settingsType]
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    parentWindow_ = parentWindow;
    anchor_ = anchoredAt;

    settingsType_ = settingsType;
    host_ = host;
    displayHost_.reset([displayHost retain]);
    tabContents_ = tabContents;
    profile_ = profile;
    closer_.reset(new BubbleCloser(self, tabContents_));

    // Watch to see if the parent window closes, and if so, close this one.
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(parentWindowWillClose:)
                   name:NSWindowWillCloseNotification
                 object:parentWindow_];
  }
  return self;
}

- (void)initializeTitle {
  // Layout title post-localization.
  CGFloat titleDeltaY = [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:titleLabel_];
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += titleDeltaY;
  [[self window] setFrame:windowFrame display:NO];
  NSRect titleFrame = [titleLabel_ frame];
  titleFrame.origin.y -= titleDeltaY;
  [titleLabel_ setFrame:titleFrame];
}

- (void)initializeRadioGroup {
  // Select appropriate radio button..
  if (profile_) {  // NULL in tests.
    HostContentSettingsMap* contentSettingsMap =
        profile_->GetHostContentSettingsMap();
    ContentSetting setting = contentSettingsMap->GetContentSetting(
        host_, settingsType_);
    [allowBlockRadioGroup_ selectCellWithTag:
        setting == CONTENT_SETTING_ALLOW ? kAllowTag : kBlockTag];
  }

  // Copy |host_| into radio group label.
  NSCell* radioCell = [allowBlockRadioGroup_ cellWithTag:kAllowTag];
  [radioCell setTitle:ReplaceNSStringPlaceholders([radioCell title],
                                                  UTF8ToUTF16(host_),
                                                  NULL)];

 // Layout radio group labels post-localization.
  [GTMUILocalizerAndLayoutTweaker
      wrapRadioGroupForWidth:allowBlockRadioGroup_];
  CGFloat radioDeltaY = [GTMUILocalizerAndLayoutTweaker
      sizeToFitView:allowBlockRadioGroup_].height;
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += radioDeltaY;
  [[self window] setFrame:windowFrame display:NO];
}

- (NSButton*)hyperlinkButtonWithFrame:(NSRect)frame
                                title:(NSString*)title
                                 icon:(NSImage*)icon
                       referenceFrame:(NSRect)referenceFrame {
  scoped_nsobject<HyperlinkButtonCell> cell([[HyperlinkButtonCell alloc]
      initTextCell:title]);
  [cell.get() setAlignment:NSNaturalTextAlignment];
  if (icon) {
    [cell.get() setImagePosition:NSImageLeft];
    [cell.get() setImage:icon];
  } else {
    [cell.get() setImagePosition:NSNoImage];
  }
  [cell.get() setControlSize:NSSmallControlSize];

  NSButton* button = [[[NSButton alloc] initWithFrame:frame] autorelease];
  // Cell must be set immediately after construction.
  [button setCell:cell.get()];

  // If the link text is too long, clamp it.
  [button sizeToFit];
  int maxWidth = NSWidth([bubble_ frame]) - 2 * NSMinX(referenceFrame);
  NSRect buttonFrame = [button frame];
  if (NSWidth(buttonFrame) > maxWidth) {
    buttonFrame.size.width = maxWidth;
    [button setFrame:buttonFrame];
  }

  [button setTarget:self];
  [button setAction:@selector(popupLinkClicked:)];
  return button;
}

- (void)initializePopupList {
  // I didn't put the buttons into a NSMatrix because then they are only one
  // entity in the key view loop. This way, one can tab through all of them.
  BlockedPopupContainer::BlockedContents blockedContents;
  DCHECK(tabContents_->blocked_popup_container());
  tabContents_->blocked_popup_container()->GetBlockedContents(
      &blockedContents);

  // Get the pre-resize frame of the radio group. Its origin is where the
  // popup list should go.
  NSRect radioFrame = [allowBlockRadioGroup_ frame];

  // Make room for the popup list. The bubble view and its subviews autosize
  // themselves when the window is enlarged.
  // Heading and radio box are already 1 * kLinkOuterPadding apart in the nib,
  // so only 1 * kLinkOuterPadding more is needed.
  int delta = blockedContents.size() * kLinkLineHeight - kLinkPadding +
              kLinkOuterPadding;
  NSSize deltaSize = NSMakeSize(0, delta);
  deltaSize = [[[self window] contentView] convertSize:deltaSize toView:nil];
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += deltaSize.height;
  [[self window] setFrame:windowFrame display:NO];

  // Create popup list.
  int topLinkY = NSMaxY(radioFrame) + delta - kLinkHeight;
  int row = 0;
  for (BlockedPopupContainer::BlockedContents::const_iterator
           it(blockedContents.begin()); it != blockedContents.end();
       ++it, ++row) {
    SkBitmap icon = (*it)->GetFavIcon();
    NSImage* image = nil;
    if (!icon.empty())
      image = gfx::SkBitmapToNSImage(icon);

    string16 title((*it)->GetTitle());
    // The popup may not have committed a load yet, in which case it won't
    // have a URL or title.
    if (title.empty())
      title = l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);

    NSRect linkFrame =
        NSMakeRect(NSMinX(radioFrame), topLinkY - kLinkLineHeight * row,
                   200, kLinkHeight);
    NSButton* button = [self
        hyperlinkButtonWithFrame:linkFrame
                           title:base::SysUTF16ToNSString(title)
                            icon:image
                  referenceFrame:radioFrame];
    [bubble_ addSubview:button];
    popupLinks_[button] = *it;
  }
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);

  [bubble_ setBubbleType:kWhiteInfoBubble];
  [bubble_ setArrowLocation:kTopRight];

  [self initializeTitle];
  if (allowBlockRadioGroup_)  // not bound in cookie bubble xib
    [self initializeRadioGroup];
  if (settingsType_ == CONTENT_SETTINGS_TYPE_POPUPS && tabContents_)
    [self initializePopupList];
}

///////////////////////////////////////////////////////////////////////////////
// Bubble-management related stuff

// TODO(thakis): All that junk below should be in some superclass that all the
// bubble controllers (bookmark bubble, extension installed bubble, page/browser
// action bubble, content blocked bubble) derive from -- http://crbug.com/36366

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)parentWindowWillClose:(NSNotification*)notification {
  [self close];
}

- (void)windowWillClose:(NSNotification*)notification {
  // We caught a close so we don't need to watch for the parent closing.
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self autorelease];
}

// We want this to be a child of a browser window.  addChildWindow:
// (called from this function) will bring the window on-screen;
// unfortunately, [NSWindowController showWindow:] will also bring it
// on-screen (but will cause unexpected changes to the window's
// position).  We cannot have an addChildWindow: and a subsequent
// showWindow:. Thus, we have our own version.
- (void)showWindow:(id)sender {
  NSWindow* window = [self window];  // completes nib load

  NSPoint origin = anchor_;
  NSSize offsets = NSMakeSize(kBubbleArrowXOffset + kBubbleArrowWidth / 2.0,
                              0);
  offsets = [[parentWindow_ contentView] convertSize:offsets toView:nil];
  origin.x -= NSWidth([window frame]) - offsets.width;
  origin.y -= NSHeight([window frame]);
  [window setFrameOrigin:origin];
  [parentWindow_ addChildWindow:window ordered:NSWindowAbove];
  [window makeKeyAndOrderFront:self];
}

- (void)close {
  [parentWindow_ removeChildWindow:[self window]];
  [super close];
}

// The controller is the delegate of the window so it receives did resign key
// notifications. When key is resigned mirror Windows behavior and close the
// window.
- (void)windowDidResignKey:(NSNotification*)notification {
  NSWindow* window = [self window];
  DCHECK_EQ([notification object], window);
  if ([window isVisible]) {
    // If the window isn't visible, it is already closed, and this notification
    // has been sent as part of the closing operation, so no need to close.
    [self close];
  }
}

// By implementing this, ESC causes the window to go away.
- (IBAction)cancel:(id)sender {
  // This is not a "real" cancel as potential changes to the radio group are not
  // undone. That's ok.
  [self close];
}

///////////////////////////////////////////////////////////////////////////////
// Actual application logic

- (IBAction)allowBlockToggled:(id)sender {
  NSButtonCell *selectedCell = [sender selectedCell];
  ContentSetting setting = [selectedCell tag] == kAllowTag ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetHostContentSettingsMap()->SetContentSetting(host_,
                                                           settingsType_,
                                                           setting);
}

- (IBAction)closeBubble:(id)sender {
  [self close];
}

- (IBAction)manageBlocking:(id)sender {
  if (tabContents_) {
    tabContents_->delegate()->ShowContentSettingsWindow(settingsType_);
  } else {
    [ContentSettingsDialogController showContentSettingsForType:settingsType_
                                                        profile:profile_];
  }
}

- (void)popupLinkClicked:(id)sender {
  content_blocked_bubble::PopupLinks::iterator i(popupLinks_.find(sender));
  DCHECK(i != popupLinks_.end());
  if (tabContents_ && tabContents_->blocked_popup_container())
    tabContents_->blocked_popup_container()->LaunchPopupForContents(i->second);
}

@end  // ContentBlockedBubbleController
