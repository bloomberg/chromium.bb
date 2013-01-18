// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/browser_action_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events/event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

using extensions::Extension;

////////////////////////////////////////////////////////////////////////////////
// BrowserActionView

bool BrowserActionView::Delegate::NeedToShowMultipleIconStates() const {
  return true;
}

bool BrowserActionView::Delegate::NeedToShowTooltip() const {
  return true;
}

BrowserActionView::BrowserActionView(const Extension* extension,
                                     Browser* browser,
                                     BrowserActionView::Delegate* delegate)
    : browser_(browser),
      delegate_(delegate),
      button_(NULL),
      extension_(extension) {
  button_ = new BrowserActionButton(extension_, browser_, delegate_);
  button_->set_drag_controller(delegate_);
  button_->set_owned_by_client();
  AddChildView(button_);
  button_->UpdateState();
}

BrowserActionView::~BrowserActionView() {
  button_->Destroy();
}

gfx::ImageSkia BrowserActionView::GetIconWithBadge() {
  int tab_id = delegate_->GetCurrentTabId();

  const ExtensionAction* action =
      extensions::ExtensionActionManager::Get(browser_->profile())->
      GetBrowserAction(*button_->extension());
  gfx::Size spacing(0, ToolbarView::kVertSpacing);
  gfx::ImageSkia icon = *button_->icon_factory().GetIcon(tab_id).ToImageSkia();
  if (!button_->IsEnabled(tab_id))
    icon = gfx::ImageSkiaOperations::CreateTransparentImage(icon, .25);
  return action->GetIconWithBadge(icon, tab_id, spacing);
}

void BrowserActionView::Layout() {
  // We can't rely on button_->GetPreferredSize() here because that's not set
  // correctly until the first call to
  // BrowserActionsContainer::RefreshBrowserActionViews(), whereas this can be
  // called before that when the initial bounds are set (and then not after,
  // since the bounds don't change).  So instead of setting the height from the
  // button's preferred size, we use IconHeight(), since that's how big the
  // button should be regardless of what it's displaying.
  gfx::Point offset = delegate_->GetViewContentOffset();
  button_->SetBounds(offset.x(), offset.y(), width() - offset.x(),
                     BrowserActionsContainer::IconHeight());
}

void BrowserActionView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(
      IDS_ACCNAME_EXTENSIONS_BROWSER_ACTION);
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
}

gfx::Size BrowserActionView::GetPreferredSize() {
  return gfx::Size(BrowserActionsContainer::IconWidth(false),
                   BrowserActionsContainer::IconHeight());
}

void BrowserActionView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);
  ExtensionAction* action = button()->browser_action();
  int tab_id = delegate_->GetCurrentTabId();
  if (tab_id >= 0)
    action->PaintBadge(canvas, GetLocalBounds(), tab_id);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

BrowserActionButton::BrowserActionButton(const Extension* extension,
                                         Browser* browser,
                                         BrowserActionView::Delegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          MenuButton(this, string16(), NULL, false)),
      browser_(browser),
      browser_action_(
          extensions::ExtensionActionManager::Get(browser->profile())->
          GetBrowserAction(*extension)),
      extension_(extension),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          icon_factory_(extension, browser_action_, this)),
      delegate_(delegate),
      context_menu_(NULL),
      called_registered_extension_command_(false) {
  set_border(NULL);
  set_alignment(TextButton::ALIGN_CENTER);
  set_context_menu_controller(this);

  // No UpdateState() here because View hierarchy not setup yet. Our parent
  // should call UpdateState() after creation.

  content::NotificationSource notification_source =
      content::Source<Profile>(browser_->profile()->GetOriginalProfile());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
                 content::Source<ExtensionAction>(browser_action_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED,
                 notification_source);
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_COMMAND_REMOVED,
                 notification_source);
}

