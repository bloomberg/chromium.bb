// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar/page_action_decoration.h"

#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/extensions/extension_action_context_menu.h"
#import "chrome/browser/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "skia/ext/skia_utils_mac.h"

PageActionDecoration::PageActionDecoration(
    LocationBarViewMac* owner,
    Profile* profile,
    ExtensionAction* page_action)
    : owner_(owner),
      profile_(profile),
      page_action_(page_action),
      tracker_(this),
      current_tab_id_(-1),
      preview_enabled_(false) {
  Extension* extension = profile->GetExtensionsService()->GetExtensionById(
      page_action->extension_id(), false);
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

  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      Source<Profile>(profile_));
}

PageActionDecoration::~PageActionDecoration() {
}

// Overridden from LocationBarImageView. Either notify listeners or show a
// popup depending on the Page Action.
bool PageActionDecoration::OnMousePressed(NSRect frame) {
  if (current_tab_id_ < 0) {
    NOTREACHED() << "No current tab.";
    // We don't want other code to try and handle this click.  Returning true
    // prevents this by indicating that we handled it.
    return true;
  }

  if (page_action_->HasPopup(current_tab_id_)) {
    // Anchor popup at the bottom center of the page action icon.
    AutocompleteTextField* field = owner_->GetAutocompleteTextField();
    NSPoint anchor = GetBubblePointInFrame(frame);
    anchor = [field convertPoint:anchor toView:nil];

    const GURL popup_url(page_action_->GetPopupUrl(current_tab_id_));
    [ExtensionPopupController showURL:popup_url
                            inBrowser:BrowserList::GetLastActive()
                           anchoredAt:anchor
                        arrowLocation:info_bubble::kTopRight
                              devMode:NO];
  } else {
    ExtensionBrowserEventRouter::GetInstance()->PageActionExecuted(
        profile_, page_action_->extension_id(), page_action_->id(),
        current_tab_id_, current_url_.spec(),
        1);
  }
  return true;
}

void PageActionDecoration::OnImageLoaded(
    SkBitmap* image, ExtensionResource resource, int index) {
  // We loaded icons()->size() icons, plus one extra if the Page Action had
  // a default icon.
  int total_icons = static_cast<int>(page_action_->icon_paths()->size());
  if (!page_action_->default_icon_path().empty())
    total_icons++;
  DCHECK(index < total_icons);

  // Map the index of the loaded image back to its name. If we ever get an
  // index greater than the number of icons, it must be the default icon.
  if (image) {
    if (index < static_cast<int>(page_action_->icon_paths()->size()))
      page_action_icons_[page_action_->icon_paths()->at(index)] = *image;
    else
      page_action_icons_[page_action_->default_icon_path()] = *image;
  }

  owner_->UpdatePageActions();
}

void PageActionDecoration::UpdateVisibility(
    TabContents* contents, const GURL& url) {
  // Save this off so we can pass it back to the extension when the action gets
  // executed. See PageActionDecoration::OnMousePressed.
  current_tab_id_ = ExtensionTabUtil::GetTabId(contents);
  current_url_ = url;

  bool visible = preview_enabled_ ||
                 page_action_->GetIsVisible(current_tab_id_);
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
      std::string icon_path;
      if (icon_index >= 0)
        icon_path = page_action_->icon_paths()->at(icon_index);
      else
        icon_path = page_action_->default_icon_path();

      if (!icon_path.empty()) {
        PageActionMap::iterator iter = page_action_icons_.find(icon_path);
        if (iter != page_action_icons_.end())
          skia_icon = iter->second;
      }
    }

    if (!skia_icon.isNull())
      SetImage(gfx::SkBitmapToNSImage(skia_icon));
  }
  if (IsVisible() != visible) {
    SetVisible(visible);
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
        Source<ExtensionAction>(page_action_),
        Details<TabContents>(contents));
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
  frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(frame), NSMaxY(frame));
}

NSMenu* PageActionDecoration::GetMenu() {
  if (!profile_)
    return nil;
  ExtensionsService* service = profile_->GetExtensionsService();
  if (!service)
    return nil;
  Extension* extension = service->GetExtensionById(
      page_action_->extension_id(), false);
  DCHECK(extension);
  if (!extension)
    return nil;
  return [[[ExtensionActionContextMenu alloc]
            initWithExtension:extension
                      profile:profile_
              extensionAction:page_action_] autorelease];
}

void PageActionDecoration::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
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
