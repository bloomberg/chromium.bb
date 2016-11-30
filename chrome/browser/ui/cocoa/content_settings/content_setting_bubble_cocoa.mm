// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_media_menu_model.h"
#import "chrome/browser/ui/cocoa/location_bar/content_setting_decoration.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/base/l10n/l10n_util.h"

using content::PluginService;

namespace {

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

// Minimal padding between "Manage" and "Done" buttons.
const int kManageDonePadding = 8;

// Padding between radio buttons and media menus buttons in the media bubble.
const int kMediaMenuVerticalPadding = 25;

// Padding between media menu elements in the media bubble.
const int kMediaMenuElementVerticalPadding = 5;

// The amount of horizontal space between the media menu title and the border.
const int kMediaMenuTitleHorizontalPadding = 10;

// The minimum width of the media menu buttons.
const CGFloat kMinMediaMenuButtonWidth = 100;

// Height of each of the labels in the MIDI bubble.
const int kMIDISysExLabelHeight = 14;

// Height of the "Clear" button in the MIDI bubble.
const int kMIDISysExClearButtonHeight = 17;

// General padding between elements in the MIDI bubble.
const int kMIDISysExPadding = 8;

// Padding between host names in the MIDI bubble.
const int kMIDISysExHostPadding = 4;

void SetControlSize(NSControl* control, NSControlSize controlSize) {
  CGFloat fontSize = [NSFont systemFontSizeForControlSize:controlSize];
  NSCell* cell = [control cell];
  [cell setFont:[NSFont systemFontOfSize:fontSize]];
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

// Sets the title for the popup button.
void SetTitleForPopUpButton(NSPopUpButton* button, NSString* title) {
  base::scoped_nsobject<NSMenuItem> titleItem([[NSMenuItem alloc] init]);
  [titleItem setTitle:title];
  [[button cell] setUsesItemFromMenu:NO];
  [[button cell] setMenuItem:titleItem.get()];
}

// Builds the popup button menu from the menu model and returns the width of the
// longgest item as the width of the popup menu.
CGFloat BuildPopUpMenuFromModel(NSPopUpButton* button,
                                ContentSettingMediaMenuModel* model,
                                const std::string& title,
                                bool disabled) {
  [[button cell] setControlSize:NSSmallControlSize];
  [[button cell] setArrowPosition:NSPopUpArrowAtBottom];
  [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [button setButtonType:NSMomentaryPushInButton];
  [button setAlignment:NSLeftTextAlignment];
  [button setAutoresizingMask:NSViewMinXMargin];
  [button setAction:@selector(mediaMenuChanged:)];
  [button sizeToFit];

  CGFloat menuWidth = 0;
  for (int i = 0; i < model->GetItemCount(); ++i) {
    NSString* itemTitle =
        base::SysUTF16ToNSString(model->GetLabelAt(i));
    [button addItemWithTitle:itemTitle];
    [[button lastItem] setTag:i];

    if (UTF16ToUTF8(model->GetLabelAt(i)) == title)
      [button selectItemWithTag:i];

    // Determine the largest possible size for this button.
    NSDictionary* textAttributes =
        [NSDictionary dictionaryWithObject:[button font]
                                    forKey:NSFontAttributeName];
    NSSize size = [itemTitle sizeWithAttributes:textAttributes];
    NSRect buttonFrame = [button frame];
    NSRect titleRect = [[button cell] titleRectForBounds:buttonFrame];
    CGFloat width = size.width + NSWidth(buttonFrame) - NSWidth(titleRect) +
        kMediaMenuTitleHorizontalPadding;
    menuWidth = std::max(menuWidth, width);
  }

  if (!model->GetItemCount()) {
    // Show a "None available" title and grey out the menu when there is no
    // available device.
    SetTitleForPopUpButton(
        button, l10n_util::GetNSString(IDS_MEDIA_MENU_NO_DEVICE_TITLE));
    [button setEnabled:NO];
  } else {
    SetTitleForPopUpButton(button, base::SysUTF8ToNSString(title));

    // Disable the device selection when the website is managing the devices
    // itself.
    if (disabled)
      [button setEnabled:NO];
  }

  return menuWidth;
}

}  // namespace

namespace content_setting_bubble {

MediaMenuParts::MediaMenuParts(content::MediaStreamType type,
                               NSTextField* label)
    : type(type),
      label(label) {}
MediaMenuParts::~MediaMenuParts() {}

}  // namespace content_setting_bubble

class ContentSettingBubbleWebContentsObserverBridge
    : public content::WebContentsObserver {
 public:
  ContentSettingBubbleWebContentsObserverBridge(
      content::WebContents* web_contents,
      ContentSettingBubbleController* controller)
      : content::WebContentsObserver(web_contents),
        controller_(controller) {
  }

 protected:
  // WebContentsObserver:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override {
    // Content settings are based on the main frame, so if it switches then
    // close up shop.
    [controller_ closeBubble:nil];
  }

 private:
  ContentSettingBubbleController* controller_;  // weak

  DISALLOW_COPY_AND_ASSIGN(ContentSettingBubbleWebContentsObserverBridge);
};

@interface ContentSettingBubbleController(Private)
- (id)initWithModel:(ContentSettingBubbleModel*)settingsBubbleModel
        webContents:(content::WebContents*)webContents
       parentWindow:(NSWindow*)parentWindow
         decoration:(ContentSettingDecoration*)decoration
         anchoredAt:(NSPoint)anchoredAt;
- (NSString*)getNibPathForModel:(ContentSettingBubbleModel*)model;
- (NSButton*)hyperlinkButtonWithFrame:(NSRect)frame
                                title:(NSString*)title
                                 icon:(NSImage*)icon
                       referenceFrame:(NSRect)referenceFrame;
- (void)initializeBlockedPluginsList;
- (void)initializeTitle;
- (void)initializeMessage;
- (void)initializeRadioGroup;
- (void)initializeItemList;
- (void)initializeGeoLists;
- (void)initializeMediaMenus;
- (void)initializeMIDISysExLists;
- (void)sizeToFitLoadButton;
- (void)initManageDoneButtons;
- (void)removeInfoButton;
- (void)popupLinkClicked:(id)sender;
- (void)clearGeolocationForCurrentHost:(id)sender;
- (void)clearMIDISysExForCurrentHost:(id)sender;
@end

@implementation ContentSettingBubbleController

+ (ContentSettingBubbleController*)
showForModel:(ContentSettingBubbleModel*)contentSettingBubbleModel
 webContents:(content::WebContents*)webContents
parentWindow:(NSWindow*)parentWindow
  decoration:(ContentSettingDecoration*)decoration
  anchoredAt:(NSPoint)anchor {
  // Autoreleases itself on bubble close.
  return [[ContentSettingBubbleController alloc]
      initWithModel:contentSettingBubbleModel
        webContents:webContents
       parentWindow:parentWindow
         decoration:decoration
         anchoredAt:anchor];
}

struct ContentTypeToNibPath {
  ContentSettingsType type;
  NSString* path;
};

const ContentTypeToNibPath kNibPaths[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, @"ContentBlockedCookies"},
    {CONTENT_SETTINGS_TYPE_IMAGES, @"ContentBlockedSimple"},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, @"ContentBlockedSimple"},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, @"ContentBlockedSimple"},
    {CONTENT_SETTINGS_TYPE_PLUGINS, @"ContentBlockedPlugins"},
    {CONTENT_SETTINGS_TYPE_POPUPS, @"ContentBlockedPopups"},
    {CONTENT_SETTINGS_TYPE_GEOLOCATION, @"ContentBlockedGeolocation"},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, @"ContentBlockedMixedScript"},
    {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, @"ContentProtocolHandlers"},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, @"ContentBlockedDownloads"},
    {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, @"ContentBlockedMIDISysEx"},
};