void BrowserActionButton::Destroy() {
  MaybeUnregisterExtensionCommand(false);

  if (context_menu_) {
    context_menu_->Cancel();
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

void BrowserActionButton::ViewHierarchyChanged(
    bool is_add, View* parent, View* child) {

  if (is_add && !called_registered_extension_command_ && GetFocusManager()) {
    MaybeRegisterExtensionCommand();
    called_registered_extension_command_ = true;
  }

  MenuButton::ViewHierarchyChanged(is_add, parent, child);
}

bool BrowserActionButton::CanHandleAccelerators() const {
  // View::CanHandleAccelerators() checks to see if the view is visible before
  // allowing it to process accelerators. This is not appropriate for browser
  // actions buttons, which can be hidden inside the overflow area.
  return true;
}

void BrowserActionButton::GetAccessibleState(ui::AccessibleViewState* state) {
  views::MenuButton::GetAccessibleState(state);
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
}

void BrowserActionButton::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  delegate_->OnBrowserActionExecuted(this);
}

void BrowserActionButton::ShowContextMenuForView(View* source,
                                                 const gfx::Point& point) {
  if (!extension()->ShowConfigureContextMenus())
    return;

  SetButtonPushed();

  // Reconstructs the menu every time because the menu's contents are dynamic.
  scoped_refptr<ExtensionContextMenuModel> context_menu_contents_(
      new ExtensionContextMenuModel(extension(), browser_, delegate_));
  views::MenuModelAdapter menu_model_adapter(context_menu_contents_.get());
  menu_runner_.reset(new views::MenuRunner(menu_model_adapter.CreateMenu()));

  context_menu_ = menu_runner_->GetMenu();
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(this, &screen_loc);
  if (menu_runner_->RunMenuAt(GetWidget(), NULL, gfx::Rect(screen_loc, size()),
          views::MenuItemView::TOPLEFT, views::MenuRunner::HAS_MNEMONICS |
          views::MenuRunner::CONTEXT_MENU) ==
      views::MenuRunner::MENU_DELETED) {
    return;
  }

  menu_runner_.reset();
  SetButtonNotPushed();
  context_menu_ = NULL;
}

void BrowserActionButton::UpdateState() {
  int tab_id = delegate_->GetCurrentTabId();
  if (tab_id < 0)
    return;

  SetShowMultipleIconStates(delegate_->NeedToShowMultipleIconStates());

  if (!IsEnabled(tab_id)) {
    SetState(views::CustomButton::STATE_DISABLED);
  } else {
    SetState(menu_visible_ ?
             views::CustomButton::STATE_PRESSED :
             views::CustomButton::STATE_NORMAL);
  }

  gfx::ImageSkia icon = *icon_factory_.GetIcon(tab_id).ToImageSkia();

  if (!icon.isNull()) {
    if (!browser_action()->GetIsVisible(tab_id))
      icon = gfx::ImageSkiaOperations::CreateTransparentImage(icon, .25);

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    gfx::ImageSkia bg = *rb.GetImageSkiaNamed(IDR_BROWSER_ACTION);
    SetIcon(gfx::ImageSkiaOperations::CreateSuperimposedImage(bg, icon));

    gfx::ImageSkia bg_h = *rb.GetImageSkiaNamed(IDR_BROWSER_ACTION_H);
    SetHoverIcon(gfx::ImageSkiaOperations::CreateSuperimposedImage(bg_h, icon));

    gfx::ImageSkia bg_p = *rb.GetImageSkiaNamed(IDR_BROWSER_ACTION_P);
    SetPushedIcon(
        gfx::ImageSkiaOperations::CreateSuperimposedImage(bg_p, icon));
  }

  // If the browser action name is empty, show the extension name instead.
  std::string title = browser_action()->GetTitle(tab_id);
  string16 name = UTF8ToUTF16(title.empty() ? extension()->name() : title);
  SetTooltipText(delegate_->NeedToShowTooltip() ? name : string16());
  SetAccessibleName(name);

  parent()->SchedulePaint();
}

bool BrowserActionButton::IsPopup() {
  int tab_id = delegate_->GetCurrentTabId();
  return (tab_id < 0) ? false : browser_action_->HasPopup(tab_id);
}

GURL BrowserActionButton::GetPopupUrl() {
  int tab_id = delegate_->GetCurrentTabId();
  return (tab_id < 0) ? GURL() : browser_action_->GetPopupUrl(tab_id);
}

void BrowserActionButton::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED:
      UpdateState();
      // The browser action may have become visible/hidden so we need to make
      // sure the state gets updated.
      delegate_->OnBrowserActionVisibilityChanged();
      break;
    case chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED:
    case chrome::NOTIFICATION_EXTENSION_COMMAND_REMOVED: {
      std::pair<const std::string, const std::string>* payload =
          content::Details<std::pair<const std::string, const std::string> >(
              details).ptr();
      if (extension_->id() == payload->first &&
          payload->second ==
              extension_manifest_values::kBrowserActionCommandEvent) {
        if (type == chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED)
          MaybeRegisterExtensionCommand();
        else
          MaybeUnregisterExtensionCommand(true);
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void BrowserActionButton::OnIconUpdated() {
  UpdateState();
}

bool BrowserActionButton::Activate() {
  if (!IsPopup())
    return true;

  delegate_->OnBrowserActionExecuted(this);

  // TODO(erikkay): Run a nested modal loop while the mouse is down to
  // enable menu-like drag-select behavior.

  // The return value of this method is returned via OnMousePressed.
  // We need to return false here since we're handing off focus to another
  // widget/view, and true will grab it right back and try to send events
  // to us.
  return false;
}

bool BrowserActionButton::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsRightMouseButton()) {
    return IsPopup() ? MenuButton::OnMousePressed(event) :
                       TextButton::OnMousePressed(event);
  }

  // See comments in MenuButton::Activate() as to why this is needed.
  SetMouseHandler(NULL);

  ShowContextMenu(gfx::Point(), true);
  return false;
}

void BrowserActionButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (IsPopup() || context_menu_) {
    // TODO(erikkay) this never actually gets called (probably because of the
    // loss of focus).
    MenuButton::OnMouseReleased(event);
  } else {
    TextButton::OnMouseReleased(event);
  }
}

