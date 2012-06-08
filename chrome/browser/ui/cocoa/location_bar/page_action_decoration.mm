// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/ui/cocoa/location_bar/page_action_decoration.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/skia_utils_mac.h"

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
    Profile* profile,
    ExtensionAction* page_action)
    : owner_(NULL),
      profile_(profile),
      page_action_(page_action),
      tracker_(this),
      current_tab_id_(-1),
      preview_enabled_(false) {
  DCHECK(profile);
  const Extension* extension = profile->GetExtensionService()->
      GetExtensionById(page_action->extension_id(), false);
  DCHECK(extension);

  // Load all the icons declared in the manifest. This is the contents of the
  // icons array, plus the default_icon property, if any.
  std::vector<std::string> icon_paths(*page_action->icon_paths());
  if (!page_action_->default_icon_path().empty())
    icon_paths.push_back(page_action_->default_icon_path());

  for (std::vector<std::string>::iterator iter = icon_paths.begin();
       iter != icon_paths.end(); ++iter) {
    tracker_.LoadImage(extension, extension->GetResource(*iter),
                       gfx::Size(Extension::kPageActionIconMaxSize,
                                 Extension::kPageActionIconMaxSize),
                       ImageLoadingTracker::DONT_CACHE);
  }

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      content::Source<Profile>(profile_));

  // We set the owner last of all so that we can determine whether we are in
  // the process of initializing this class or not.
  owner_ = owner;
}

PageActionDecoration::~PageActionDecoration() {}

// Always |kPageActionIconMaxSize| wide.  |ImageDecoration| draws the
// image centered.
CGFloat PageActionDecoration::GetWidthForSpace(CGFloat width) {
  return Extension::kPageActionIconMaxSize;
}

bool PageActionDecoration::AcceptsMousePress() {
  return true;
}

// Either notify listeners or show a popup depending on the Page
// Action.
bool PageActionDecoration::OnMousePressed(NSRect frame) {
  TabContents* tab_contents = owner_->GetTabContents();
  if (!tab_contents) {
    // We don't want other code to try and handle this click. Returning true
    // prevents this by indicating that we handled it.
    return true;
  }

  LocationBarController* controller =
      tab_contents->extension_tab_helper()->location_bar_controller();

  // 1 is left click.
  switch (controller->OnClicked(page_action_->extension_id(), 1)) {
    case LocationBarController::ACTION_NONE:
      break;

    case LocationBarController::ACTION_SHOW_POPUP: {
      // Anchor popup at the bottom center of the page action icon.
      AutocompleteTextField* field = owner_->GetAutocompleteTextField();
      NSPoint anchor = GetBubblePointInFrame(frame);
      anchor = [field convertPoint:anchor toView:nil];

      const GURL popup_url(page_action_->GetPopupUrl(current_tab_id_));
      [ExtensionPopupController showURL:popup_url
                              inBrowser:browser::GetLastActiveBrowser()
                             anchoredAt:anchor
                          arrowLocation:info_bubble::kTopRight
                                devMode:NO];
      break;
    }

    case LocationBarController::ACTION_SHOW_CONTEXT_MENU:
      // We are never passing OnClicked a right-click button, so assume that
      // we're never going to be asked to show a context menu.
      // TODO(kalman): if this changes, update this class to pass the real
      // mouse button through to the LocationBarController.
      NOTREACHED();
      break;
  }

  return true;
}

