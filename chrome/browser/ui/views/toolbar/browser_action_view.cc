// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_action_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"

using extensions::Extension;
using views::LabelButtonBorder;

namespace {

// We have smaller insets than normal STYLE_TEXTBUTTON buttons so that we can
// fit user supplied icons in without clipping them.
const int kBorderInset = 4;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserActionView

BrowserActionView::BrowserActionView(const Extension* extension,
                                     Browser* browser,
                                     BrowserActionView::Delegate* delegate)
    : browser_(browser),
      delegate_(delegate),
      button_(NULL),
      extension_(extension) {
  set_id(VIEW_ID_BROWSER_ACTION);
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
  return button_->GetIconWithBadge();
}

void BrowserActionView::Layout() {
  button_->SetBounds(0, 0, width(), height());
}

void BrowserActionView::GetAccessibleState(ui::AXViewState* state) {
  state->name = l10n_util::GetStringUTF16(
      IDS_ACCNAME_EXTENSIONS_BROWSER_ACTION);
  state->role = ui::AX_ROLE_GROUP;
}

gfx::Size BrowserActionView::GetPreferredSize() const {
  return gfx::Size(BrowserActionsContainer::IconWidth(false),
                   BrowserActionsContainer::IconHeight());
}

void BrowserActionView::PaintChildren(gfx::Canvas* canvas,
                                      const views::CullSet& cull_set) {
  View::PaintChildren(canvas, cull_set);
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
    : MenuButton(this, base::string16(), NULL, false),
      browser_(browser),
      browser_action_(
          extensions::ExtensionActionManager::Get(browser->profile())->
          GetBrowserAction(*extension)),
      extension_(extension),
      icon_factory_(browser->profile(), extension, browser_action_, this),
      delegate_(delegate),
      called_registered_extension_command_(false),
      icon_observer_(NULL) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
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

  // We also listen for browser theme changes on linux because a switch from or
  // to GTK requires that we regrab our browser action images.
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(browser->profile())));
}

void BrowserActionButton::Destroy() {
  MaybeUnregisterExtensionCommand(false);

  if (menu_runner_) {
    menu_runner_->Cancel();
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

void BrowserActionButton::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && !called_registered_extension_command_ &&
      GetFocusManager()) {
    MaybeRegisterExtensionCommand();
    called_registered_extension_command_ = true;
  }

  MenuButton::ViewHierarchyChanged(details);
}

bool BrowserActionButton::CanHandleAccelerators() const {
  // View::CanHandleAccelerators() checks to see if the view is visible before
  // allowing it to process accelerators. This is not appropriate for browser
  // actions buttons, which can be hidden inside the overflow area.
  return true;
}

void BrowserActionButton::GetAccessibleState(ui::AXViewState* state) {
  views::MenuButton::GetAccessibleState(state);
  state->role = ui::AX_ROLE_BUTTON;
}

void BrowserActionButton::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  delegate_->OnBrowserActionExecuted(this);
}

void BrowserActionButton::ShowContextMenuForView(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (!extension()->ShowConfigureContextMenus())
    return;

  SetButtonPushed();

  // Reconstructs the menu every time because the menu's contents are dynamic.
  scoped_refptr<ExtensionContextMenuModel> context_menu_contents(
      new ExtensionContextMenuModel(extension(), browser_, delegate_));
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(this, &screen_loc);

  views::Widget* parent = NULL;
  int run_types = views::MenuRunner::HAS_MNEMONICS |
                  views::MenuRunner::CONTEXT_MENU;
  if (delegate_->ShownInsideMenu()) {
    run_types |= views::MenuRunner::IS_NESTED;
    // RunMenuAt expects a nested menu to be parented by the same widget as the
    // already visible menu, in this case the Chrome menu.
    parent = BrowserView::GetBrowserViewForBrowser(browser_)->toolbar()
                                                            ->app_menu()
                                                            ->GetWidget();
  } else {
    parent = GetWidget();
  }

  menu_runner_.reset(
      new views::MenuRunner(context_menu_contents.get(), run_types));

  if (menu_runner_->RunMenuAt(parent,
                              NULL,
                              gfx::Rect(screen_loc, size()),
                              views::MENU_ANCHOR_TOPLEFT,
                              source_type) ==
      views::MenuRunner::MENU_DELETED) {
    return;
  }

  menu_runner_.reset();
  SetButtonNotPushed();
}