void BrowserActionButton::OnMouseExited(const ui::MouseEvent& event) {
  if (IsPopup() || context_menu_)
    MenuButton::OnMouseExited(event);
  else
    TextButton::OnMouseExited(event);
}

bool BrowserActionButton::OnKeyReleased(const ui::KeyEvent& event) {
  return IsPopup() ? MenuButton::OnKeyReleased(event) :
                     TextButton::OnKeyReleased(event);
}

void BrowserActionButton::OnGestureEvent(ui::GestureEvent* event) {
  if (IsPopup())
    MenuButton::OnGestureEvent(event);
  else
    TextButton::OnGestureEvent(event);
}

bool BrowserActionButton::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  delegate_->OnBrowserActionExecuted(this);
  return true;
}

void BrowserActionButton::SetButtonPushed() {
  SetState(views::CustomButton::STATE_PRESSED);
  menu_visible_ = true;
}

void BrowserActionButton::SetButtonNotPushed() {
  SetState(views::CustomButton::STATE_NORMAL);
  menu_visible_ = false;
}

bool BrowserActionButton::IsEnabled(int tab_id) const {
  return browser_action_->GetIsVisible(tab_id);
}

gfx::ImageSkia BrowserActionButton::GetIconForTest() {
  return icon();
}

BrowserActionButton::~BrowserActionButton() {
}

void BrowserActionButton::MaybeRegisterExtensionCommand() {
  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser_->profile());
  extensions::Command browser_action_command;
  if (command_service->GetBrowserActionCommand(
          extension_->id(),
          extensions::CommandService::ACTIVE_ONLY,
          &browser_action_command,
          NULL)) {
    keybinding_.reset(new ui::Accelerator(
        browser_action_command.accelerator()));
    GetFocusManager()->RegisterAccelerator(
        *keybinding_.get(), ui::AcceleratorManager::kHighPriority, this);
  }
}

void BrowserActionButton::MaybeUnregisterExtensionCommand(bool only_if_active) {
  if (!keybinding_.get() || !GetFocusManager())
    return;

  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser_->profile());

  extensions::Command browser_action_command;
  if (!only_if_active || !command_service->GetBrowserActionCommand(
          extension_->id(),
          extensions::CommandService::ACTIVE_ONLY,
          &browser_action_command,
          NULL)) {
    GetFocusManager()->UnregisterAccelerator(*keybinding_.get(), this);
  }
}