- (id)initWithModel:(ContentSettingBubbleModel*)contentSettingBubbleModel
        webContents:(content::WebContents*)webContents
       parentWindow:(NSWindow*)parentWindow
         decoration:(ContentSettingDecoration*)decoration
         anchoredAt:(NSPoint)anchoredAt {
  // This method takes ownership of |contentSettingBubbleModel| in all cases.
  std::unique_ptr<ContentSettingBubbleModel> model(contentSettingBubbleModel);
  DCHECK(model.get());
  observerBridge_.reset(
    new ContentSettingBubbleWebContentsObserverBridge(webContents, self));

  NSString* nibPath = [self getNibPathForModel:model.get()];

  DCHECK_NE(0u, [nibPath length]);

  if ((self = [super initWithWindowNibPath:nibPath
                              parentWindow:parentWindow
                                anchoredAt:anchoredAt])) {
    contentSettingBubbleModel_.reset(model.release());
    decoration_ = decoration;
    [self showWindow:nil];
  }
  return self;
}

- (NSString*)getNibPathForModel:(ContentSettingBubbleModel*)model {
  NSString* nibPath = @"";

  ContentSettingSimpleBubbleModel* simple_bubble = model->AsSimpleBubbleModel();
  if (simple_bubble) {
    ContentSettingsType settingsType = simple_bubble->content_type();

    for (const ContentTypeToNibPath& type_to_path : kNibPaths) {
      if (settingsType == type_to_path.type) {
        nibPath = type_to_path.path;
        break;
      }
    }
  }

  if (model->AsMediaStreamBubbleModel())
    nibPath = @"ContentBlockedMedia";

  if (model->AsSubresourceFilterBubbleModel())
    nibPath = @"ContentSubresourceFilter";
  return nibPath;
}

