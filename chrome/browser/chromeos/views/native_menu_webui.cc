// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/views/native_menu_webui.h"

#include <string>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/dom_ui/menu_ui.h"
#include "chrome/browser/chromeos/views/menu_locator.h"
#include "chrome/browser/chromeos/views/webui_menu_widget.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/rect.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/nested_dispatcher_gtk.h"

#if defined(TOUCH_UI)
#include "views/focus/accelerator_handler.h"
#include "views/controls/menu/native_menu_x.h"
#else
#include "views/controls/menu/native_menu_gtk.h"
#endif

namespace {

using chromeos::NativeMenuWebUI;
using chromeos::WebUIMenuWidget;

// Returns true if the menu item type specified can be executed as a command.
bool MenuTypeCanExecute(ui::MenuModel::ItemType type) {
  return type == ui::MenuModel::TYPE_COMMAND ||
      type == ui::MenuModel::TYPE_CHECK ||
      type == ui::MenuModel::TYPE_RADIO;
}

gboolean Destroy(GtkWidget* widget, gpointer data) {
  WebUIMenuWidget* menu_widget = static_cast<WebUIMenuWidget*>(data);
  NativeMenuWebUI* webui_menu = menu_widget->webui_menu();
  // webui_menu can be NULL if widget is destroyed by signal.
  if (webui_menu)
    webui_menu->Hide();
  return true;
}

// Returns the active toplevel window.
gfx::NativeWindow FindActiveToplevelWindow() {
  GList* toplevels = gtk_window_list_toplevels();
  while (toplevels) {
    gfx::NativeWindow window = static_cast<gfx::NativeWindow>(toplevels->data);
    if (gtk_window_is_active(window)) {
      return window;
    }
    toplevels = g_list_next(toplevels);
  }
  return NULL;
}

// Currently opened menu. See RunMenuAt for reason why we need this.
NativeMenuWebUI* current_ = NULL;

}  // namespace

