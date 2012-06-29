// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/browser_action_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/commands/command_service_factory.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

using extensions::Extension;

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

BrowserActionButton::BrowserActionButton(const Extension* extension,
                                         BrowserActionsContainer* panel)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          MenuButton(this, string16(), NULL, false)),
      browser_action_(extension->browser_action()),
      extension_(extension),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)),
      panel_(panel),
      context_menu_(NULL) {
  set_border(NULL);
  set_alignment(TextButton::ALIGN_CENTER);

  // No UpdateState() here because View hierarchy not setup yet. Our parent
  // should call UpdateState() after creation.

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
                 content::Source<ExtensionAction>(browser_action_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED,
                 content::Source<Profile>(
                     panel_->profile()->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_COMMAND_REMOVED,
                 content::Source<Profile>(
                     panel_->profile()->GetOriginalProfile()));
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
  if (is_add && child == this) {
    // The Browser Action API does not allow the default icon path to be
    // changed at runtime, so we can load this now and cache it.
    std::string relative_path = browser_action_->default_icon_path();
    if (!relative_path.empty()) {
      // LoadImage is not guaranteed to be synchronous, so we might see the
      // callback OnImageLoaded execute immediately. It (through UpdateState)
      // expects parent() to return the owner for this button, so this
      // function is as early as we can start this request.
      tracker_.LoadImage(extension_, extension_->GetResource(relative_path),
                         gfx::Size(Extension::kBrowserActionIconMaxSize,
                                   Extension::kBrowserActionIconMaxSize),
                         ImageLoadingTracker::DONT_CACHE);
    } else {
      // Set the icon to be the default extensions icon.
      default_icon_ = *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_EXTENSIONS_FAVICON).ToSkBitmap();
      UpdateState();
    }

    MaybeRegisterExtensionCommand();
  }

  MenuButton::ViewHierarchyChanged(is_add, parent, child);
}

bool BrowserActionButton::CanHandleAccelerators() const {
  // View::CanHandleAccelerators() checks to see if the view is visible before
  // allowing it to process accelerators. This is not appropriate for browser
  // actions buttons, which can be hidden inside the overflow area.
  return true;
}

void BrowserActionButton::ButtonPressed(views::Button* sender,
                                        const views::Event& event) {
  panel_->OnBrowserActionExecuted(this);
}

void BrowserActionButton::OnImageLoaded(const gfx::Image& image,
                                        const std::string& extension_id,
                                        int index) {
  if (!image.IsEmpty())
    default_icon_ = *image.ToSkBitmap();

  // Call back to UpdateState() because a more specific icon might have been set
  // while the load was outstanding.
  UpdateState();
}

void BrowserActionButton::UpdateState() {
  int tab_id = panel_->GetCurrentTabId();
  if (tab_id < 0)
    return;

  SkBitmap icon(browser_action()->GetIcon(tab_id));
  if (icon.isNull())
    icon = default_icon_;
  if (!icon.isNull()) {
    SkPaint paint;
    paint.setXfermode(SkXfermode::Create(SkXfermode::kSrcOver_Mode));
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    SkBitmap bg;
    rb.GetBitmapNamed(IDR_BROWSER_ACTION)->copyTo(&bg,
        SkBitmap::kARGB_8888_Config);
    SkCanvas bg_canvas(bg);
    bg_canvas.drawBitmap(icon, SkIntToScalar((bg.width() - icon.width()) / 2),
        SkIntToScalar((bg.height() - icon.height()) / 2), &paint);
    SetIcon(bg);

    SkBitmap bg_h;
    rb.GetBitmapNamed(IDR_BROWSER_ACTION_H)->copyTo(&bg_h,
        SkBitmap::kARGB_8888_Config);
    SkCanvas bg_h_canvas(bg_h);
    bg_h_canvas.drawBitmap(icon,
        SkIntToScalar((bg_h.width() - icon.width()) / 2),
        SkIntToScalar((bg_h.height() - icon.height()) / 2), &paint);
    SetHoverIcon(bg_h);

    SkBitmap bg_p;
    rb.GetBitmapNamed(IDR_BROWSER_ACTION_P)->copyTo(&bg_p,
        SkBitmap::kARGB_8888_Config);
    SkCanvas bg_p_canvas(bg_p);
    bg_p_canvas.drawBitmap(icon,
        SkIntToScalar((bg_p.width() - icon.width()) / 2),
        SkIntToScalar((bg_p.height() - icon.height()) / 2), &paint);
    SetPushedIcon(bg_p);
  }

  // If the browser action name is empty, show the extension name instead.
  string16 name = UTF8ToUTF16(browser_action()->GetTitle(tab_id));
  if (name.empty())
    name = UTF8ToUTF16(extension()->name());
  SetTooltipText(name);
  SetAccessibleName(name);
  parent()->SchedulePaint();
}

