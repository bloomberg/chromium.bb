// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/extension_command_service.h"
#include "chrome/browser/extensions/api/commands/extension_command_service_factory.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

using content::WebContents;

PageActionImageView::PageActionImageView(LocationBarView* owner,
                                         ExtensionAction* page_action,
                                         Browser* browser)
    : owner_(owner),
      page_action_(page_action),
      browser_(browser),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)),
      current_tab_id_(-1),
      preview_enabled_(false),
      popup_(NULL) {
  const Extension* extension = owner_->profile()->GetExtensionService()->
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

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(
                     owner_->profile()->GetOriginalProfile()));

  set_accessibility_focusable(true);

  ExtensionCommandService* command_service =
      ExtensionCommandServiceFactory::GetForProfile(browser_->profile());
  const extensions::Command* page_action_command =
      command_service->GetActivePageActionCommand(extension->id());
  if (page_action_command) {
    keybinding_.reset(new ui::Accelerator(page_action_command->accelerator()));
    owner_->GetFocusManager()->RegisterAccelerator(
        *keybinding_.get(), ui::AcceleratorManager::kHighPriority, this);
  }
}

PageActionImageView::~PageActionImageView() {
  if (keybinding_.get() && owner_->GetFocusManager())
    owner_->GetFocusManager()->UnregisterAccelerator(*keybinding_.get(), this);

  if (popup_)
    popup_->GetWidget()->RemoveObserver(this);
  HidePopup();
}

void PageActionImageView::ExecuteAction(int button) {
  if (current_tab_id_ < 0) {
    NOTREACHED() << "No current tab.";
    return;
  }

  if (page_action_->HasPopup(current_tab_id_)) {
    bool popup_showing = popup_ != NULL;

    // Always hide the current popup. Only one popup at a time.
    HidePopup();

    // If we were already showing, then treat this click as a dismiss.
    if (popup_showing)
      return;

    views::BubbleBorder::ArrowLocation arrow_location = base::i18n::IsRTL() ?
        views::BubbleBorder::TOP_LEFT : views::BubbleBorder::TOP_RIGHT;

    popup_ = ExtensionPopup::ShowPopup(
        page_action_->GetPopupUrl(current_tab_id_),
        browser_,
        this,
        arrow_location);
    popup_->GetWidget()->AddObserver(this);
  } else {
    Profile* profile = owner_->profile();
    ExtensionService* service = profile->GetExtensionService();
    service->browser_event_router()->PageActionExecuted(
        profile, page_action_->extension_id(), page_action_->id(),
        current_tab_id_, current_url_.spec(), button);
  }
}

void PageActionImageView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = UTF8ToUTF16(tooltip_);
}

bool PageActionImageView::OnMousePressed(const views::MouseEvent& event) {
  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.  (Also, triggering on mouse press causes bugs like
  // http://crbug.com/33155.)
  return true;
}

void PageActionImageView::OnMouseReleased(const views::MouseEvent& event) {
  if (!HitTest(event.location()))
    return;

  int button = -1;
  if (event.IsLeftMouseButton()) {
    button = 1;
  } else if (event.IsMiddleMouseButton()) {
    button = 2;
  } else if (event.IsRightMouseButton()) {
    ShowContextMenu(gfx::Point(), true);
    return;
  }

  ExecuteAction(button);
}

bool PageActionImageView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    ExecuteAction(1);
    return true;
  }
  return false;
}

void PageActionImageView::ShowContextMenu(const gfx::Point& p,
                                          bool is_mouse_gesture) {
  const Extension* extension = owner_->profile()->GetExtensionService()->
      GetExtensionById(page_action()->extension_id(), false);
  if (!extension->ShowConfigureContextMenus())
    return;

  scoped_refptr<ExtensionContextMenuModel> context_menu_model(
      new ExtensionContextMenuModel(extension, browser_));
  views::MenuModelAdapter menu_model_adapter(context_menu_model.get());
  menu_runner_.reset(new views::MenuRunner(menu_model_adapter.CreateMenu()));
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(this, &screen_loc);
  if (menu_runner_->RunMenuAt(GetWidget(), NULL, gfx::Rect(screen_loc, size()),
          views::MenuItemView::TOPLEFT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
}

void PageActionImageView::OnImageLoaded(const gfx::Image& image,
                                        const std::string& extension_id,
                                        int index) {
  // We loaded icons()->size() icons, plus one extra if the page action had
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

  // During object construction (before the parent has been set) we are already
  // in a UpdatePageActions call, so we don't need to start another one (and
  // doing so causes crash described in http://crbug.com/57333).
  if (parent())
    owner_->UpdatePageActions();
}

bool PageActionImageView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK(visible());  // Should not have happened due to CanHandleAccelerator.

  ExecuteAction(1);  // 1 means left-click.
  return true;
}

bool PageActionImageView::CanHandleAccelerators() const {
  // While visible, we don't handle accelerators and while so we also don't
  // count as a priority accelerator handler.
  return visible();
}

void PageActionImageView::UpdateVisibility(WebContents* contents,
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
  SetTooltipText(UTF8ToUTF16(tooltip_));

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
    SetImage(icon);

  SetVisible(true);
}

void PageActionImageView::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(popup_->GetWidget(), widget);
  popup_->GetWidget()->RemoveObserver(this);
  popup_ = NULL;
}

void PageActionImageView::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_EXTENSION_UNLOADED, type);
  const Extension* unloaded_extension =
      content::Details<UnloadedExtensionInfo>(details)->extension;
  if (page_action_ == unloaded_extension ->page_action())
    owner_->UpdatePageActions();
}

void PageActionImageView::HidePopup() {
  if (popup_)
    popup_->GetWidget()->Close();
}