void PageActionDecoration::OnImageLoaded(const gfx::Image& image,
                                         const std::string& extension_id,
                                         int index) {
  // We loaded icons()->size() icons, plus one extra if the Page Action had
  // a default icon.
  int total_icons = static_cast<int>(page_action_->icon_paths()->size());
  if (!page_action_->default_icon_path().empty())
    total_icons++;
  DCHECK(index < total_icons);

  // Map the index of the loaded image back to its name. If we ever get an
  // index greater than the number of icons, it must be the default icon.
  if (!image.IsEmpty()) {
    const SkBitmap* bitmap = image.ToSkBitmap();
    if (index < static_cast<int>(page_action_->icon_paths()->size()))
      page_action_icons_[page_action_->icon_paths()->at(index)] = *bitmap;
    else
      page_action_icons_[page_action_->default_icon_path()] = *bitmap;
  }

  // If we have no owner, that means this class is still being constructed and
  // we should not UpdatePageActions, since it leads to the PageActions being
  // destroyed again and new ones recreated (causing an infinite loop).
  if (owner_)
    owner_->UpdatePageActions();
}

void PageActionDecoration::UpdateVisibility(WebContents* contents,
                                            const GURL& url) {
  // Save this off so we can pass it back to the extension when the action gets
  // executed. See PageActionDecoration::OnMousePressed.
  current_tab_id_ = contents ? ExtensionTabUtil::GetTabId(contents) : -1;
  current_url_ = url;

  bool visible = contents &&
      (preview_enabled_ || page_action_->GetIsVisible(current_tab_id_));
  if (visible) {
    SetToolTip(page_action_->GetTitle(current_tab_id_));

    // Set the image.
    // It can come from three places. In descending order of priority:
    // - The developer can set it dynamically by path or bitmap. It will be in
    //   page_action_->GetIcon().
    // - The developer can set it dynamically by index. It will be in
    //   page_action_->GetIconIndex().
    // - It can be set in the manifest by path. It will be in page_action_->
    //   default_icon_path().

    // First look for a dynamically set bitmap.
    SkBitmap skia_icon = page_action_->GetIcon(current_tab_id_);
    if (skia_icon.isNull()) {
      int icon_index = page_action_->GetIconIndex(current_tab_id_);
      std::string icon_path = (icon_index < 0) ?
          page_action_->default_icon_path() :
          page_action_->icon_paths()->at(icon_index);
      if (!icon_path.empty()) {
        PageActionMap::iterator iter = page_action_icons_.find(icon_path);
        if (iter != page_action_icons_.end())
          skia_icon = iter->second;
      }
    }
    if (!skia_icon.isNull()) {
      SetImage(gfx::SkBitmapToNSImage(skia_icon));
    } else if (!GetImage()) {
      // During install the action can be displayed before the icons
      // have come in.  Rather than deal with this in multiple places,
      // provide a placeholder image.  This will be replaced when an
      // icon comes in.
      const NSSize default_size = NSMakeSize(Extension::kPageActionIconMaxSize,
                                             Extension::kPageActionIconMaxSize);
      SetImage([[NSImage alloc] initWithSize:default_size]);
    }
  }

  if (IsVisible() != visible) {
    SetVisible(visible);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
        content::Source<ExtensionAction>(page_action_),
        content::Details<WebContents>(contents));
  }
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
      NSHeight(frame) - Extension::kPageActionIconMaxSize;
  const CGFloat bottom_inset = std::ceil(delta_height / 2.0);

  // Return a point just below the bottom of the maximal drawing area.
  return NSMakePoint(NSMidX(frame),
                     NSMaxY(frame) - bottom_inset + kBubblePointYOffset);
}

NSMenu* PageActionDecoration::GetMenu() {
  if (!profile_)
    return nil;
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return nil;
  const Extension* extension = service->GetExtensionById(
      page_action_->extension_id(), false);
  DCHECK(extension);
  if (!extension)
    return nil;
  menu_.reset([[ExtensionActionContextMenu alloc]
      initWithExtension:extension
                profile:profile_
        extensionAction:page_action_]);

  return menu_.get();
}

void PageActionDecoration::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
      ExtensionPopupController* popup = [ExtensionPopupController popup];
      if (popup && ![popup isClosing])
        [popup close];

      break;
    }
    default:
      NOTREACHED() << "Unexpected notification";
      break;
  }
}
