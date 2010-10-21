// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/views/native_menu_domui.h"

#include <string>

#include "app/menus/menu_model.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/dom_ui/menu_ui.h"
#include "chrome/browser/chromeos/views/domui_menu_widget.h"
#include "chrome/browser/chromeos/views/menu_locator.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/url_constants.h"
#include "gfx/rect.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/native_menu_gtk.h"

namespace {

// Returns true if the menu item type specified can be executed as a command.
bool MenuTypeCanExecute(menus::MenuModel::ItemType type) {
  return type == menus::MenuModel::TYPE_COMMAND ||
      type == menus::MenuModel::TYPE_CHECK ||
      type == menus::MenuModel::TYPE_RADIO;
}

gboolean Destroy(GtkWidget* widget, gpointer data) {
  chromeos::NativeMenuDOMUI* domui_menu =
      static_cast<chromeos::NativeMenuDOMUI*>(data);
  domui_menu->Hide();
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
chromeos::NativeMenuDOMUI* current_ = NULL;

}  // namespace

namespace chromeos {

// static
void NativeMenuDOMUI::SetMenuURL(views::Menu2* menu2, const GURL& url) {
  // No-op if DOMUI menu is disabled.
  if (!MenuUI::IsEnabled())
    return;

  gfx::NativeView native = menu2->GetNativeMenu();
  DCHECK(native);
  DOMUIMenuWidget* widget = DOMUIMenuWidget::FindDOMUIMenuWidget(native);
  DCHECK(widget);
  widget->domui_menu()->set_menu_url(url);
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuDOMUI, public:

NativeMenuDOMUI::NativeMenuDOMUI(menus::MenuModel* menu_model, bool root)
    : parent_(NULL),
      submenu_(NULL),
      model_(menu_model),
      menu_widget_(NULL),
      menu_shown_(false),
      activated_menu_(NULL),
      activated_index_(-1),
      menu_action_(MENU_ACTION_NONE),
      menu_url_(StringPrintf("chrome://%s", chrome::kChromeUIMenu)),
      on_menu_opened_called_(false) {
  menu_widget_ = new DOMUIMenuWidget(this, root);
  // Set the initial location off the screen not to show small
  // window with dropshadow.
  menu_widget_->Init(NULL, gfx::Rect(-10000, -10000, 1, 1));
}

NativeMenuDOMUI::~NativeMenuDOMUI() {
  DCHECK(!menu_shown_) << "Deleting while the menu is shown";
  if (menu_widget_) {
    menu_widget_->Close();
    menu_widget_ = NULL;
  }
  parent_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuDOMUI, MenuWrapper implementation:

void NativeMenuDOMUI::RunMenuAt(const gfx::Point& point, int alignment) {
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
                              this);
  }

  // We need to turn on nestable tasks as a renderer uses tasks internally.
  // Without this, renderer cannnot finish loading page.
  bool nestable = MessageLoopForUI::current()->NestableTasksAllowed();
  MessageLoopForUI::current()->SetNestableTasksAllowed(true);
  MessageLoopForUI::current()->Run(this);
  MessageLoopForUI::current()->SetNestableTasksAllowed(nestable);
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
  current_ = NULL;
  ProcessActivate();
}

void NativeMenuDOMUI::CancelMenu() {
  Hide();
}

void NativeMenuDOMUI::Rebuild() {
  activated_menu_ = NULL;
  menu_widget_->ExecuteJavascript(L"modelUpdated()");
}

void NativeMenuDOMUI::UpdateStates() {
  // Update menu contnets and submenus.
  Rebuild();
}

gfx::NativeMenu NativeMenuDOMUI::GetNativeMenu() const {
  return menu_widget_->GetNativeView();
}

NativeMenuDOMUI::MenuAction NativeMenuDOMUI::GetMenuAction() const {
  return menu_action_;
}

void NativeMenuDOMUI::AddMenuListener(views::MenuListener* listener) {
  listeners_.AddObserver(listener);
}

void NativeMenuDOMUI::RemoveMenuListener(views::MenuListener* listener) {
  listeners_.RemoveObserver(listener);
}

void NativeMenuDOMUI::SetMinimumWidth(int width) {
  gtk_widget_set_size_request(menu_widget_->GetNativeView(), width, 1);
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuDOMUI, MessageLoopForUI::Dispatcher implementation:

bool NativeMenuDOMUI::Dispatch(GdkEvent* event) {
  switch (event->type) {
    case GDK_MOTION_NOTIFY: {
      NativeMenuDOMUI* target = FindMenuAt(
          gfx::Point(event->motion.x_root, event->motion.y_root));
      if (target)
        target->menu_widget_->EnableInput(false);
      break;
    }
    case GDK_BUTTON_PRESS: {
      NativeMenuDOMUI* target = FindMenuAt(
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

////////////////////////////////////////////////////////////////////////////////
// NativeMenuDOMUI, MenuControl implementation:

void NativeMenuDOMUI::Activate(menus::MenuModel* model,
                               int index,
                               ActivationMode activation) {
  NativeMenuDOMUI* root = GetRoot();
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

void NativeMenuDOMUI::OpenSubmenu(int index, int y) {
  submenu_.reset();
  // Returns the model for the submenu at the specified index.
  menus::MenuModel* submenu = model_->GetSubmenuModelAt(index);
  submenu_.reset(new chromeos::NativeMenuDOMUI(submenu, false));
  submenu_->set_menu_url(menu_url_);
  // y in menu_widget_ coordinate.
  submenu_->set_parent(this);
  submenu_->ShowAt(
      MenuLocator::CreateSubMenuLocator(
          menu_widget_,
          menu_widget_->menu_locator()->GetSubmenuDirection(),
          y));
}

void NativeMenuDOMUI::CloseAll() {
  NativeMenuDOMUI* root = GetRoot();
  // root can be null if the submenu is detached from parent.
  if (root)
    root->Hide();
}

void NativeMenuDOMUI::CloseSubmenu() {
  submenu_.reset();  // This closes subsequent children.
}

void NativeMenuDOMUI::MoveInputToSubmenu() {
  if (submenu_.get()) {
    submenu_->menu_widget_->EnableInput(true);
  }
}

void NativeMenuDOMUI::MoveInputToParent() {
  if (parent_) {
    parent_->menu_widget_->EnableInput(true);
  }
}

void NativeMenuDOMUI::OnLoad() {
  // TODO(oshima): OnLoad is no longer used, but kept in case
  // we may need it. Delete this if this is not necessary to
  // implement wrench/network/bookmark menus.
}

void NativeMenuDOMUI::SetSize(const gfx::Size& size) {
  menu_widget_->SetSize(size);
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuDOMUI, public:

void NativeMenuDOMUI::Hide() {
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

NativeMenuDOMUI* NativeMenuDOMUI::GetRoot() {
  NativeMenuDOMUI* ancestor = this;
  while (ancestor->parent_)
    ancestor = ancestor->parent_;
  if (ancestor->menu_widget_->is_root())
    return ancestor;
  else
    return NULL;
}

Profile* NativeMenuDOMUI::GetProfile() {
  Browser* browser = BrowserList::GetLastActive();
    // browser can be null in login screen.
  if (!browser)
    return ProfileManager::GetDefaultProfile();
  return browser->GetProfile();
}

void NativeMenuDOMUI::InputIsReady() {
  if (!on_menu_opened_called_) {
    on_menu_opened_called_ = true;
    FOR_EACH_OBSERVER(views::MenuListener, listeners_, OnMenuOpened());
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuDOMUI, private:

void NativeMenuDOMUI::ProcessActivate() {
  if (activated_menu_ &&
      activated_menu_->IsEnabledAt(activated_index_) &&
      MenuTypeCanExecute(activated_menu_->GetTypeAt(activated_index_))) {
    activated_menu_->ActivatedAt(activated_index_);
  }
}

void NativeMenuDOMUI::ShowAt(MenuLocator* locator) {
  model_->MenuWillShow();
  menu_widget_->ShowAt(locator);
}

NativeMenuDOMUI* NativeMenuDOMUI::FindMenuAt(const gfx::Point& point) {
  if (submenu_.get()) {
    NativeMenuDOMUI* found = submenu_->FindMenuAt(point);
    if (found) return found;
  }
  gfx::Rect bounds;
  menu_widget_->GetBounds(&bounds, false);
  return bounds.Contains(point) ? this : NULL;
}

}  // namespace chromeos

////////////////////////////////////////////////////////////////////////////////
// MenuWrapper, public:

namespace views {

// static
MenuWrapper* MenuWrapper::CreateWrapper(Menu2* menu) {
  menus::MenuModel* model = menu->model();
  if (chromeos::MenuUI::IsEnabled()) {
    return new chromeos::NativeMenuDOMUI(model, true);
  } else {
    return new NativeMenuGtk(menu);
  }
}

}  // namespace views