- (void)initializeTitle {
  if (!titleLabel_)
    return;

  NSString* label = base::SysUTF16ToNSString(
      contentSettingBubbleModel_->bubble_content().title);
  [titleLabel_ setStringValue:label];

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

- (void)initializeMessage {
  if (!messageLabel_)
    return;

  NSString* label = base::SysUTF16ToNSString(
      contentSettingBubbleModel_->bubble_content().message);
  [messageLabel_ setStringValue:label];

  CGFloat deltaY = [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:messageLabel_];
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += deltaY;
  [[self window] setFrame:windowFrame display:NO];
  NSRect messageFrame = [messageLabel_ frame];
  messageFrame.origin.y -= deltaY;
  [messageLabel_ setFrame:messageFrame];
}

- (void)initializeRadioGroup {
  // NOTE! Tags in the xib files must match the order of the radio buttons
  // passed in the radio_group and be 1-based, not 0-based.
  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      contentSettingBubbleModel_->bubble_content();
  const ContentSettingBubbleModel::RadioGroup& radio_group =
      bubble_content.radio_group;

  // Xcode 5.1 Interface Builder doesn't allow a font property to be set for
  // NSMatrix. The implementation of GTMUILocalizerAndLayoutTweaker assumes that
  // the font for each of the cells in a NSMatrix is identical, and is the font
  // of the NSMatrix. This logic sets the font of NSMatrix to be that of its
  // cells.
  NSFont* font = nil;
  for (NSCell* cell in [allowBlockRadioGroup_ cells]) {
    if (!font)
      font = [cell font];
    DCHECK([font isEqual:[cell font]]);
  }
  [allowBlockRadioGroup_ setFont:font];
  [allowBlockRadioGroup_ setEnabled:bubble_content.radio_group_enabled];

  // Select appropriate radio button.
  [allowBlockRadioGroup_ selectCellWithTag: radio_group.default_item + 1];

  const ContentSettingBubbleModel::RadioItems& radio_items =
      radio_group.radio_items;
  for (size_t ii = 0; ii < radio_group.radio_items.size(); ++ii) {
    NSCell* radioCell = [allowBlockRadioGroup_ cellWithTag: ii + 1];
    [radioCell setTitle:base::SysUTF8ToNSString(radio_items[ii])];
  }

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
  base::scoped_nsobject<HyperlinkButtonCell> cell(
      [[HyperlinkButtonCell alloc] initTextCell:title]);
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

  // Size to fit the button and add a little extra padding for the small-text
  // hyperlink button, which sizeToFit gets wrong.
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:button];
  NSRect buttonFrame = [button frame];
  buttonFrame.size.width += 2;

  // If the link text is too long, clamp it.
  int maxWidth = NSWidth([[self bubble] frame]) - 2 * NSMinX(referenceFrame);
  if (NSWidth(buttonFrame) > maxWidth)
    buttonFrame.size.width = maxWidth;

  [button setFrame:buttonFrame];
  [button setTarget:self];
  [button setAction:@selector(popupLinkClicked:)];
  return button;
}