bool BrowserActionButton::IsPopup() {
  int tab_id = panel_->GetCurrentTabId();
  return (tab_id < 0) ? false : browser_action_->HasPopup(tab_id);
}

GURL BrowserActionButton::GetPopupUrl() {
  int tab_id = panel_->GetCurrentTabId();
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
      panel_->OnBrowserActionVisibilityChanged();
      break;
    case chrome::NOTIFICATION_EXTENSION_COMMAND_ADDED:
    case chrome::NOTIFICATION_EXTENSION_COMMAND_REMOVED: {
      std::pair<const std::string, const std::string>* payload =
          content::Details<std::pair<const std::string, const std::string> >(
              details).ptr();
      if (extension_->id() == payload->first &&
          payload->second ==
              extension_manifest_values::kBrowserActionKeybindingEvent) {
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

bool BrowserActionButton::Activate() {
  if (!IsPopup())
    return true;

  panel_->OnBrowserActionExecuted(this);

  // TODO(erikkay): Run a nested modal loop while the mouse is down to
  // enable menu-like drag-select behavior.

  // The return value of this method is returned via OnMousePressed.
  // We need to return false here since we're handing off focus to another
  // widget/view, and true will grab it right back and try to send events
  // to us.
  return false;
}

bool BrowserActionButton::OnMousePressed(const views::MouseEvent& event) {
  if (!event.IsRightMouseButton()) {
    return IsPopup() ? MenuButton::OnMousePressed(event)
                     : TextButton::OnMousePressed(event);
  }

  // See comments in MenuButton::Activate() as to why this is needed.
  SetMouseHandler(NULL);

  ShowContextMenu(gfx::Point(), true);
  return false;
}

void BrowserActionButton::OnMouseReleased(const views::MouseEvent& event) {
  if (IsPopup() || context_menu_) {
    // TODO(erikkay) this never actually gets called (probably because of the
    // loss of focus).
    MenuButton::OnMouseReleased(event);
  } else {
    TextButton::OnMouseReleased(event);
  }
}

void BrowserActionButton::OnMouseExited(const views::MouseEvent& event) {
  if (IsPopup() || context_menu_)
    MenuButton::OnMouseExited(event);
  else
    TextButton::OnMouseExited(event);
}

bool BrowserActionButton::OnKeyReleased(const views::KeyEvent& event) {
  return IsPopup() ? MenuButton::OnKeyReleased(event)
                   : TextButton::OnKeyReleased(event);
}

void BrowserActionButton::ShowContextMenu(const gfx::Point& p,
                                          bool is_mouse_gesture) {
  if (!extension()->ShowConfigureContextMenus())
    return;

  SetButtonPushed();

  // Reconstructs the menu every time because the menu's contents are dynamic.
  scoped_refptr<ExtensionContextMenuModel> context_menu_contents_(
      new ExtensionContextMenuModel(extension(), panel_->browser()));
  views::MenuModelAdapter menu_model_adapter(context_menu_contents_.get());
  views::MenuRunner menu_runner(menu_model_adapter.CreateMenu());

  context_menu_ = menu_runner.GetMenu();
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(this, &screen_loc);
  if (menu_runner.RunMenuAt(GetWidget(), NULL, gfx::Rect(screen_loc, size()),
          views::MenuItemView::TOPLEFT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;

  SetButtonNotPushed();
  context_menu_ = NULL;
}

bool BrowserActionButton::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  panel_->OnBrowserActionExecuted(this);
  return true;
}

void BrowserActionButton::SetButtonPushed() {
  SetState(views::CustomButton::BS_PUSHED);
  menu_visible_ = true;
}

void BrowserActionButton::SetButtonNotPushed() {
  SetState(views::CustomButton::BS_NORMAL);
  menu_visible_ = false;
}

BrowserActionButton::~BrowserActionButton() {
}

void BrowserActionButton::MaybeRegisterExtensionCommand() {
  extensions::CommandService* command_service =
      extensions::CommandServiceFactory::GetForProfile(
          panel_->browser()->profile());
  extensions::Command browser_action_command;
  if (command_service->GetBrowserActionCommand(
          extension_->id(),
          extensions::CommandService::ACTIVE_ONLY,
          &browser_action_command,
          NULL)) {
    keybinding_.reset(new ui::Accelerator(
        browser_action_command.accelerator()));
    panel_->GetFocusManager()->RegisterAccelerator(
        *keybinding_.get(), ui::AcceleratorManager::kHighPriority, this);
  }
}

void BrowserActionButton::MaybeUnregisterExtensionCommand(bool only_if_active) {
  if (!keybinding_.get() || !panel_->GetFocusManager())
    return;

  extensions::CommandService* command_service =
      extensions::CommandServiceFactory::GetForProfile(
          panel_->browser()->profile());

  extensions::Command browser_action_command;
  if (!only_if_active || !command_service->GetBrowserActionCommand(
          extension_->id(),
          extensions::CommandService::ACTIVE_ONLY,
          &browser_action_command,
          NULL)) {
    panel_->GetFocusManager()->UnregisterAccelerator(*keybinding_.get(), this);
  }
}


////////////////////////////////////////////////////////////////////////////////
// BrowserActionView

BrowserActionView::BrowserActionView(const Extension* extension,
                                     BrowserActionsContainer* panel)
    : panel_(panel) {
  button_ = new BrowserActionButton(extension, panel);
  button_->set_drag_controller(panel_);
  AddChildView(button_);
  button_->UpdateState();
}

BrowserActionView::~BrowserActionView() {
  RemoveChildView(button_);
  button_->Destroy();
}

gfx::Canvas* BrowserActionView::GetIconWithBadge() {
  int tab_id = panel_->GetCurrentTabId();

  SkBitmap icon = button_->extension()->browser_action()->GetIcon(tab_id);
  if (icon.isNull())
    icon = button_->default_icon();

  gfx::Canvas* canvas = new gfx::Canvas(icon, false);

  if (tab_id >= 0) {
    gfx::Rect bounds(icon.width(), icon.height() + ToolbarView::kVertSpacing);
    button_->extension()->browser_action()->PaintBadge(canvas, bounds, tab_id);
  }

  return canvas;
}

void BrowserActionView::Layout() {
  // We can't rely on button_->GetPreferredSize() here because that's not set
  // correctly until the first call to
  // BrowserActionsContainer::RefreshBrowserActionViews(), whereas this can be
  // called before that when the initial bounds are set (and then not after,
  // since the bounds don't change).  So instead of setting the height from the
  // button's preferred size, we use IconHeight(), since that's how big the
  // button should be regardless of what it's displaying.
  button_->SetBounds(0, ToolbarView::kVertSpacing, width(),
                     BrowserActionsContainer::IconHeight());
}

void BrowserActionView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(
      IDS_ACCNAME_EXTENSIONS_BROWSER_ACTION);
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
}

void BrowserActionView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);
  ExtensionAction* action = button()->browser_action();
  int tab_id = panel_->GetCurrentTabId();
  if (tab_id >= 0)
    action->PaintBadge(canvas, gfx::Rect(width(), height()), tab_id);
}
