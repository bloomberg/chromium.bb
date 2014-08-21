// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/ui/cocoa/location_bar/page_action_decoration.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu_controller.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/webui/extensions/extension_info_ui.h"
#include "components/sessions/session_id.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/image/image.h"

using content::WebContents;
using extensions::Extension;
using extensions::LocationBarController;

namespace {

// Distance to offset the bubble pointer from the bottom of the max
// icon area of the decoration.  This makes the popup's upper border
// 2px away from the omnibox's lower border (matches omnibox popup
// upper border).
const CGFloat kBubblePointYOffset = 2.0;

}  // namespace

PageActionDecoration::PageActionDecoration(
    LocationBarViewMac* owner,
    Browser* browser,
    ExtensionAction* page_action)
    : owner_(NULL),
      browser_(browser),
      page_action_(page_action),
      current_tab_id_(-1),
      preview_enabled_(false) {
  const Extension* extension = extensions::ExtensionRegistry::Get(
      browser->profile())->enabled_extensions().GetByID(
          page_action->extension_id());
  DCHECK(extension);

  icon_factory_.reset(new ExtensionActionIconFactory(
      browser_->profile(), extension, page_action, this));

  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(browser_->profile()));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_COMMAND_PAGE_ACTION_MAC,
                 content::Source<Profile>(browser_->profile()));

  // We set the owner last of all so that we can determine whether we are in
  // the process of initializing this class or not.
  owner_ = owner;
}

PageActionDecoration::~PageActionDecoration() {}

// Always |kPageActionIconMaxSize| wide.  |ImageDecoration| draws the
// image centered.
CGFloat PageActionDecoration::GetWidthForSpace(CGFloat width) {
  return ExtensionAction::kPageActionIconMaxSize;
}

bool PageActionDecoration::AcceptsMousePress() {
  return true;
}

// Either notify listeners or show a popup depending on the Page
// Action.
bool PageActionDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  return ActivatePageAction(frame);
}

void PageActionDecoration::ActivatePageAction() {
  ActivatePageAction(owner_->GetPageActionFrame(page_action_));
}

bool PageActionDecoration::ActivatePageAction(NSRect frame) {
  WebContents* web_contents = owner_->GetWebContents();
  if (!web_contents) {
    // We don't want other code to try and handle this click. Returning true
    // prevents this by indicating that we handled it.
    return true;
  }

  LocationBarController* controller =
      extensions::TabHelper::FromWebContents(web_contents)->
          location_bar_controller();

  switch (controller->OnClicked(page_action_)) {
    case ExtensionAction::ACTION_NONE:
      break;

    case ExtensionAction::ACTION_SHOW_POPUP:
      ShowPopup(frame, page_action_->GetPopupUrl(current_tab_id_));
      break;
  }

  return true;
}

void PageActionDecoration::OnIconUpdated() {
  // If we have no owner, that means this class is still being constructed.
  WebContents* web_contents = owner_ ? owner_->GetWebContents() : NULL;
  if (web_contents) {
    UpdateVisibility(web_contents, current_url_);
    owner_->RedrawDecoration(this);
  }
}

void PageActionDecoration::UpdateVisibility(WebContents* contents,
                                            const GURL& url) {
  // Save this off so we can pass it back to the extension when the action gets
  // executed. See PageActionDecoration::OnMousePressed.
  current_tab_id_ =
      contents ? extensions::ExtensionTabUtil::GetTabId(contents) : -1;
  current_url_ = url;

  bool visible = contents &&
      (preview_enabled_ || page_action_->GetIsVisible(current_tab_id_));
  if (visible) {
    SetToolTip(page_action_->GetTitle(current_tab_id_));

    // Set the image.
    gfx::Image icon = icon_factory_->GetIcon(current_tab_id_);
    if (!icon.IsEmpty()) {
      SetImage(icon.ToNSImage());
    } else if (!GetImage()) {
      const NSSize default_size = NSMakeSize(
          ExtensionAction::kPageActionIconMaxSize,
          ExtensionAction::kPageActionIconMaxSize);
      SetImage([[[NSImage alloc] initWithSize:default_size] autorelease]);
    }
  }

  if (IsVisible() != visible)
    SetVisible(visible);
}

void PageActionDecoration::SetToolTip(NSString* tooltip) {
  tooltip_.reset([tooltip retain]);
}

void PageActionDecoration::SetToolTip(std::string tooltip) {
  SetToolTip(tooltip.empty() ? nil : base::SysUTF8ToNSString(tooltip));
}

NSString* PageActionDecoration::GetToolTip() {
  return tooltip_.get();
}

NSPoint PageActionDecoration::GetBubblePointInFrame(NSRect frame) {
  // This is similar to |ImageDecoration::GetDrawRectInFrame()|,
  // except that code centers the image, which can differ in size
  // between actions.  This centers the maximum image size, so the
  // point will consistently be at the same y position.  x position is
  // easier (the middle of the centered image is the middle of the
  // frame).
  const CGFloat delta_height =
      NSHeight(frame) - ExtensionAction::kPageActionIconMaxSize;
  const CGFloat bottom_inset = std::ceil(delta_height / 2.0);

  // Return a point just below the bottom of the maximal drawing area.
  return NSMakePoint(NSMidX(frame),
                     NSMaxY(frame) - bottom_inset + kBubblePointYOffset);
}

NSMenu* PageActionDecoration::GetMenu() {
  const Extension* extension = extensions::ExtensionRegistry::Get(
      browser_->profile())->enabled_extensions().GetByID(
          page_action_->extension_id());
  DCHECK(extension);
  if (!extension->ShowConfigureContextMenus())
    return nil;

  contextMenuController_.reset([[ExtensionActionContextMenuController alloc]
      initWithExtension:extension
                browser:browser_
        extensionAction:page_action_]);

  base::scoped_nsobject<NSMenu> contextMenu([[NSMenu alloc] initWithTitle:@""]);
  [contextMenuController_ populateMenu:contextMenu];
  return contextMenu.autorelease();
}

void PageActionDecoration::ShowPopup(const NSRect& frame,
                                     const GURL& popup_url) {
  // Anchor popup at the bottom center of the page action icon.
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  NSPoint anchor = GetBubblePointInFrame(frame);
  anchor = [field convertPoint:anchor toView:nil];

  [ExtensionPopupController showURL:popup_url
                          inBrowser:chrome::GetLastActiveBrowser()
                         anchoredAt:anchor
                      arrowLocation:info_bubble::kTopRight
                            devMode:NO];
}

void PageActionDecoration::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
      ExtensionPopupController* popup = [ExtensionPopupController popup];
      if (popup && ![popup isClosing])
        [popup close];

      break;
    }
    case extensions::NOTIFICATION_EXTENSION_COMMAND_PAGE_ACTION_MAC: {
      std::pair<const std::string, gfx::NativeWindow>* payload =
      content::Details<std::pair<const std::string, gfx::NativeWindow> >(
          details).ptr();
      std::string extension_id = payload->first;
      gfx::NativeWindow window = payload->second;
      if (window != browser_->window()->GetNativeWindow())
        break;
      if (extension_id != page_action_->extension_id())
        break;
      if (IsVisible())
        ActivatePageAction();
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
}
