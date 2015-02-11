// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_action_platform_delegate_views.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/command.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using extensions::ActionInfo;

namespace {

// The ExtensionActionPlatformDelegateViews which is currently showing its
// context menu, if any.
// Since only one context menu can be shown (even across browser windows), it's
// safe to have this be a global singleton.
ExtensionActionPlatformDelegateViews* context_menu_owner = NULL;

}  // namespace

// static
scoped_ptr<ExtensionActionPlatformDelegate>
ExtensionActionPlatformDelegate::Create(
    ExtensionActionViewController* controller) {
  return make_scoped_ptr(new ExtensionActionPlatformDelegateViews(controller));
}

ExtensionActionPlatformDelegateViews::ExtensionActionPlatformDelegateViews(
    ExtensionActionViewController* controller)
    : controller_(controller),
      weak_factory_(this) {
  content::NotificationSource notification_source = content::Source<Profile>(
      controller_->browser()->profile()->GetOriginalProfile());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED,
                 notification_source);
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_COMMAND_REMOVED,
                 notification_source);
}

ExtensionActionPlatformDelegateViews::~ExtensionActionPlatformDelegateViews() {
  if (context_menu_owner == this)
    context_menu_owner = nullptr;
  UnregisterCommand(false);
}

bool ExtensionActionPlatformDelegateViews::IsMenuRunning() const {
  return menu_runner_.get() != NULL;
}

void ExtensionActionPlatformDelegateViews::RegisterCommand() {
  // If we've already registered, do nothing.
  if (action_keybinding_.get())
    return;

  extensions::Command extension_command;
  views::FocusManager* focus_manager =
      GetDelegateViews()->GetFocusManagerForAccelerator();
  if (focus_manager && controller_->GetExtensionCommand(&extension_command)) {
    action_keybinding_.reset(
        new ui::Accelerator(extension_command.accelerator()));
    focus_manager->RegisterAccelerator(
        *action_keybinding_,
        GetAcceleratorPriority(extension_command.accelerator(),
                               controller_->extension()),
        this);
  }
}

void ExtensionActionPlatformDelegateViews::OnDelegateSet() {
  GetDelegateViews()->GetAsView()->set_context_menu_controller(this);
}

void ExtensionActionPlatformDelegateViews::CloseActivePopup() {
  if (controller_->extension_action()->action_type() ==
          ActionInfo::TYPE_BROWSER) {
    BrowserView::GetBrowserViewForBrowser(controller_->browser())->toolbar()->
        browser_actions()->HideActivePopup();
  } else {
    DCHECK_EQ(ActionInfo::TYPE_PAGE,
              controller_->extension_action()->action_type());
    // Page actions only know how to close their own popups.
    controller_->HidePopup();
  }
}

extensions::ExtensionViewHost*
ExtensionActionPlatformDelegateViews::ShowPopupWithUrl(
    ExtensionActionViewController::PopupShowAction show_action,
    const GURL& popup_url,
    bool grant_tab_permissions) {
  // TOP_RIGHT is correct for both RTL and LTR, because the views platform
  // performs the flipping in RTL cases.
  views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_RIGHT;

  views::View* reference_view = GetDelegateViews()->GetReferenceViewForPopup();

  ExtensionPopup::ShowAction popup_show_action =
      show_action == ExtensionActionViewController::SHOW_POPUP ?
          ExtensionPopup::SHOW : ExtensionPopup::SHOW_AND_INSPECT;
  ExtensionPopup* popup = ExtensionPopup::ShowPopup(popup_url,
                                                    controller_->browser(),
                                                    reference_view,
                                                    arrow,
                                                    popup_show_action);
  return popup->host();
}

void ExtensionActionPlatformDelegateViews::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED ||
         type == extensions::NOTIFICATION_EXTENSION_COMMAND_REMOVED);
  extensions::ExtensionCommandRemovedDetails* payload =
      content::Details<extensions::ExtensionCommandRemovedDetails>(details)
          .ptr();
  if (controller_->extension()->id() == payload->extension_id &&
      (payload->command_name ==
           extensions::manifest_values::kBrowserActionCommandEvent ||
       payload->command_name ==
           extensions::manifest_values::kPageActionCommandEvent)) {
    if (type == extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED)
      RegisterCommand();
    else
      UnregisterCommand(true);
  }
}

bool ExtensionActionPlatformDelegateViews::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // We shouldn't be handling any accelerators if the view is hidden, unless
  // this is a browser action.
  DCHECK(controller_->extension_action()->action_type() ==
             ActionInfo::TYPE_BROWSER ||
         GetDelegateViews()->GetAsView()->visible());

  // Normal priority shortcuts must be handled via standard browser commands to
  // be processed at the proper time.
  if (GetAcceleratorPriority(accelerator, controller_->extension()) ==
      ui::AcceleratorManager::kNormalPriority)
    return false;

  controller_->ExecuteAction(true);
  return true;
}

