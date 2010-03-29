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
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/host_content_settings_map.h"
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

// Height of each of the labels in the geolocation bubble.
const int kGeoLabelHeight = 14;

// Height of the "Clear" button in the geolocation bubble.
const int kGeoClearButtonHeight = 17;

// General padding between elements in the geolocation bubble.
const int kGeoPadding = 8;

// Padding between host names in the geolocation bubble.
const int kGeoHostPadding = 4;


// Like |ReplaceStringPlaceholders(const string16&, const string16&, size_t*)|,
// but for a NSString formatString.
NSString* ReplaceNSStringPlaceholders(NSString* formatString,
                                      const string16& a,
                                      size_t* offset) {
  return base::SysUTF16ToNSString(
      ReplaceStringPlaceholders(base::SysNSStringToUTF16(formatString),
                                a,
                                offset));
}

void SetControlSize(NSControl* control, NSControlSize controlSize) {
  CGFloat fontSize = [NSFont systemFontSizeForControlSize:controlSize];
  NSCell* cell = [control cell];
  NSFont* font = [NSFont fontWithName:[[cell font] fontName] size:fontSize];
  [cell setFont:font];
  [cell setControlSize:controlSize];
}

// Returns an autoreleased NSTextField that is configured to look like a Label
// looks in Interface Builder.
NSTextField* LabelWithFrame(NSString* text, const NSRect& frame) {
  NSTextField* label = [[NSTextField alloc] initWithFrame:frame];
  [label setStringValue:text];
  [label setSelectable:NO];
  [label setBezeled:NO];
  return [label autorelease];
}

}  // namespace

@interface ContentBlockedBubbleController(Private)
- (id)initWithModel:(ContentSettingBubbleModel*)settingsBubbleModel
       parentWindow:(NSWindow*)parentWindow
         anchoredAt:(NSPoint)anchoredAt;
- (NSButton*)hyperlinkButtonWithFrame:(NSRect)frame
                                title:(NSString*)title
                                 icon:(NSImage*)icon
                       referenceFrame:(NSRect)referenceFrame;
- (void)initializeTitle;
- (void)initializeRadioGroup;
- (void)initializePopupList;
- (void)initializeGeoLists;
- (void)popupLinkClicked:(id)sender;
- (void)clearGeolocationForCurrentHost:(id)sender;
@end

@implementation ContentBlockedBubbleController

+ (ContentBlockedBubbleController*)
    showForModel:(ContentSettingBubbleModel*)contentSettingBubbleModel
    parentWindow:(NSWindow*)parentWindow
      anchoredAt:(NSPoint)anchor {
  // Autoreleases itself on bubble close.
  return [[ContentBlockedBubbleController alloc]
             initWithModel:contentSettingBubbleModel
              parentWindow:parentWindow
                anchoredAt:anchor];
}

