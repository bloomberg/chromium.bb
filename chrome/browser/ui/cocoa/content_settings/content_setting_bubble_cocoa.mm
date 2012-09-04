// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
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
- (void)sizeToFitLoadButton;
- (void)sizeToFitManageDoneButtons;
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
      nibPath = @"ContentBlockedImages"; break;
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      nibPath = @"ContentBlockedJavaScript"; break;
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
    // These content types have no bubble:
    case CONTENT_SETTINGS_TYPE_DEFAULT:
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_INTENTS:
    case CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE:
    case CONTENT_SETTINGS_TYPE_FULLSCREEN:
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
    case CONTENT_SETTINGS_TYPE_PPAPI_BROKER:
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
    for (std::set<std::string>::iterator it = plugins.begin();
         it != plugins.end(); ++it) {
      NSString* name = SysUTF16ToNSString(
          PluginService::GetInstance()->GetPluginGroupName(*it));
      if ([name length] == 0)
        name = base::SysUTF8ToNSString(*it);
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

- (void)sizeToFitManageDoneButtons {
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
  [self sizeToFitManageDoneButtons];

  [self initializeTitle];

  ContentSettingsType type = contentSettingBubbleModel_->content_type();
  if (type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    [self sizeToFitLoadButton];
    [self initializeBlockedPluginsList];
  }

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

@end  // ContentSettingBubbleController