- (void)initializeBlockedPluginsList {
  // Hide the empty label at the top of the dialog.
  int delta =
      NSMinY([titleLabel_ frame]) - NSMinY([blockedResourcesField_ frame]);
  [blockedResourcesField_ removeFromSuperview];
  NSRect frame = [[self window] frame];
  frame.size.height -= delta;
  [[self window] setFrame:frame display:NO];
}

- (void)initializeItemList {
  // I didn't put the buttons into a NSMatrix because then they are only one
  // entity in the key view loop. This way, one can tab through all of them.
  const ContentSettingBubbleModel::ListItems& listItems =
      contentSettingBubbleModel_->bubble_content().list_items;

  // Get the pre-resize frame of the radio group. Its origin is where the
  // popup list should go.
  NSRect radioFrame = [allowBlockRadioGroup_ frame];

  // Make room for the popup list. The bubble view and its subviews autosize
  // themselves when the window is enlarged.
  // Heading and radio box are already 1 * kLinkOuterPadding apart in the nib,
  // so only 1 * kLinkOuterPadding more is needed.
  int delta =
      listItems.size() * kLinkLineHeight - kLinkPadding + kLinkOuterPadding;
  NSSize deltaSize = NSMakeSize(0, delta);
  deltaSize = [[[self window] contentView] convertSize:deltaSize toView:nil];
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += deltaSize.height;
  [[self window] setFrame:windowFrame display:NO];

  // Create item list.
  int topLinkY = NSMaxY(radioFrame) + delta - kLinkHeight;
  int row = 0;
  for (const ContentSettingBubbleModel::ListItem& listItem : listItems) {
    NSImage* image = listItem.image.AsNSImage();
    NSRect frame = NSMakeRect(
        NSMinX(radioFrame), topLinkY - kLinkLineHeight * row, 200, kLinkHeight);
    if (listItem.has_link) {
      NSButton* button =
          [self hyperlinkButtonWithFrame:frame
                                   title:base::SysUTF8ToNSString(listItem.title)
                                    icon:image
                          referenceFrame:radioFrame];
      [[self bubble] addSubview:button];
      popupLinks_[button] = row++;
    } else {
      NSTextField* label =
          LabelWithFrame(base::SysUTF8ToNSString(listItem.title), frame);
      SetControlSize(label, NSSmallControlSize);
      [[self bubble] addSubview:label];
      row++;
    }
  }
}

