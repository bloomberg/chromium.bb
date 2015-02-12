// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/page_action_decoration.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/gfx/image/image.h"

using content::WebContents;
using extensions::Extension;

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
      preview_enabled_(false) {
  const Extension* extension = extensions::ExtensionRegistry::Get(
      browser->profile())->enabled_extensions().GetByID(
          page_action->extension_id());
  DCHECK(extension);

  viewController_.reset(
      new ExtensionActionViewController(extension, browser, page_action));
  viewController_->SetDelegate(this);

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
  ActivatePageAction(true);
  // We don't want other code to try and handle this click. Returning true
  // prevents this by indicating that we handled it.
  return true;
}

bool PageActionDecoration::ActivatePageAction(bool grant_active_tab) {
  WebContents* web_contents = owner_->GetWebContents();
  if (!web_contents)
    return false;

  viewController_->ExecuteAction(grant_active_tab);
  return true;
}

const extensions::Extension* PageActionDecoration::GetExtension() {
  return viewController_->extension();
}

ExtensionAction* PageActionDecoration::GetPageAction() {
  return viewController_->extension_action();
}

void PageActionDecoration::UpdateVisibility(WebContents* contents) {
  bool visible =
      contents && (preview_enabled_ || viewController_->IsEnabled(contents));
  if (visible) {
    SetToolTip(viewController_->GetTooltip(contents));

    // Set the image.
    gfx::Image icon = viewController_->GetIcon(contents);
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
  ui::MenuModel* contextMenu = viewController_->GetContextMenu();
  if (!contextMenu)
    return nil;
  contextMenuController_.reset(
      [[MenuController alloc] initWithModel:contextMenu
                     useWithPopUpButtonCell:NO]);
  return [contextMenuController_ menu];
}

void PageActionDecoration::SetToolTip(const base::string16& tooltip) {
  NSString* nsTooltip =
      tooltip.empty() ? nil : base::SysUTF16ToNSString(tooltip);
  tooltip_.reset([nsTooltip retain]);
}

ToolbarActionViewController*
PageActionDecoration::GetPreferredPopupViewController() {
  return viewController_.get();
}

content::WebContents* PageActionDecoration::GetCurrentWebContents() const {
  return owner_ ? owner_->GetWebContents() : nullptr;
}

void PageActionDecoration::UpdateState() {
  // If we have no owner, that means this class is still being constructed.
  WebContents* web_contents = owner_ ? owner_->GetWebContents() : NULL;
  if (web_contents) {
    UpdateVisibility(web_contents);
    owner_->RedrawDecoration(this);
  }
}