bool ExtensionActionPlatformDelegateViews::CanHandleAccelerators() const {
  // Page actions can only handle accelerators when they are visible.
  // Browser actions can handle accelerators even when not visible, since they
  // might be hidden in an overflow menu.
  return controller_->extension_action()->action_type() ==
      ActionInfo::TYPE_PAGE ? GetDelegateViews()->GetAsView()->visible() :
          true;
}

void ExtensionActionPlatformDelegateViews::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // If there's another active menu that won't be dismissed by opening this one,
  // then we can't show this one right away, since we can only show one nested
  // menu at a time.
  // If the other menu is an extension action's context menu, then we'll run
  // this one after that one closes. If it's a different type of menu, then we
  // close it and give up, for want of a better solution. (Luckily, this is
  // rare).
  // TODO(devlin): Update this when views code no longer runs menus in a nested
  // loop.
  if (context_menu_owner) {
    context_menu_owner->followup_context_menu_task_ =
        base::Bind(&ExtensionActionPlatformDelegateViews::DoShowContextMenu,
                   weak_factory_.GetWeakPtr(),
                   source_type);
  }
  if (CloseActiveMenuIfNeeded())
    return;

  // Otherwise, no other menu is showing, and we can proceed normally.
  DoShowContextMenu(source_type);
}

void ExtensionActionPlatformDelegateViews::DoShowContextMenu(
    ui::MenuSourceType source_type) {
  ui::MenuModel* context_menu_model = controller_->GetContextMenu();
  // It's possible the extension doesn't have a context menu.
  if (!context_menu_model)
    return;

  DCHECK(!context_menu_owner);
  context_menu_owner = this;

  // We shouldn't have both a popup and a context menu showing.
  CloseActivePopup();

  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(GetDelegateViews()->GetAsView(),
                                    &screen_loc);

  int run_types =
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;
  if (GetDelegateViews()->IsShownInMenu())
    run_types |= views::MenuRunner::IS_NESTED;

  views::Widget* parent = GetDelegateViews()->GetParentForContextMenu();

  menu_runner_.reset(new views::MenuRunner(context_menu_model, run_types));

  if (menu_runner_->RunMenuAt(
          parent,
          GetDelegateViews()->GetContextMenuButton(),
          gfx::Rect(screen_loc, GetDelegateViews()->GetAsView()->size()),
          views::MENU_ANCHOR_TOPLEFT,
          source_type) == views::MenuRunner::MENU_DELETED) {
    return;
  }

  context_menu_owner = NULL;
  menu_runner_.reset();

  // If another extension action wants to show its context menu, allow it to.
  if (!followup_context_menu_task_.is_null()) {
    base::Closure task = followup_context_menu_task_;
    followup_context_menu_task_ = base::Closure();
    task.Run();
  }
}

void ExtensionActionPlatformDelegateViews::UnregisterCommand(
    bool only_if_removed) {
  views::FocusManager* focus_manager =
      GetDelegateViews()->GetFocusManagerForAccelerator();
  if (!focus_manager || !action_keybinding_.get())
    return;

  // If |only_if_removed| is true, it means that we only need to unregister
  // ourselves as an accelerator if the command was removed. Otherwise, we need
  // to unregister ourselves no matter what (likely because we are shutting
  // down).
  extensions::Command extension_command;
  if (!only_if_removed ||
      !controller_->GetExtensionCommand(&extension_command)) {
    focus_manager->UnregisterAccelerator(*action_keybinding_, this);
    action_keybinding_.reset();
  }
}

bool ExtensionActionPlatformDelegateViews::CloseActiveMenuIfNeeded() {
  // If this view is shown inside another menu, there's a possibility that there
  // is another context menu showing that we have to close before we can
  // activate a different menu.
  if (GetDelegateViews()->IsShownInMenu()) {
    views::MenuController* menu_controller =
        views::MenuController::GetActiveInstance();
    // If this is shown inside a menu, then there should always be an active
    // menu controller.
    DCHECK(menu_controller);
    if (menu_controller->in_nested_run()) {
      // There is another menu showing. Close the outermost menu (since we are
      // shown in the same menu, we don't want to close the whole thing).
      menu_controller->Cancel(views::MenuController::EXIT_OUTERMOST);
      return true;
    }
  }

  return false;
}

ToolbarActionViewDelegateViews*
ExtensionActionPlatformDelegateViews::GetDelegateViews() const {
  return static_cast<ToolbarActionViewDelegateViews*>(
      controller_->view_delegate());
}
