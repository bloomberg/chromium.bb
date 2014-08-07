// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_action_view_controller.h"

#include "base/logging.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "chrome/browser/ui/views/extensions/extension_action_view_delegate.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using extensions::ActionInfo;
using extensions::CommandService;

ExtensionActionViewController::ExtensionActionViewController(
    const extensions::Extension* extension,
    Browser* browser,
    ExtensionAction* extension_action,
    ExtensionActionViewDelegate* delegate)
    : extension_(extension),
      browser_(browser),
      extension_action_(extension_action),
      delegate_(delegate),
      icon_factory_(browser->profile(), extension, extension_action, this),
      popup_(NULL) {
  DCHECK(extension_action->action_type() == ActionInfo::TYPE_PAGE ||
         extension_action->action_type() == ActionInfo::TYPE_BROWSER);
  DCHECK(extension);

}

ExtensionActionViewController::~ExtensionActionViewController() {
  HidePopup();
  UnregisterCommand(false);
}

void ExtensionActionViewController::InspectPopup() {
  ExecuteAction(ExtensionPopup::SHOW_AND_INSPECT, true);
}

void ExtensionActionViewController::ExecuteActionByUser() {
  ExecuteAction(ExtensionPopup::SHOW, true);
}

bool ExtensionActionViewController::ExecuteAction(
    ExtensionPopup::ShowAction show_action, bool grant_tab_permissions) {
  GURL popup_url;
  bool show_popup = false;
  if (extension_action_->action_type() == ActionInfo::TYPE_BROWSER) {
    extensions::ExtensionToolbarModel* toolbar_model =
        extensions::ExtensionToolbarModel::Get(browser_->profile());
    show_popup = toolbar_model->ExecuteBrowserAction(
                     extension_, browser_, &popup_url, grant_tab_permissions) ==
                 extensions::ExtensionToolbarModel::ACTION_SHOW_POPUP;
  } else {  // PageAction
    content::WebContents* web_contents = delegate_->GetCurrentWebContents();
    if (!web_contents)
      return false;
    extensions::LocationBarController* controller =
        extensions::TabHelper::FromWebContents(web_contents)->
            location_bar_controller();
    switch (controller->OnClicked(extension_action_)) {
      case extensions::LocationBarController::ACTION_NONE:
        break;
      case extensions::LocationBarController::ACTION_SHOW_POPUP:
        popup_url = extension_action_->GetPopupUrl(GetCurrentTabId());
        show_popup = true;
        break;
      case extensions::LocationBarController::ACTION_SHOW_CONTEXT_MENU:
        // We are never passing OnClicked a right-click button, so assume that
        // we're never going to be asked to show a context menu.
        // TODO(kalman): if this changes, update this class to pass the real
        // mouse button through to the LocationBarController.
        NOTREACHED();
        break;
    }
  }

  if (show_popup && ShowPopupWithUrl(show_action, popup_url)) {
    delegate_->OnPopupShown(grant_tab_permissions);
    return true;
  }

  return false;
}

void ExtensionActionViewController::HidePopup() {
  if (popup_)
    CleanupPopup(true);
}

gfx::Image ExtensionActionViewController::GetIcon(int tab_id) {
  return icon_factory_.GetIcon(tab_id);
}

int ExtensionActionViewController::GetCurrentTabId() const {
  content::WebContents* web_contents = delegate_->GetCurrentWebContents();
  return web_contents ? SessionID::IdForTab(web_contents) : -1;
}

void ExtensionActionViewController::RegisterCommand() {
  // If we've already registered, do nothing.
  if (action_keybinding_.get())
    return;

  extensions::Command extension_command;
  views::FocusManager* focus_manager =
      delegate_->GetFocusManagerForAccelerator();
  if (focus_manager && GetExtensionCommand(&extension_command)) {
    action_keybinding_.reset(
        new ui::Accelerator(extension_command.accelerator()));
    focus_manager->RegisterAccelerator(
        *action_keybinding_,
        GetAcceleratorPriority(extension_command.accelerator(), extension_),
        this);
  }
}