- (void)initializeGeoLists {
  // Cocoa has its origin in the lower left corner. This means elements are
  // added from bottom to top, which explains why loops run backwards and the
  // order of operations is the other way than on Linux/Windows.
  const ContentSettingBubbleModel::BubbleContent& content =
      contentSettingBubbleModel_->bubble_content();
  NSRect containerFrame = [contentsContainer_ frame];
  NSRect frame = NSMakeRect(0, 0, NSWidth(containerFrame), kGeoLabelHeight);

  // "Clear" button / text field.
  if (!content.custom_link.empty()) {
    base::scoped_nsobject<NSControl> control;
    if(content.custom_link_enabled) {
      NSRect buttonFrame = NSMakeRect(0, 0,
                                      NSWidth(containerFrame),
                                      kGeoClearButtonHeight);
      NSButton* button = [[NSButton alloc] initWithFrame:buttonFrame];
      control.reset(button);
      [button setTitle:base::SysUTF8ToNSString(content.custom_link)];
      [button setTarget:self];
      [button setAction:@selector(clearGeolocationForCurrentHost:)];
      [button setBezelStyle:NSRoundRectBezelStyle];
      SetControlSize(button, NSSmallControlSize);
      [button sizeToFit];
    } else {
      // Add the notification that settings will be cleared on next reload.
      control.reset([LabelWithFrame(
          base::SysUTF8ToNSString(content.custom_link), frame) retain]);
      SetControlSize(control.get(), NSSmallControlSize);
    }

    // If the new control is wider than the container, widen the window.
    CGFloat controlWidth = NSWidth([control frame]);
    if (controlWidth > NSWidth(containerFrame)) {
      NSRect windowFrame = [[self window] frame];
      windowFrame.size.width += controlWidth - NSWidth(containerFrame);
      [[self window] setFrame:windowFrame display:NO];
      // Fetch the updated sizes.
      containerFrame = [contentsContainer_ frame];
      frame = NSMakeRect(0, 0, NSWidth(containerFrame), kGeoLabelHeight);
    }

    DCHECK(control);
    [contentsContainer_ addSubview:control];
    frame.origin.y = NSMaxY([control frame]) + kGeoPadding;
  }

  for (auto i = content.domain_lists.rbegin();
       i != content.domain_lists.rend(); ++i) {
    // Add all hosts in the current domain list.
    for (auto j = i->hosts.rbegin(); j != i->hosts.rend(); ++j) {
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
  windowFrame.size.height += containerHeight - NSHeight(containerFrame);
  [[self window] setFrame:windowFrame display:NO];
  containerFrame.size.height = containerHeight;
  [contentsContainer_ setFrame:containerFrame];
}

- (void)initializeMediaMenus {
  const ContentSettingBubbleModel::MediaMenuMap& media_menus =
      contentSettingBubbleModel_->bubble_content().media_menus;

  // Calculate the longest width of the labels and menus menus to avoid
  // truncation by the window's edge.
  CGFloat maxLabelWidth = 0;
  CGFloat maxMenuWidth = 0;
  CGFloat maxMenuHeight = 0;
  NSRect radioFrame = [allowBlockRadioGroup_ frame];
  for (const std::pair<content::MediaStreamType,
                       ContentSettingBubbleModel::MediaMenu>& map_entry :
       media_menus) {
    // |labelFrame| will be resized later on in this function.
    NSRect labelFrame = NSMakeRect(NSMinX(radioFrame), 0, 0, 0);
    NSTextField* label = LabelWithFrame(
        base::SysUTF8ToNSString(map_entry.second.label), labelFrame);
    SetControlSize(label, NSSmallControlSize);
    NSCell* cell = [label cell];
    [cell setAlignment:NSRightTextAlignment];
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:label];
    maxLabelWidth = std::max(maxLabelWidth, [label frame].size.width);
    [[self bubble] addSubview:label];

    // |buttonFrame| will be resized and repositioned later on.
    NSRect buttonFrame = NSMakeRect(NSMinX(radioFrame), 0, 0, 0);
    base::scoped_nsobject<NSPopUpButton> button(
        [[NSPopUpButton alloc] initWithFrame:buttonFrame]);
    [button setTarget:self];

    // Set the map_entry's key value to |button| tag.
    // MediaMenuPartsMap uses this value to order its elements.
    [button setTag:static_cast<NSInteger>(map_entry.first)];

    // Store the |label| and |button| into MediaMenuParts struct and build
    // the popup menu from the menu model.
    content_setting_bubble::MediaMenuParts* menuParts =
        new content_setting_bubble::MediaMenuParts(map_entry.first, label);
    menuParts->model.reset(new ContentSettingMediaMenuModel(
        map_entry.first, contentSettingBubbleModel_.get(),
        ContentSettingMediaMenuModel::MenuLabelChangedCallback()));
    mediaMenus_[button] = base::WrapUnique(menuParts);
    CGFloat width = BuildPopUpMenuFromModel(
        button, menuParts->model.get(), map_entry.second.selected_device.name,
        map_entry.second.disabled);
    maxMenuWidth = std::max(maxMenuWidth, width);

    [[self bubble] addSubview:button
                   positioned:NSWindowBelow
                   relativeTo:nil];

    maxMenuHeight = std::max(maxMenuHeight, [button frame].size.height);
  }

  // Make room for the media menu(s) and enlarge the windows to fit the views.
  // The bubble view and its subviews autosize themselves when the window is
  // enlarged.
  int delta = media_menus.size() * maxMenuHeight +
      (media_menus.size() - 1) * kMediaMenuElementVerticalPadding;
  NSSize deltaSize = NSMakeSize(0, delta);
  deltaSize = [[[self window] contentView] convertSize:deltaSize toView:nil];
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += deltaSize.height;
  // If the media menus are wider than the window, widen the window.
  CGFloat widthNeeded = maxLabelWidth + maxMenuWidth + 2 * NSMinX(radioFrame);
  if (widthNeeded > windowFrame.size.width)
    windowFrame.size.width = widthNeeded;
  [[self window] setFrame:windowFrame display:NO];

  // The radio group lies above the media menus, move the radio group up.
  radioFrame.origin.y += delta;
  [allowBlockRadioGroup_ setFrame:radioFrame];

  // Resize and reposition the media menus layout.
  CGFloat topMenuY = NSMinY(radioFrame) - kMediaMenuVerticalPadding;
  maxMenuWidth = std::max(maxMenuWidth, kMinMediaMenuButtonWidth);
  for (const auto& map_entry : mediaMenus_) {
    NSRect labelFrame = [map_entry.second->label frame];
    // Align the label text with the button text.
    labelFrame.origin.y =
        topMenuY + (maxMenuHeight - labelFrame.size.height) / 2 + 1;
    labelFrame.size.width = maxLabelWidth;
    [map_entry.second->label setFrame:labelFrame];
    NSRect menuFrame = [map_entry.first frame];
    menuFrame.origin.y = topMenuY;
    menuFrame.origin.x = NSMinX(radioFrame) + maxLabelWidth;
    menuFrame.size.width = maxMenuWidth;
    menuFrame.size.height = maxMenuHeight;
    [map_entry.first setFrame:menuFrame];
    topMenuY -= (maxMenuHeight + kMediaMenuElementVerticalPadding);
  }
}

- (void)initializeMIDISysExLists {
  const ContentSettingBubbleModel::BubbleContent& content =
      contentSettingBubbleModel_->bubble_content();
  NSRect containerFrame = [contentsContainer_ frame];
  NSRect frame =
      NSMakeRect(0, 0, NSWidth(containerFrame), kMIDISysExLabelHeight);

  // "Clear" button / text field.
  if (!content.custom_link.empty()) {
    base::scoped_nsobject<NSControl> control;
    if (content.custom_link_enabled) {
      NSRect buttonFrame = NSMakeRect(0, 0,
                                      NSWidth(containerFrame),
                                      kMIDISysExClearButtonHeight);
      NSButton* button = [[NSButton alloc] initWithFrame:buttonFrame];
      control.reset(button);
      [button setTitle:base::SysUTF8ToNSString(content.custom_link)];
      [button setTarget:self];
      [button setAction:@selector(clearMIDISysExForCurrentHost:)];
      [button setBezelStyle:NSRoundRectBezelStyle];
      SetControlSize(button, NSSmallControlSize);
      [button sizeToFit];
    } else {
      // Add the notification that settings will be cleared on next reload.
      control.reset([LabelWithFrame(
          base::SysUTF8ToNSString(content.custom_link), frame) retain]);
      SetControlSize(control.get(), NSSmallControlSize);
    }

    // If the new control is wider than the container, widen the window.
    CGFloat controlWidth = NSWidth([control frame]);
    if (controlWidth > NSWidth(containerFrame)) {
      NSRect windowFrame = [[self window] frame];
      windowFrame.size.width += controlWidth - NSWidth(containerFrame);
      [[self window] setFrame:windowFrame display:NO];
      // Fetch the updated sizes.
      containerFrame = [contentsContainer_ frame];
      frame = NSMakeRect(0, 0, NSWidth(containerFrame), kMIDISysExLabelHeight);
    }

    DCHECK(control);
    [contentsContainer_ addSubview:control];
    frame.origin.y = NSMaxY([control frame]) + kMIDISysExPadding;
  }

  for (auto i = content.domain_lists.rbegin();
       i != content.domain_lists.rend(); ++i) {
    // Add all hosts in the current domain list.
    for (auto j = i->hosts.rbegin(); j != i->hosts.rend(); ++j) {
      NSTextField* title = LabelWithFrame(base::SysUTF8ToNSString(*j), frame);
      SetControlSize(title, NSSmallControlSize);
      [contentsContainer_ addSubview:title];

      frame.origin.y = NSMaxY(frame) + kMIDISysExHostPadding +
          [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:title];
    }
    if (!i->hosts.empty())
      frame.origin.y += kMIDISysExPadding - kMIDISysExHostPadding;

    // Add the domain list's title.
    NSTextField* title =
        LabelWithFrame(base::SysUTF8ToNSString(i->title), frame);
    SetControlSize(title, NSSmallControlSize);
    [contentsContainer_ addSubview:title];

    frame.origin.y = NSMaxY(frame) + kMIDISysExPadding +
        [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:title];
  }

  CGFloat containerHeight = frame.origin.y;
  // Undo last padding.
  if (!content.domain_lists.empty())
    containerHeight -= kMIDISysExPadding;

  // Resize container to fit its subviews, and window to fit the container.
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height += containerHeight - NSHeight(containerFrame);
  [[self window] setFrame:windowFrame display:NO];
  containerFrame.size.height = containerHeight;
  [contentsContainer_ setFrame:containerFrame];
}

- (void)sizeToFitLoadButton {
  const ContentSettingBubbleModel::BubbleContent& content =
      contentSettingBubbleModel_->bubble_content();
  [loadButton_ setEnabled:content.custom_link_enabled];

  // Resize horizontally to fit button if necessary.
  NSRect windowFrame = [[self window] frame];
  int widthNeeded = NSWidth([loadButton_ frame]) +
      2 * NSMinX([loadButton_ frame]);
  if (NSWidth(windowFrame) < widthNeeded) {
    windowFrame.size.width = widthNeeded;
    [[self window] setFrame:windowFrame display:NO];
  }
}

- (void)initManageDoneButtons {
  const ContentSettingBubbleModel::BubbleContent& content =
      contentSettingBubbleModel_->bubble_content();
  [manageButton_ setTitle:base::SysUTF8ToNSString(content.manage_text)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:[manageButton_ superview]];

  CGFloat actualWidth = NSWidth([[[self window] contentView] frame]);
  CGFloat requiredWidth = NSMaxX([manageButton_ frame]) + kManageDonePadding +
      NSWidth([[doneButton_ superview] frame]) - NSMinX([doneButton_ frame]);
  if (requiredWidth <= actualWidth || !doneButton_ || !manageButton_)
    return;

  // Resize window, autoresizing takes care of the rest.
  NSSize size = NSMakeSize(requiredWidth - actualWidth, 0);
  size = [[[self window] contentView] convertSize:size toView:nil];
  NSRect frame = [[self window] frame];
  frame.origin.x -= size.width;
  frame.size.width += size.width;
  [[self window] setFrame:frame display:NO];
}

- (void)awakeFromNib {
  [super awakeFromNib];

  [[self bubble] setArrowLocation:info_bubble::kTopRight];

  // Adapt window size to bottom buttons. Do this before all other layouting.
  [self initManageDoneButtons];

  [self initializeTitle];
  [self initializeMessage];

  // Note that the per-content-type methods and |initializeRadioGroup| below
  // must be kept in the correct order, as they make interdependent adjustments
  // of the bubble's height.
  ContentSettingSimpleBubbleModel* simple_bubble =
      contentSettingBubbleModel_->AsSimpleBubbleModel();
  if (simple_bubble &&
      simple_bubble->content_type() == CONTENT_SETTINGS_TYPE_PLUGINS) {
    [self sizeToFitLoadButton];
    [self initializeBlockedPluginsList];
  }

  if (allowBlockRadioGroup_)  // Some xibs do not bind |allowBlockRadioGroup_|.
    [self initializeRadioGroup];

  if (simple_bubble) {
    ContentSettingsType type = simple_bubble->content_type();

    if (type == CONTENT_SETTINGS_TYPE_POPUPS ||
        type == CONTENT_SETTINGS_TYPE_PLUGINS)
      [self initializeItemList];
    if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION)
      [self initializeGeoLists];
    if (type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX)
      [self initializeMIDISysExLists];
  }

  if (contentSettingBubbleModel_->AsMediaStreamBubbleModel())
    [self initializeMediaMenus];
}

