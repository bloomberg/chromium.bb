// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_action_view_controller.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using extensions::ActionInfo;
using extensions::CommandService;

namespace {

// The ExtensionActionViewController which is currently showing its context
// menu, if any.
// Since only one context menu can be shown (even across browser windows), it's
// safe to have this be a global singleton.
ExtensionActionViewController* context_menu_owner = NULL;

}  // namespace

ExtensionActionViewController::ExtensionActionViewController(
    const extensions::Extension* extension,
    Browser* browser,
    ExtensionAction* extension_action)
    : extension_(extension),
      browser_(browser),
      extension_action_(extension_action),
      delegate_(nullptr),
      icon_factory_(browser->profile(), extension, extension_action, this),
      icon_observer_(nullptr),
      popup_(nullptr),
      weak_factory_(this) {
  DCHECK(extension_action);
  DCHECK(extension_action->action_type() == ActionInfo::TYPE_PAGE ||
         extension_action->action_type() == ActionInfo::TYPE_BROWSER);
  DCHECK(extension);

  content::NotificationSource notification_source =
      content::Source<Profile>(browser->profile()->GetOriginalProfile());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED,
                 notification_source);
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_COMMAND_REMOVED,
                 notification_source);
}

ExtensionActionViewController::~ExtensionActionViewController() {
  if (context_menu_owner == this)
    context_menu_owner = NULL;
  HidePopup();
  UnregisterCommand(false);
}

const std::string& ExtensionActionViewController::GetId() const {
  return extension_->id();
}

void ExtensionActionViewController::SetDelegate(
    ToolbarActionViewDelegate* delegate) {
  delegate_ = delegate;
  delegate_->GetAsView()->set_context_menu_controller(this);
}

gfx::Image ExtensionActionViewController::GetIcon(
    content::WebContents* web_contents) {
  return icon_factory_.GetIcon(SessionTabHelper::IdForTab(web_contents));
}

gfx::ImageSkia ExtensionActionViewController::GetIconWithBadge() {
  content::WebContents* web_contents = delegate_->GetCurrentWebContents();
  gfx::Size spacing(0, ToolbarView::kVertSpacing);
  gfx::ImageSkia icon = *GetIcon(web_contents).ToImageSkia();
  if (!IsEnabled(web_contents))
    icon = gfx::ImageSkiaOperations::CreateTransparentImage(icon, .25);
  return extension_action_->GetIconWithBadge(
      icon, SessionTabHelper::IdForTab(web_contents), spacing);
}

base::string16 ExtensionActionViewController::GetAccessibleName(
    content::WebContents* web_contents) const {
  std::string title =
      extension_action()->GetTitle(SessionTabHelper::IdForTab(web_contents));
  return base::UTF8ToUTF16(title.empty() ? extension()->name() : title);
}

base::string16 ExtensionActionViewController::GetTooltip(
    content::WebContents* web_contents) const {
  return GetAccessibleName(web_contents);
}

bool ExtensionActionViewController::IsEnabled(
    content::WebContents* web_contents) const {
  return extension_action_->GetIsVisible(
      SessionTabHelper::IdForTab(web_contents));
}

bool ExtensionActionViewController::HasPopup(
    content::WebContents* web_contents) const {
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  return (tab_id < 0) ? false : extension_action_->HasPopup(tab_id);
}

void ExtensionActionViewController::HidePopup() {
  if (popup_)
    CleanupPopup(true);
}

gfx::NativeView ExtensionActionViewController::GetPopupNativeView() {
  return popup_ ? popup_->GetWidget()->GetNativeView() : nullptr;
}

bool ExtensionActionViewController::IsMenuRunning() const {
  return menu_runner_.get() != NULL;
}

bool ExtensionActionViewController::CanDrag() const {
  return true;
}

bool ExtensionActionViewController::ExecuteAction(bool by_user) {
  return ExecuteAction(ExtensionPopup::SHOW, by_user);
}