- (id)initWithModel:(ContentSettingBubbleModel*)contentSettingBubbleModel
       parentWindow:(NSWindow*)parentWindow
         anchoredAt:(NSPoint)anchoredAt {
  // This method takes ownership of |contentSettingBubbleModel| in all cases.
  scoped_ptr<ContentSettingBubbleModel> model(contentSettingBubbleModel);
  DCHECK(model.get());

  NSString* const nibPaths[CONTENT_SETTINGS_NUM_TYPES] = {
    @"ContentBlockedCookies",
    @"ContentBlockedImages",
    @"ContentBlockedJavaScript",
    @"ContentBlockedPlugins",
    @"ContentBlockedPopups",
    @"ContentBubbleGeolocation",
  };
  COMPILE_ASSERT(arraysize(nibPaths) == CONTENT_SETTINGS_NUM_TYPES,
                 nibPaths_requires_an_entry_for_every_setting_type);
  const int settingsType = model->content_type();
  DCHECK_LT(settingsType, CONTENT_SETTINGS_NUM_TYPES);
  NSString* nibPath =
      [mac_util::MainAppBundle() pathForResource:nibPaths[settingsType]
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    parentWindow_ = parentWindow;
    anchor_ = anchoredAt;
    contentSettingBubbleModel_.reset(model.release());

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
  if (!titleLabel_)
    return;

  // Layout title post-localization.
  CGFloat deltaY = [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:titleLabel_];
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += deltaY;
  [[self window] setFrame:windowFrame display:NO];
  NSRect titleFrame = [titleLabel_ frame];
  titleFrame.origin.y -= deltaY;
  [titleLabel_ setFrame:titleFrame];
}

- (void)initializeRadioGroup {
  // Configure the radio group. For now, only deal with the
  // strictly needed case of 1 radio group (containing 2 radio buttons).
  // TODO(joth): Implement the generic case, getting localized strings from the
  // bubble model instead of the xib, or remove it if it's never needed.
  // http://crbug.com/38432
  const ContentSettingBubbleModel::RadioGroups& radioGroups =
      contentSettingBubbleModel_->bubble_content().radio_groups;
  DCHECK_EQ(1u, radioGroups.size()) << "Only one radio group supported";
  const ContentSettingBubbleModel::RadioGroup& radioGroup = radioGroups.at(0);

  // Select appropriate radio button..
  [allowBlockRadioGroup_ selectCellWithTag:
      radioGroup.default_item == 0 ? kAllowTag : kBlockTag];

  // Copy |host_| into radio group label.
  NSCell* radioCell = [allowBlockRadioGroup_ cellWithTag:kAllowTag];
  [radioCell setTitle:ReplaceNSStringPlaceholders([radioCell title],
                                                  UTF8ToUTF16(radioGroup.host),
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
  const ContentSettingBubbleModel::PopupItems& popupItems =
      contentSettingBubbleModel_->bubble_content().popup_items;

  // Get the pre-resize frame of the radio group. Its origin is where the
  // popup list should go.
  NSRect radioFrame = [allowBlockRadioGroup_ frame];

  // Make room for the popup list. The bubble view and its subviews autosize
  // themselves when the window is enlarged.
  // Heading and radio box are already 1 * kLinkOuterPadding apart in the nib,
  // so only 1 * kLinkOuterPadding more is needed.
  int delta = popupItems.size() * kLinkLineHeight - kLinkPadding +
              kLinkOuterPadding;
  NSSize deltaSize = NSMakeSize(0, delta);
  deltaSize = [[[self window] contentView] convertSize:deltaSize toView:nil];
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += deltaSize.height;
  [[self window] setFrame:windowFrame display:NO];

  // Create popup list.
  int topLinkY = NSMaxY(radioFrame) + delta - kLinkHeight;
  int row = 0;
  for (std::vector<ContentSettingBubbleModel::PopupItem>::const_iterator
       it(popupItems.begin()); it != popupItems.end(); ++it, ++row) {
    const SkBitmap& icon = it->bitmap;
    NSImage* image = nil;
    if (!icon.empty())
      image = gfx::SkBitmapToNSImage(icon);

    std::string title(it->title);
    // The popup may not have committed a load yet, in which case it won't
    // have a URL or title.
    if (title.empty())
      title = l10n_util::GetStringUTF8(IDS_TAB_LOADING_TITLE);

    NSRect linkFrame =
        NSMakeRect(NSMinX(radioFrame), topLinkY - kLinkLineHeight * row,
                   200, kLinkHeight);
    NSButton* button = [self
        hyperlinkButtonWithFrame:linkFrame
                           title:base::SysUTF8ToNSString(title)
                            icon:image
                  referenceFrame:radioFrame];
    [bubble_ addSubview:button];
    popupLinks_[button] = row;
  }
}

- (void)initializeGeoLists {
  // Cocoa has its origin in the lower left corner. This means elements are
  // added from bottom to top, which explains why loops run backwards and the
  // order of operations is the other way than on Linux/Windows.
  const ContentSettingBubbleModel::BubbleContent& content =
      contentSettingBubbleModel_->bubble_content();
  NSRect containerFrame = [contentsContainer_ frame];
  NSRect frame = NSMakeRect(0, 0, containerFrame.size.width, kGeoLabelHeight);

  // "Clear" button.
  if (!content.clear_link.empty()) {
    NSRect buttonFrame = NSMakeRect(0, 0,
                                    containerFrame.size.width,
                                    kGeoClearButtonHeight);
    NSButton* button = [[NSButton alloc] initWithFrame:buttonFrame];
    [button setTitle:base::SysUTF8ToNSString(content.clear_link)];
    [button setTarget:self];
    [button setAction:@selector(clearGeolocationForCurrentHost:)];
    [button setBezelStyle:NSRoundRectBezelStyle];
    SetControlSize(button, NSSmallControlSize);
    [button sizeToFit];
    [contentsContainer_ addSubview:button];

    frame.origin.y = NSMaxY([button frame]) + kGeoPadding;
  }

  typedef
      std::vector<ContentSettingBubbleModel::DomainList>::const_reverse_iterator
      GeolocationGroupIterator;
  for (GeolocationGroupIterator i = content.domain_lists.rbegin();
       i != content.domain_lists.rend(); ++i) {
    // Add all hosts in the current domain list.
    for (std::set<std::string>::const_reverse_iterator j = i->hosts.rbegin();
         j != i->hosts.rend(); ++j) {
      NSTextField* title = LabelWithFrame(base::SysUTF8ToNSString(*j), frame);
      SetControlSize(title, NSSmallControlSize);
      [contentsContainer_ addSubview:title];

      frame.origin.y = NSMaxY(frame) + kGeoHostPadding +
          [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:title];
    }
    if (!i->hosts.empty())
      frame.origin.y += kGeoPadding - kGeoHostPadding;

    // Add the domain list's title.
    NSTextField* title =
        LabelWithFrame(base::SysUTF8ToNSString(i->title), frame);
    SetControlSize(title, NSSmallControlSize);
    [contentsContainer_ addSubview:title];

    frame.origin.y = NSMaxY(frame) + kGeoPadding +
        [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:title];
  }

  CGFloat containerHeight = frame.origin.y;
  // Undo last padding.
  if (!content.domain_lists.empty())
    containerHeight -= kGeoPadding;

  // Resize container to fit its subviews, and window to fit the container.
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += containerHeight - containerFrame.size.height;
  [[self window] setFrame:windowFrame display:NO];
  containerFrame.size.height = containerHeight;
  [contentsContainer_ setFrame:containerFrame];
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK_EQ(self, [[self window] delegate]);

  [bubble_ setBubbleType:kWhiteInfoBubble];
  [bubble_ setArrowLocation:kTopRight];

  [self initializeTitle];
  if (allowBlockRadioGroup_)  // not bound in cookie bubble xib
    [self initializeRadioGroup];
  if (contentSettingBubbleModel_->content_type() ==
      CONTENT_SETTINGS_TYPE_POPUPS)
    [self initializePopupList];
  if (contentSettingBubbleModel_->content_type() ==
      CONTENT_SETTINGS_TYPE_GEOLOCATION)
    [self initializeGeoLists];
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
  contentSettingBubbleModel_->OnRadioClicked(
      0, [selectedCell tag] == kAllowTag ? 0 : 1);
}

- (IBAction)closeBubble:(id)sender {
  [self close];
}

- (IBAction)manageBlocking:(id)sender {
  contentSettingBubbleModel_->OnManageLinkClicked();
}

- (void)popupLinkClicked:(id)sender {
  content_blocked_bubble::PopupLinks::iterator i(popupLinks_.find(sender));
  DCHECK(i != popupLinks_.end());
  contentSettingBubbleModel_->OnPopupClicked(i->second);
}

- (void)clearGeolocationForCurrentHost:(id)sender {
  contentSettingBubbleModel_->OnClearLinkClicked();
  [self close];
}

@end  // ContentBlockedBubbleController