void BrowserActionButton::UpdateState() {
  int tab_id = delegate_->GetCurrentTabId();
  if (tab_id < 0)
    return;

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

    ThemeService* theme =
        ThemeServiceFactory::GetForProfile(browser_->profile());

    gfx::ImageSkia bg = *theme->GetImageSkiaNamed(IDR_BROWSER_ACTION);
    SetImage(views::Button::STATE_NORMAL,
             gfx::ImageSkiaOperations::CreateSuperimposedImage(bg, icon));
  }

  // If the browser action name is empty, show the extension name instead.
  std::string title = browser_action()->GetTitle(tab_id);
  base::string16 name =
      base::UTF8ToUTF16(title.empty() ? extension()->name() : title);
  SetTooltipText(name);
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
              extensions::manifest_values::kBrowserActionCommandEvent) {
        if (type == chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED)
          MaybeRegisterExtensionCommand();
        else
          MaybeUnregisterExtensionCommand(true);
      }
      break;
    }
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      UpdateState();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void BrowserActionButton::OnIconUpdated() {
  UpdateState();
  if (icon_observer_)
    icon_observer_->OnIconUpdated(GetIconWithBadge());
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
                       LabelButton::OnMousePressed(event);
  }

  if (!views::View::ShouldShowContextMenuOnMousePress()) {
    // See comments in MenuButton::Activate() as to why this is needed.
    SetMouseHandler(NULL);

    ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);
  }
  return false;
}

void BrowserActionButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (IsPopup() || menu_runner_) {
    // TODO(erikkay) this never actually gets called (probably because of the
    // loss of focus).
    MenuButton::OnMouseReleased(event);
  } else {
    LabelButton::OnMouseReleased(event);
  }
}

void BrowserActionButton::OnMouseExited(const ui::MouseEvent& event) {
  if (IsPopup() || menu_runner_)
    MenuButton::OnMouseExited(event);
  else
    LabelButton::OnMouseExited(event);
}

bool BrowserActionButton::OnKeyReleased(const ui::KeyEvent& event) {
  return IsPopup() ? MenuButton::OnKeyReleased(event) :
                     LabelButton::OnKeyReleased(event);
}

void BrowserActionButton::OnGestureEvent(ui::GestureEvent* event) {
  if (IsPopup())
    MenuButton::OnGestureEvent(event);
  else
    LabelButton::OnGestureEvent(event);
}

scoped_ptr<LabelButtonBorder> BrowserActionButton::CreateDefaultBorder() const {
  scoped_ptr<LabelButtonBorder> border = LabelButton::CreateDefaultBorder();
  border->set_insets(gfx::Insets(kBorderInset, kBorderInset,
                                 kBorderInset, kBorderInset));
  return border.Pass();
}

bool BrowserActionButton::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // Normal priority shortcuts must be handled via standard browser commands to
  // be processed at the proper time.
  if (GetAcceleratorPriority(accelerator, extension_) ==
      ui::AcceleratorManager::kNormalPriority)
    return false;

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

gfx::ImageSkia BrowserActionButton::GetIconWithBadge() {
  int tab_id = delegate_->GetCurrentTabId();
  gfx::Size spacing(0, ToolbarView::kVertSpacing);
  gfx::ImageSkia icon = *icon_factory_.GetIcon(tab_id).ToImageSkia();
  if (!IsEnabled(tab_id))
    icon = gfx::ImageSkiaOperations::CreateTransparentImage(icon, .25);
  return browser_action_->GetIconWithBadge(icon, tab_id, spacing);
}

gfx::ImageSkia BrowserActionButton::GetIconForTest() {
  return GetImage(views::Button::STATE_NORMAL);
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
        *keybinding_.get(),
        GetAcceleratorPriority(browser_action_command.accelerator(),
                               extension_),
        this);
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
    keybinding_.reset(NULL);
  }
}
