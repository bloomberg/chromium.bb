// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_resource.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "views/controls/menu/menu_2.h"

PageActionImageView::PageActionImageView(LocationBarView* owner,
                                         Profile* profile,
                                         ExtensionAction* page_action)
    : owner_(owner),
      profile_(profile),
      page_action_(page_action),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)),
      current_tab_id_(-1),
      preview_enabled_(false),
      popup_(NULL) {
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

  set_accessibility_focusable(true);
}

PageActionImageView::~PageActionImageView() {
  if (popup_)
    HidePopup();
}

void PageActionImageView::ExecuteAction(int button,
                                        bool inspect_with_devtools) {
  if (current_tab_id_ < 0) {
    NOTREACHED() << "No current tab.";
    return;
  }

  if (page_action_->HasPopup(current_tab_id_)) {
    // In tests, GetLastActive could return NULL, so we need to have
    // a fallback.
    // TODO(erikkay): Find a better way to get the Browser that this
    // button is in.
    Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
    if (!browser)
      browser = BrowserList::FindBrowserWithProfile(profile_);
    DCHECK(browser);

    bool popup_showing = popup_ != NULL;

    // Always hide the current popup. Only one popup at a time.
    HidePopup();

    // If we were already showing, then treat this click as a dismiss.
    if (popup_showing)
      return;

    gfx::Rect screen_bounds(GetImageBounds());
    gfx::Point origin(screen_bounds.origin());
    View::ConvertPointToScreen(this, &origin);
    screen_bounds.set_origin(origin);

    BubbleBorder::ArrowLocation arrow_location = base::i18n::IsRTL() ?
        BubbleBorder::TOP_LEFT : BubbleBorder::TOP_RIGHT;

    popup_ = ExtensionPopup::Show(
        page_action_->GetPopupUrl(current_tab_id_),
        browser,
        screen_bounds,
        arrow_location,
        inspect_with_devtools,
        this);  // ExtensionPopup::Observer
  } else {
    ExtensionService* service = profile_->GetExtensionService();
    service->browser_event_router()->PageActionExecuted(
        profile_, page_action_->extension_id(), page_action_->id(),
        current_tab_id_, current_url_.spec(), button);
  }
}

void PageActionImageView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
}

bool PageActionImageView::OnMousePressed(const views::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.  (Also, triggering on mouse press causes bugs like
  // http://crbug.com/33155.)
  return true;
}

void PageActionImageView::OnMouseReleased(const views::MouseEvent& event,
                                          bool canceled) {
  if (canceled || !HitTest(event.location()))
    return;

  int button = -1;
  if (event.IsLeftMouseButton()) {
    button = 1;
  } else if (event.IsMiddleMouseButton()) {
    button = 2;
  } else if (event.IsRightMouseButton()) {
    // Get the top left point of this button in screen coordinates.
    gfx::Point menu_origin;
    ConvertPointToScreen(this, &menu_origin);
    // Make the menu appear below the button.
    menu_origin.Offset(0, height());
    ShowContextMenu(menu_origin, true);
    return;
  }

  ExecuteAction(button, false);  // inspect_with_devtools
}

bool PageActionImageView::OnKeyPressed(const views::KeyEvent& e) {
  if (e.key_code() == ui::VKEY_SPACE || e.key_code() == ui::VKEY_RETURN) {
    ExecuteAction(1, false);
    return true;
  }
  return false;
}

void PageActionImageView::ShowContextMenu(const gfx::Point& p,
                                          bool is_mouse_gesture) {
  const Extension* extension = profile_->GetExtensionService()->
      GetExtensionById(page_action()->extension_id(), false);
  if (!extension->ShowConfigureContextMenus())
    return;

  Browser* browser = BrowserView::GetBrowserViewForNativeWindow(
      platform_util::GetTopLevel(GetWidget()->GetNativeView()))->browser();
  context_menu_contents_ =
      new ExtensionContextMenuModel(extension, browser, this);
  context_menu_menu_.reset(new views::Menu2(context_menu_contents_.get()));
  context_menu_menu_->RunContextMenuAt(p);
}

void PageActionImageView::OnImageLoaded(
    SkBitmap* image, ExtensionResource resource, int index) {
  // We loaded icons()->size() icons, plus one extra if the page action had
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

  // During object construction (before the parent has been set) we are already
  // in a UpdatePageActions call, so we don't need to start another one (and
  // doing so causes crash described in http://crbug.com/57333).
  if (parent())
    owner_->UpdatePageActions();
}

void PageActionImageView::UpdateVisibility(TabContents* contents,
                                           const GURL& url) {
  // Save this off so we can pass it back to the extension when the action gets
  // executed. See PageActionImageView::OnMousePressed.
  current_tab_id_ = contents ? ExtensionTabUtil::GetTabId(contents) : -1;
  current_url_ = url;

  if (!contents ||
      (!preview_enabled_ && !page_action_->GetIsVisible(current_tab_id_))) {
    SetVisible(false);
    return;
  }

  // Set the tooltip.
  tooltip_ = page_action_->GetTitle(current_tab_id_);
  SetTooltipText(UTF8ToWide(tooltip_));

  // Set the image.
  // It can come from three places. In descending order of priority:
  // - The developer can set it dynamically by path or bitmap. It will be in
  //   page_action_->GetIcon().
  // - The developer can set it dynamically by index. It will be in
  //   page_action_->GetIconIndex().
  // - It can be set in the manifest by path. It will be in
  //   page_action_->default_icon_path().

  // First look for a dynamically set bitmap.
  SkBitmap icon = page_action_->GetIcon(current_tab_id_);
  if (icon.isNull()) {
    int icon_index = page_action_->GetIconIndex(current_tab_id_);
    std::string icon_path = (icon_index < 0) ?
        page_action_->default_icon_path() :
        page_action_->icon_paths()->at(icon_index);
    if (!icon_path.empty()) {
      PageActionMap::iterator iter = page_action_icons_.find(icon_path);
      if (iter != page_action_icons_.end())
        icon = iter->second;
    }
  }
  if (!icon.isNull())
    SetImage(&icon);

  SetVisible(true);
}

void PageActionImageView::InspectPopup(ExtensionAction* action) {
  ExecuteAction(1,  // left-click
                true);  // inspect_with_devtools
}

void PageActionImageView::ExtensionPopupIsClosing(ExtensionPopup* popup) {
  DCHECK_EQ(popup_, popup);
  // ExtensionPopup is ref-counted, so we don't need to delete it.
  popup_ = NULL;
}

void PageActionImageView::HidePopup() {
  if (popup_)
    popup_->Close();
}