namespace chromeos {

// static
void NativeMenuWebUI::SetMenuURL(views::Menu2* menu2, const GURL& url) {
  // No-op if WebUI menu is disabled.
  if (!MenuUI::IsEnabled())
    return;

  gfx::NativeView native = menu2->GetNativeMenu();
  DCHECK(native);
  WebUIMenuWidget* widget = WebUIMenuWidget::FindWebUIMenuWidget(native);
  DCHECK(widget);
  widget->webui_menu()->set_menu_url(url);
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWebUI, public:

NativeMenuWebUI::NativeMenuWebUI(ui::MenuModel* menu_model, bool root)
    : parent_(NULL),
      submenu_(NULL),
      model_(menu_model),
      menu_widget_(NULL),
      menu_shown_(false),
      activated_menu_(NULL),
      activated_index_(-1),
      menu_action_(MENU_ACTION_NONE),
      menu_url_(StringPrintf("chrome://%s", chrome::kChromeUIMenu)),
      on_menu_opened_called_(false),
      nested_dispatcher_(NULL) {
  menu_widget_ = new WebUIMenuWidget(this, root);
  // Set the initial location off the screen not to show small
  // window with dropshadow.
  menu_widget_->Init(NULL, gfx::Rect(-10000, -10000, 1, 1));
}

NativeMenuWebUI::~NativeMenuWebUI() {
  if (nested_dispatcher_) {
    // Menu is destroyed while its in message loop.
    // Let nested dispatcher know the creator is deleted.
    nested_dispatcher_->CreatorDestroyed();
    Hide();
  }
  if (menu_widget_) {
    menu_widget_->Close();
    menu_widget_ = NULL;
  }
  parent_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWebUI, MenuWrapper implementation:

void NativeMenuWebUI::RunMenuAt(const gfx::Point& point, int alignment) {
  if (current_ != NULL) {
    // This happens when there is a nested task to show menu, which is
    // executed after menu is open. Since we need to enable nested task,
    // this condition has to be handled here.
    return;
  }
  current_ = this;
  bool context = false;

  // TODO(oshima): This is quick hack to check if it's context menu. (in rtl)
  // Fix this once we migrated.
  if (alignment == views::Menu2::ALIGN_TOPLEFT) {
    context = true;
  }

  activated_menu_ = NULL;
  activated_index_ = -1;
  menu_action_ = MENU_ACTION_NONE;

  MenuLocator* locator = context ?
      MenuLocator::CreateContextMenuLocator(point) :
      MenuLocator::CreateDropDownMenuLocator(point);
  ShowAt(locator);
  DCHECK(!menu_shown_);
  menu_shown_ = true;
  on_menu_opened_called_ = false;

  // TODO(oshima): A menu must be deleted when parent window is
  // closed. Menu2 doesn't know about the parent window, however, so
  // we're using toplevel gtkwindow. This is probably sufficient, but
  // I will update Menu2 to pass host view (which is necessary anyway
  // to get the right position) and get a parent widnow through
  // it. http://crosbug/7642
  gfx::NativeWindow parent = FindActiveToplevelWindow();
  gulong handle = 0;
  if (parent) {
    handle = g_signal_connect(G_OBJECT(parent), "destroy",
                              G_CALLBACK(&Destroy),
                              menu_widget_);
  }
  // We need to turn on nestable tasks as a renderer uses tasks internally.
  // Without this, renderer cannnot finish loading page.
  nested_dispatcher_ =
      new views::NestedDispatcherGtk(this, true /* allow nested */);
  bool deleted = nested_dispatcher_->RunAndSelfDestruct();
  current_ = NULL;  // this is static and safe to access.
  if (deleted) {
    // The menu was destryed while menu is shown, so return immediately.
    // Don't touch the instance which is already deleted.
    return;
  }
  nested_dispatcher_ = NULL;
  if (menu_shown_) {
    // If this happens it means we haven't yet gotten the hide signal and
    // someone else quit the message loop on us.
    NOTREACHED();
    menu_shown_ = false;
  }
  if (handle)
    g_signal_handler_disconnect(G_OBJECT(parent), handle);

  menu_widget_->Hide();
  // Close All submenus.
  submenu_.reset();
  ProcessActivate();
}

void NativeMenuWebUI::CancelMenu() {
  Hide();
}

void NativeMenuWebUI::Rebuild() {
  activated_menu_ = NULL;
  menu_widget_->ExecuteJavascript(L"modelUpdated()");
}

void NativeMenuWebUI::UpdateStates() {
  // Update menu contnets and submenus.
  Rebuild();
}

gfx::NativeMenu NativeMenuWebUI::GetNativeMenu() const {
  return menu_widget_->GetNativeView();
}

NativeMenuWebUI::MenuAction NativeMenuWebUI::GetMenuAction() const {
  return menu_action_;
}

void NativeMenuWebUI::AddMenuListener(views::MenuListener* listener) {
  listeners_.AddObserver(listener);
}

void NativeMenuWebUI::RemoveMenuListener(views::MenuListener* listener) {
  listeners_.RemoveObserver(listener);
}

void NativeMenuWebUI::SetMinimumWidth(int width) {
  gtk_widget_set_size_request(menu_widget_->GetNativeView(), width, 1);
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWebUI, MessageLoopForUI::Dispatcher implementation:

bool NativeMenuWebUI::Dispatch(GdkEvent* event) {
  switch (event->type) {
    case GDK_MOTION_NOTIFY: {
      NativeMenuWebUI* target = FindMenuAt(
          gfx::Point(event->motion.x_root, event->motion.y_root));
      if (target)
        target->menu_widget_->EnableInput(false);
      break;
    }
    case GDK_BUTTON_PRESS: {
      NativeMenuWebUI* target = FindMenuAt(
          gfx::Point(event->motion.x_root, event->motion.y_root));
      if (!target) {
        Hide();
        return true;
      }
      break;
    }
    default:
      break;
  }
  gtk_main_do_event(event);
  return true;
}

#if defined(TOUCH_UI)
base::MessagePumpGlibXDispatcher::DispatchStatus
    NativeMenuWebUI::DispatchX(XEvent* xevent) {
  return views::DispatchXEvent(xevent) ?
      base::MessagePumpGlibXDispatcher::EVENT_PROCESSED :
      base::MessagePumpGlibXDispatcher::EVENT_IGNORED;

}
#endif

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWebUI, MenuControl implementation:

void NativeMenuWebUI::Activate(ui::MenuModel* model,
                               int index,
                               ActivationMode activation) {
  NativeMenuWebUI* root = GetRoot();
  if (root) {
    if (activation == CLOSE_AND_ACTIVATE) {
      root->activated_menu_ = model;
      root->activated_index_ = index;
      root->menu_action_ = MENU_ACTION_SELECTED;
      root->Hide();
    } else {
      if (model->IsEnabledAt(index) &&
          MenuTypeCanExecute(model->GetTypeAt(index))) {
        model->ActivatedAt(index);
      }
    }
  }
}

void NativeMenuWebUI::OpenSubmenu(int index, int y) {
  submenu_.reset();
  // Returns the model for the submenu at the specified index.
  ui::MenuModel* submenu = model_->GetSubmenuModelAt(index);
  submenu_.reset(new chromeos::NativeMenuWebUI(submenu, false));
  submenu_->set_menu_url(menu_url_);
  // y in menu_widget_ coordinate.
  submenu_->set_parent(this);
  submenu_->ShowAt(
      MenuLocator::CreateSubMenuLocator(
          menu_widget_,
          menu_widget_->menu_locator()->GetSubmenuDirection(),
          y));
}

void NativeMenuWebUI::CloseAll() {
  NativeMenuWebUI* root = GetRoot();
  // root can be null if the submenu is detached from parent.
  if (root)
    root->Hide();
}

void NativeMenuWebUI::CloseSubmenu() {
  submenu_.reset();  // This closes subsequent children.
}

void NativeMenuWebUI::MoveInputToSubmenu() {
  if (submenu_.get()) {
    submenu_->menu_widget_->EnableInput(true);
  }
}

void NativeMenuWebUI::MoveInputToParent() {
  if (parent_) {
    parent_->menu_widget_->EnableInput(true);
  }
}

void NativeMenuWebUI::OnLoad() {
  // TODO(oshima): OnLoad is no longer used, but kept in case
  // we may need it. Delete this if this is not necessary to
  // implement wrench/network/bookmark menus.
}

void NativeMenuWebUI::SetSize(const gfx::Size& size) {
  menu_widget_->SetSize(size);
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWebUI, public:

void NativeMenuWebUI::Hide() {
  // Only root can hide and exit the message loop.
  DCHECK(menu_widget_->is_root());
  DCHECK(!parent_);
  if (!menu_shown_) {
    // The menu has been already hidden by us and we're in the process of
    // quiting the message loop..
    return;
  }
  CloseSubmenu();
  menu_shown_ = false;
  MessageLoop::current()->Quit();
}

NativeMenuWebUI* NativeMenuWebUI::GetRoot() {
  NativeMenuWebUI* ancestor = this;
  while (ancestor->parent_)
    ancestor = ancestor->parent_;
  if (ancestor->menu_widget_->is_root())
    return ancestor;
  else
    return NULL;
}

Profile* NativeMenuWebUI::GetProfile() {
  Browser* browser = BrowserList::GetLastActive();
    // browser can be null in login screen.
  if (!browser)
    return ProfileManager::GetDefaultProfile();
  return browser->GetProfile();
}

void NativeMenuWebUI::InputIsReady() {
  if (!on_menu_opened_called_) {
    on_menu_opened_called_ = true;
    FOR_EACH_OBSERVER(views::MenuListener, listeners_, OnMenuOpened());
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWebUI, private:

void NativeMenuWebUI::ProcessActivate() {
  if (activated_menu_ &&
      activated_menu_->IsEnabledAt(activated_index_) &&
      MenuTypeCanExecute(activated_menu_->GetTypeAt(activated_index_))) {
    activated_menu_->ActivatedAt(activated_index_);
  }
}

void NativeMenuWebUI::ShowAt(MenuLocator* locator) {
  model_->MenuWillShow();
  menu_widget_->ShowAt(locator);
}

NativeMenuWebUI* NativeMenuWebUI::FindMenuAt(const gfx::Point& point) {
  if (submenu_.get()) {
    NativeMenuWebUI* found = submenu_->FindMenuAt(point);
    if (found)
      return found;
  }
  gfx::Rect bounds;
  menu_widget_->GetBounds(&bounds, false);
  return bounds.Contains(point) ? this : NULL;
}

}  // namespace chromeos