void ExtensionActionViewController::PaintExtra(
    gfx::Canvas* canvas,
    const gfx::Rect& bounds,
    content::WebContents* web_contents) const {
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  if (tab_id >= 0)
    extension_action_->PaintBadge(canvas, bounds, tab_id);
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

void ExtensionActionViewController::InspectPopup() {
  ExecuteAction(ExtensionPopup::SHOW_AND_INSPECT, true);
}

bool ExtensionActionViewController::ExecuteAction(
    ExtensionPopup::ShowAction show_action, bool grant_tab_permissions) {
  if (extensions::ExtensionActionAPI::Get(browser_->profile())->
          ExecuteExtensionAction(extension_, browser_, grant_tab_permissions) ==
      ExtensionAction::ACTION_SHOW_POPUP) {
    GURL popup_url = extension_action_->GetPopupUrl(
        SessionTabHelper::IdForTab(delegate_->GetCurrentWebContents()));
    return static_cast<ExtensionActionViewController*>(
        delegate_->GetPreferredPopupViewController())->ShowPopupWithUrl(
            show_action, popup_url, grant_tab_permissions);
  }
  return false;
}

void ExtensionActionViewController::OnIconUpdated() {
  delegate_->OnIconUpdated();
  if (icon_observer_)
    icon_observer_->OnIconUpdated();
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

  ExecuteAction(true);
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
        base::Bind(&ExtensionActionViewController::DoShowContextMenu,
                   weak_factory_.GetWeakPtr(),
                   source_type);
  }
  if (CloseActiveMenuIfNeeded())
    return;

  // Otherwise, no other menu is showing, and we can proceed normally.
  DoShowContextMenu(source_type);
}

void ExtensionActionViewController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED ||
         type == extensions::NOTIFICATION_EXTENSION_COMMAND_REMOVED);
  std::pair<const std::string, const std::string>* payload =
      content::Details<std::pair<const std::string, const std::string> >(
          details).ptr();
  if (extension_->id() == payload->first &&
      payload->second ==
          extensions::manifest_values::kBrowserActionCommandEvent) {
    if (type == extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED)
      RegisterCommand();
    else
      UnregisterCommand(true);
  }
}

void ExtensionActionViewController::DoShowContextMenu(
    ui::MenuSourceType source_type) {
  if (!extension_->ShowConfigureContextMenus())
    return;

  DCHECK(!context_menu_owner);
  context_menu_owner = this;

  // We shouldn't have both a popup and a context menu showing.
  delegate_->HideActivePopup();

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
          delegate_->GetContextMenuButton(),
          gfx::Rect(screen_loc, delegate_->GetAsView()->size()),
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

bool ExtensionActionViewController::ShowPopupWithUrl(
    ExtensionPopup::ShowAction show_action,
    const GURL& popup_url,
    bool grant_tab_permissions) {
  // If we're already showing the popup for this browser action, just hide it
  // and return.
  bool already_showing = popup_ != NULL;

  // Always hide the current popup, even if it's not the same.
  // Only one popup should be visible at a time.
  delegate_->HideActivePopup();

  // Similarly, don't allow a context menu and a popup to be showing
  // simultaneously.
  CloseActiveMenuIfNeeded();

  if (already_showing)
    return false;

  views::BubbleBorder::Arrow arrow = base::i18n::IsRTL() ?
      views::BubbleBorder::TOP_LEFT : views::BubbleBorder::TOP_RIGHT;

  views::View* reference_view = delegate_->GetReferenceViewForPopup();

  popup_ = ExtensionPopup::ShowPopup(
               popup_url, browser_, reference_view, arrow, show_action);
  popup_->GetWidget()->AddObserver(this);

  delegate_->OnPopupShown(grant_tab_permissions);

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

bool ExtensionActionViewController::CloseActiveMenuIfNeeded() {
  // If this view is shown inside another menu, there's a possibility that there
  // is another context menu showing that we have to close before we can
  // activate a different menu.
  if (delegate_->IsShownInMenu()) {
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

void ExtensionActionViewController::CleanupPopup(bool close_widget) {
  DCHECK(popup_);
  delegate_->CleanupPopup();
  popup_->GetWidget()->RemoveObserver(this);
  if (close_widget)
    popup_->GetWidget()->Close();
  popup_ = NULL;
}