void ExtensionActionViewController::UnregisterCommand(bool only_if_removed) {
  views::FocusManager* focus_manager =
      delegate_->GetFocusManagerForAccelerator();
  if (!focus_manager || !action_keybinding_.get())
    return;

  // If |only_if_removed| is true, it means that we only need to unregister
  // ourselves as an accelerator if the command was removed. Otherwise, we need
  // to unregister ourselves no matter what (likely because we are shutting
  // down).
  extensions::Command extension_command;
  if (!only_if_removed || !GetExtensionCommand(&extension_command)) {
    focus_manager->UnregisterAccelerator(*action_keybinding_, this);
    action_keybinding_.reset();
  }
}

void ExtensionActionViewController::OnIconUpdated() {
  delegate_->OnIconUpdated();
}

bool ExtensionActionViewController::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // We shouldn't be handling any accelerators if the view is hidden, unless
  // this is a browser action.
  DCHECK(extension_action_->action_type() == ActionInfo::TYPE_BROWSER ||
         delegate_->GetAsView()->visible());

  // Normal priority shortcuts must be handled via standard browser commands to
  // be processed at the proper time.
  if (GetAcceleratorPriority(accelerator, extension()) ==
      ui::AcceleratorManager::kNormalPriority)
    return false;

  ExecuteActionByUser();
  return true;
}

bool ExtensionActionViewController::CanHandleAccelerators() const {
  // Page actions can only handle accelerators when they are visible.
  // Browser actions can handle accelerators even when not visible, since they
  // might be hidden in an overflow menu.
  return extension_action_->action_type() == ActionInfo::TYPE_PAGE ?
      delegate_->GetAsView()->visible() : true;
}

void ExtensionActionViewController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(popup_);
  DCHECK_EQ(popup_->GetWidget(), widget);
  CleanupPopup(false);
}

void ExtensionActionViewController::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (!extension_->ShowConfigureContextMenus())
    return;

  delegate_->OnWillShowContextMenus();

  // Reconstructs the menu every time because the menu's contents are dynamic.
  scoped_refptr<ExtensionContextMenuModel> context_menu_model(
      new ExtensionContextMenuModel(extension_, browser_, this));

  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(delegate_->GetAsView(), &screen_loc);

  int run_types = views::MenuRunner::HAS_MNEMONICS |
                  views::MenuRunner::CONTEXT_MENU;
  if (delegate_->IsShownInMenu())
    run_types |= views::MenuRunner::IS_NESTED;

  views::Widget* parent = delegate_->GetParentForContextMenu();

  menu_runner_.reset(
      new views::MenuRunner(context_menu_model.get(), run_types));

  if (menu_runner_->RunMenuAt(
          parent,
          NULL,
          gfx::Rect(screen_loc, delegate_->GetAsView()->size()),
          views::MENU_ANCHOR_TOPLEFT,
          source_type) == views::MenuRunner::MENU_DELETED) {
    return;
  }

  menu_runner_.reset();
  delegate_->OnContextMenuDone();
}

bool ExtensionActionViewController::ShowPopupWithUrl(
    ExtensionPopup::ShowAction show_action, const GURL& popup_url) {
  // If we're already showing the popup for this browser action, just hide it
  // and return.
  bool already_showing = popup_ != NULL;

  // Always hide the current popup, even if it's not the same.
  // Only one popup should be visible at a time.
  delegate_->HideActivePopup();
  if (already_showing)
    return false;

  views::BubbleBorder::Arrow arrow = base::i18n::IsRTL() ?
      views::BubbleBorder::TOP_LEFT : views::BubbleBorder::TOP_RIGHT;

  views::View* reference_view = delegate_->GetReferenceViewForPopup();

  popup_ = ExtensionPopup::ShowPopup(
               popup_url, browser_, reference_view, arrow, show_action);
  popup_->GetWidget()->AddObserver(this);

  return true;
}

bool ExtensionActionViewController::GetExtensionCommand(
    extensions::Command* command) {
  DCHECK(command);
  CommandService* command_service = CommandService::Get(browser_->profile());
  if (extension_action_->action_type() == ActionInfo::TYPE_PAGE) {
    return command_service->GetPageActionCommand(
        extension_->id(), CommandService::ACTIVE_ONLY, command, NULL);
  }
  return command_service->GetBrowserActionCommand(
      extension_->id(), CommandService::ACTIVE_ONLY, command, NULL);
}

void ExtensionActionViewController::CleanupPopup(bool close_widget) {
  DCHECK(popup_);
  delegate_->CleanupPopup();
  popup_->GetWidget()->RemoveObserver(this);
  if (close_widget)
    popup_->GetWidget()->Close();
  popup_ = NULL;
}
