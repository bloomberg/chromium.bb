// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_media_menu_model.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#include "content/public/browser/plugin_service.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
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

// Padding between radio buttons and "Load all plugins" button
// in the plugin bubble.
const int kLoadAllPluginsButtonVerticalPadding = 8;

// General padding between elements in the geolocation bubble.
const int kGeoPadding = 8;

// Padding between host names in the geolocation bubble.
const int kGeoHostPadding = 4;

// Minimal padding between "Manage" and "Done" buttons.
const int kManageDonePadding = 8;

// Padding between radio buttons and media menus buttons in the media bubble.
const int kMediaMenuVerticalPadding = 20;

// Padding between media menu elements in the media bubble.
const int kMediaMenuElementVerticalPadding = 5;

// The amount of horizontal space between the media menu title and the border.
const int kMediaMenuTitleHorizontalPadding = 10;

// The minimum width of the media menu buttons.
const CGFloat kMinMediaMenuButtonWidth = 100;

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

// Sets the title for the popup button.
void SetTitleForPopUpButton(NSPopUpButton* button, NSString* title) {
  scoped_nsobject<NSMenuItem> titleItem([[NSMenuItem alloc] init]);
  [titleItem setTitle:title];
  [[button cell] setUsesItemFromMenu:NO];
  [[button cell] setMenuItem:titleItem.get()];
}