///////////////////////////////////////////////////////////////////////////////
// Actual application logic

- (IBAction)allowBlockToggled:(id)sender {
  NSButtonCell *selectedCell = [sender selectedCell];
  contentSettingBubbleModel_->OnRadioClicked([selectedCell tag] - 1);
}

- (void)popupLinkClicked:(id)sender {
  content_setting_bubble::PopupLinks::iterator i(popupLinks_.find(sender));
  DCHECK(i != popupLinks_.end());
  contentSettingBubbleModel_->OnListItemClicked(i->second);
}

- (void)clearGeolocationForCurrentHost:(id)sender {
  contentSettingBubbleModel_->OnCustomLinkClicked();
  [self close];
}

- (void)clearMIDISysExForCurrentHost:(id)sender {
  contentSettingBubbleModel_->OnCustomLinkClicked();
  [self close];
}

- (IBAction)showMoreInfo:(id)sender {
  contentSettingBubbleModel_->OnCustomLinkClicked();
  [self close];
}

- (IBAction)load:(id)sender {
  contentSettingBubbleModel_->OnCustomLinkClicked();
  [self close];
}

- (IBAction)learnMoreLinkClicked:(id)sender {
  contentSettingBubbleModel_->OnManageLinkClicked();
}

- (IBAction)manageBlocking:(id)sender {
  contentSettingBubbleModel_->OnManageLinkClicked();
}

- (IBAction)closeBubble:(id)sender {
  contentSettingBubbleModel_->OnDoneClicked();
  [self close];
}

- (IBAction)mediaMenuChanged:(id)sender {
  NSPopUpButton* button = static_cast<NSPopUpButton*>(sender);
  auto it = mediaMenus_.find(sender);
  DCHECK(it != mediaMenus_.end());
  NSInteger index = [[button selectedItem] tag];

  SetTitleForPopUpButton(
      button, base::SysUTF16ToNSString(it->second->model->GetLabelAt(index)));

  it->second->model->ExecuteCommand(index, 0);
}

- (content_setting_bubble::MediaMenuPartsMap*)mediaMenus {
  return &mediaMenus_;
}

- (LocationBarDecoration*)decorationForBubble {
  return decoration_;
}

@end  // ContentSettingBubbleController
