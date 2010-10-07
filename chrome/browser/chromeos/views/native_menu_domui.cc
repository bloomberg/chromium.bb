// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/views/native_menu_domui.h"

#include <string>

#include "app/menus/menu_model.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chromeos/views/domui_menu_widget.h"
#include "chrome/browser/chromeos/views/menu_locator.h"
#include "chrome/browser/dom_ui/dom_ui_util.h"
#include "chrome/browser/profile_manager.h"
#include "gfx/favicon_size.h"
#include "gfx/font.h"
#include "gfx/rect.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/menu_config.h"

namespace {

// Returns true if the menu item type specified can be executed as a command.
bool MenuTypeCanExecute(menus::MenuModel::ItemType type) {
  return type == menus::MenuModel::TYPE_COMMAND ||
      type == menus::MenuModel::TYPE_CHECK ||
      type == menus::MenuModel::TYPE_RADIO;
}

// A utility function that generates css font property from gfx::Font.
std::wstring GetFontShorthand(const gfx::Font* font) {
  std::wstring out;
  if (font == NULL) {
    font = &(views::MenuConfig::instance().font);
  }
  if (font->GetStyle() & gfx::Font::BOLD) {
    out.append(L"bold ");
  }
  if (font->GetStyle() & gfx::Font::ITALIC) {
    out.append(L"italic ");
  }
  if (font->GetStyle() & gfx::Font::UNDERLINED) {
    out.append(L"underline ");
  }

  // TODO(oshima): The font size from gfx::Font is too small when
  // used in webkit. Figure out the reason.
  out.append(ASCIIToWide(base::IntToString(font->GetFontSize() + 4)));
  out.append(L"px/");
  out.append(ASCIIToWide(base::IntToString(
      std::max(kFavIconSize, font->GetHeight()))));
  out.append(L"px \"");
  out.append(font->GetFontName());
  out.append(L"\",sans-serif");
  return out;
}

// Currently opened menu. See RunMenuAt for reason why we need this.
chromeos::NativeMenuDOMUI* current_ = NULL;

}  // namespace

namespace chromeos {

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
      menu_action_(MENU_ACTION_NONE)  {
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
  FOR_EACH_OBSERVER(views::MenuListener, listeners_, OnMenuOpened());
  DCHECK(!menu_shown_);
  menu_shown_ = true;

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
  DictionaryValue model;
  ListValue* items = new ListValue();
  model.Set("items", items);
  bool has_icon = false;
  for (int index = 0; index < model_->GetItemCount(); ++index) {
    menus::MenuModel::ItemType type = model_->GetTypeAt(index);
    DictionaryValue* item;
    switch (type) {
      case menus::MenuModel::TYPE_SEPARATOR:
        item = CreateMenuItem(index, "separator", &has_icon);
        break;
      case menus::MenuModel::TYPE_RADIO:
        has_icon = true;  // all radio buttons has indicator icon.
        item = CreateMenuItem(index, "radio", &has_icon);
        break;
      case menus::MenuModel::TYPE_SUBMENU:
        item = CreateMenuItem(index, "submenu", &has_icon);
        break;
      case menus::MenuModel::TYPE_COMMAND:
        item = CreateMenuItem(index, "command", &has_icon);
        break;
      case menus::MenuModel::TYPE_CHECK:
        item = CreateMenuItem(index, "check", &has_icon);
        break;
      default:
        // TODO(oshima): We don't support BUTTOM_ITEM for now.
        // I haven't decided how to implement zoom/cut&paste
        // stuff, but may do somethign similar to what linux_views
        // does.
        NOTREACHED();
        continue;
    }
    items->Set(index, item);
  }
  model.SetBoolean("hasIcon", has_icon);
  model.SetBoolean("isRoot", menu_widget_->is_root());

  std::string json_model;
  base::JSONWriter::Write(&model, false, &json_model);
  std::wstring script = UTF8ToWide("updateModel(" + json_model + ")");
  menu_widget_->ExecuteJavascript(script);
}

void NativeMenuDOMUI::UpdateStates() {
  // Update menu contnets and submenus.
  Rebuild();
}

gfx::NativeMenu NativeMenuDOMUI::GetNativeMenu() const {
  NOTREACHED();
  return NULL;
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

void NativeMenuDOMUI::Activate(menus::MenuModel* model, int index) {
  NativeMenuDOMUI* root = GetRoot();
  if (root) {
    root->activated_menu_ = model;
    root->activated_index_ = index;
    root->menu_action_ = MENU_ACTION_SELECTED;
    root->Hide();
  }
}

void NativeMenuDOMUI::OpenSubmenu(int index, int y) {
  submenu_.reset();
  // Returns the model for the submenu at the specified index.
  menus::MenuModel* submenu = model_->GetSubmenuModelAt(index);
  submenu_.reset(new chromeos::NativeMenuDOMUI(submenu, false));
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
  Rebuild();
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

////////////////////////////////////////////////////////////////////////////////
// NativeMenuDOMUI, private:

void NativeMenuDOMUI::ProcessActivate() {
  if (activated_menu_ &&
      activated_menu_->IsEnabledAt(activated_index_) &&
      MenuTypeCanExecute(activated_menu_->GetTypeAt(activated_index_))) {
    activated_menu_->ActivatedAt(activated_index_);
  }
}

DictionaryValue* NativeMenuDOMUI::CreateMenuItem(
    int index,  const char* type, bool* has_icon_out) {
  // Note: DOM UI uses '&' as mnemonic.
  string16 label16 = model_->GetLabelAt(index);
  DictionaryValue* item = new DictionaryValue();

  item->SetString("type", type);
  item->SetString("label", label16);
  item->SetBoolean("enabled", model_->IsEnabledAt(index));
  item->SetBoolean("visible", model_->IsVisibleAt(index));
  item->SetBoolean("checked", model_->IsItemCheckedAt(index));
  item->SetInteger("command_id", model_->GetCommandIdAt(index));
  item->SetString(
      "font", WideToUTF16(GetFontShorthand(model_->GetLabelFontAt(index))));
  SkBitmap icon;
  if (model_->GetIconAt(index, &icon) && !icon.isNull() && !icon.empty()) {
    item->SetString("icon", dom_ui_util::GetImageDataUrl(icon));
    *has_icon_out = true;
  }
  return item;
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
  return new chromeos::NativeMenuDOMUI(model, true);
}

}  // namespace views