// Builds the popup button menu from the menu model and returns the width of the
// longgest item as the width of the popup menu.
CGFloat BuildPopUpMenuFromModel(NSPopUpButton* button,
                                ContentSettingMediaMenuModel* model,
                                const std::string& title) {
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

@interface ContentSettingBubbleController(Private)
- (id)initWithModel:(ContentSettingBubbleModel*)settingsBubbleModel
       parentWindow:(NSWindow*)parentWindow
         anchoredAt:(NSPoint)anchoredAt;
- (NSButton*)hyperlinkButtonWithFrame:(NSRect)frame
                                title:(NSString*)title
                                 icon:(NSImage*)icon
                       referenceFrame:(NSRect)referenceFrame;
- (void)initializeBlockedPluginsList;
- (void)initializeTitle;
- (void)initializeRadioGroup;
- (void)initializePopupList;
- (void)initializeGeoLists;
- (void)initializeMediaMenus;
- (void)sizeToFitLoadButton;
- (void)initManageDoneButtons;
- (void)removeInfoButton;
- (void)popupLinkClicked:(id)sender;
- (void)clearGeolocationForCurrentHost:(id)sender;
@end

@implementation ContentSettingBubbleController

+ (ContentSettingBubbleController*)
    showForModel:(ContentSettingBubbleModel*)contentSettingBubbleModel
    parentWindow:(NSWindow*)parentWindow
      anchoredAt:(NSPoint)anchor {
  // Autoreleases itself on bubble close.
  return [[ContentSettingBubbleController alloc]
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

  ContentSettingsType settingsType = model->content_type();
  NSString* nibPath = @"";
  switch (settingsType) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      nibPath = @"ContentBlockedCookies"; break;
    case CONTENT_SETTINGS_TYPE_IMAGES:
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
    case CONTENT_SETTINGS_TYPE_PPAPI_BROKER:
      nibPath = @"ContentBlockedSimple"; break;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      nibPath = @"ContentBlockedPlugins"; break;
    case CONTENT_SETTINGS_TYPE_POPUPS:
      nibPath = @"ContentBlockedPopups"; break;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      nibPath = @"ContentBlockedGeolocation"; break;
    case CONTENT_SETTINGS_TYPE_MIXEDSCRIPT:
      nibPath = @"ContentBlockedMixedScript"; break;
    case CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS:
      nibPath = @"ContentProtocolHandlers"; break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      nibPath = @"ContentBlockedMedia"; break;
    // These content types have no bubble:
    case CONTENT_SETTINGS_TYPE_DEFAULT:
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE:
    case CONTENT_SETTINGS_TYPE_FULLSCREEN:
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
    case CONTENT_SETTINGS_NUM_TYPES:
      NOTREACHED();
  }
  if ((self = [super initWithWindowNibPath:nibPath
                              parentWindow:parentWindow
                                anchoredAt:anchoredAt])) {
    contentSettingBubbleModel_.reset(model.release());
    [self showWindow:nil];
  }
  return self;
}

- (void)dealloc {
  STLDeleteValues(&mediaMenus_);
  [super dealloc];
}

- (void)initializeTitle {
  if (!titleLabel_)
    return;

  NSString* label = base::SysUTF8ToNSString(
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

- (void)initializeRadioGroup {
  // NOTE! Tags in the xib files must match the order of the radio buttons
  // passed in the radio_group and be 1-based, not 0-based.
  const ContentSettingBubbleModel::RadioGroup& radio_group =
      contentSettingBubbleModel_->bubble_content().radio_group;

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
  NSMutableArray* pluginArray = [NSMutableArray array];
  const std::set<std::string>& plugins =
      contentSettingBubbleModel_->bubble_content().resource_identifiers;
  if (plugins.empty()) {
    int delta = NSMinY([titleLabel_ frame]) -
                NSMinY([blockedResourcesField_ frame]);
    [blockedResourcesField_ removeFromSuperview];
    NSRect frame = [[self window] frame];
    frame.size.height -= delta;
    [[self window] setFrame:frame display:NO];
  } else {
    PluginFinder* finder = PluginFinder::GetInstance();
    for (std::set<std::string>::iterator it = plugins.begin();
         it != plugins.end(); ++it) {
      NSString* name =
          SysUTF16ToNSString(finder->FindPluginNameWithIdentifier(*it));
      [pluginArray addObject:name];
    }
    [blockedResourcesField_
        setStringValue:[pluginArray componentsJoinedByString:@"\n"]];
    [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:blockedResourcesField_];
  }
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
    NSImage* image = it->image.AsNSImage();

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
    [[self bubble] addSubview:button];
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
  NSRect frame = NSMakeRect(0, 0, NSWidth(containerFrame), kGeoLabelHeight);

  // "Clear" button / text field.
  if (!content.custom_link.empty()) {
    scoped_nsobject<NSControl> control;
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
  NSRect mediaMenusFrame = [mediaMenusContainer_ frame];
  CGFloat topMenuY = NSMaxY(mediaMenusFrame) - kMediaMenuVerticalPadding;
  for (ContentSettingBubbleModel::MediaMenuMap::const_iterator it(
       media_menus.begin()); it != media_menus.end(); ++it) {
    // |labelFrame| will be resized later on in this function.
    NSRect labelFrame = NSMakeRect(NSMinX(mediaMenusFrame), topMenuY, 0, 0);
    NSTextField* label =
        LabelWithFrame(base::SysUTF8ToNSString(it->second.label), labelFrame);
    SetControlSize(label, NSSmallControlSize);
    NSCell* cell = [label cell];
    [cell setAlignment:NSRightTextAlignment];
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:label];
    maxLabelWidth = std::max(maxLabelWidth, [label frame].size.width);
    [[self bubble]  addSubview:label];

    // |buttonFrame| will be resized and repositioned later on.
    NSRect buttonFrame = NSMakeRect(NSMinX(mediaMenusFrame), topMenuY, 0, 0);
    scoped_nsobject<NSPopUpButton> button(
        [[NSPopUpButton alloc] initWithFrame:buttonFrame]);
    [button setTarget:self];

    // Store the |label| and |button| into MediaMenuParts struct and build
    // the popup menu from the menu model.
    content_setting_bubble::MediaMenuParts* menuParts =
        new content_setting_bubble::MediaMenuParts(it->first, label);
    menuParts->model.reset(new ContentSettingMediaMenuModel(
        it->first, contentSettingBubbleModel_.get(),
        ContentSettingMediaMenuModel::MenuLabelChangedCallback()));
    mediaMenus_[button] = menuParts;
    CGFloat width = BuildPopUpMenuFromModel(button,
                                            menuParts->model.get(),
                                            it->second.selected_device.name);
    maxMenuWidth = std::max(maxMenuWidth, width);

    [[self bubble] addSubview:button
                   positioned:NSWindowBelow
                   relativeTo:nil];

    maxMenuHeight = std::max(maxMenuHeight, [button frame].size.height);
    topMenuY -= (maxMenuHeight + kMediaMenuElementVerticalPadding);
  }

  // Resize and reposition the media menus layout.
  maxMenuWidth = std::max(maxMenuWidth, kMinMediaMenuButtonWidth);
  for (content_setting_bubble::MediaMenuPartsMap::const_iterator i =
       mediaMenus_.begin(); i != mediaMenus_.end(); ++i) {
    NSRect labelFrame = [i->second->label frame];
    // Align the label text with the button text.
    labelFrame.origin.y += (maxMenuHeight - labelFrame.size.height) / 2 + 1;
    labelFrame.size.width = maxLabelWidth;
    [i->second->label setFrame:labelFrame];
    NSRect menuFrame = [i->first frame];
    menuFrame.origin.x = NSMinX(mediaMenusFrame) + maxLabelWidth;
    menuFrame.size.width = maxMenuWidth;
    menuFrame.size.height = maxMenuHeight;
    [i->first setFrame:menuFrame];
  }

  // If the media menus are wider than the window, widen the window.
  NSRect frame = [[self window] frame];
  CGFloat widthNeeded = maxLabelWidth + maxMenuWidth +
      2 * NSMinX([mediaMenusContainer_ frame]);
  if (widthNeeded > frame.size.width) {
    frame.size.width = widthNeeded;
    [[self window] setFrame:frame display:NO];
  }
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
  [manageButton_ setTitle:base::SysUTF8ToNSString(content.manage_link)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:manageButton_];

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

  ContentSettingsType type = contentSettingBubbleModel_->content_type();
  if (type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    [self sizeToFitLoadButton];
    [self initializeBlockedPluginsList];
  }

  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM)
    [self initializeMediaMenus];

  if (allowBlockRadioGroup_)  // not bound in cookie bubble xib
    [self initializeRadioGroup];

  if (type == CONTENT_SETTINGS_TYPE_POPUPS)
    [self initializePopupList];
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION)
    [self initializeGeoLists];
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
  contentSettingBubbleModel_->OnPopupClicked(i->second);
}

- (void)clearGeolocationForCurrentHost:(id)sender {
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
  content_setting_bubble::MediaMenuPartsMap::const_iterator it(
      mediaMenus_.find(sender));
  DCHECK(it != mediaMenus_.end());
  NSInteger index = [[button selectedItem] tag];

  SetTitleForPopUpButton(
      button, base::SysUTF16ToNSString(it->second->model->GetLabelAt(index)));

  it->second->model->ExecuteCommand(index, 0);
}

- (content_setting_bubble::MediaMenuPartsMap*)mediaMenus {
  return &mediaMenus_;
}

@end  // ContentSettingBubbleController
