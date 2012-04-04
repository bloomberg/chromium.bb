// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_INPUT_METHOD_MENU_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_INPUT_METHOD_MENU_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/menu/menu_item_view.h"

class PrefService;
class SkBitmap;

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class MenuItemView;
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

namespace chromeos {

// A class for the dropdown menu for switching input method and keyboard layout.
// Since the class provides the views::MenuButtonListener interface, it's easy
// to create a button widget (e.g. views::MenuButton, StatusAreaButton) which
// shows the dropdown menu on click.
class InputMethodMenu
    : public views::MenuButtonListener,
      public ui::MenuModel,
      public input_method::InputMethodManager::Observer {
 public:
  InputMethodMenu();
  virtual ~InputMethodMenu();

  // ui::MenuModel implementation.
  virtual bool HasIcons() const OVERRIDE;
  virtual int GetItemCount() const OVERRIDE;
  virtual ui::MenuModel::ItemType GetTypeAt(int index) const OVERRIDE;
  virtual int GetCommandIdAt(int index) const OVERRIDE;
  virtual string16 GetLabelAt(int index) const OVERRIDE;
  virtual bool IsItemDynamicAt(int index) const OVERRIDE;
  virtual bool GetAcceleratorAt(int index,
                                ui::Accelerator* accelerator) const OVERRIDE;
  virtual bool IsItemCheckedAt(int index) const OVERRIDE;
  virtual int GetGroupIdAt(int index) const OVERRIDE;
  virtual bool GetIconAt(int index, SkBitmap* icon) OVERRIDE;
  virtual ui::ButtonMenuItemModel* GetButtonMenuItemAt(
      int index) const OVERRIDE;
  virtual bool IsEnabledAt(int index) const OVERRIDE;
  virtual ui::MenuModel* GetSubmenuModelAt(int index) const OVERRIDE;
  virtual void HighlightChangedTo(int index) OVERRIDE;
  virtual void ActivatedAt(int index) OVERRIDE;
  virtual void MenuWillShow() OVERRIDE;
  virtual void SetMenuModelDelegate(ui::MenuModelDelegate* delegate) OVERRIDE;

  // views::MenuButtonListener implementation.
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  // InputMethodManager::Observer implementation.
  virtual void InputMethodChanged(
      input_method::InputMethodManager* manager,
      const input_method::InputMethodDescriptor& current_input_method,
      size_t num_active_input_methods) OVERRIDE;
  virtual void ActiveInputMethodsChanged(
      input_method::InputMethodManager* manager,
      const input_method::InputMethodDescriptor& current_input_method,
      size_t num_active_input_methods) OVERRIDE;
  virtual void PropertyListChanged(
      input_method::InputMethodManager* manager,
      const input_method::InputMethodPropertyList& properties) OVERRIDE;

  // Specify menu alignment (default TOPRIGHT).
  void set_menu_alignment(views::MenuItemView::AnchorPosition menu_alignment) {
    menu_alignment_ = menu_alignment;
  }

  // Sets the minimum width of the dropdown menu.
  void SetMinimumWidth(int width);

  // Rebuilds menu model.
  void PrepareMenuModel();

  // Returns a string for the drop-down menu and the tooltip for the indicator.
  // The method is public for unit tests.
  static string16 GetTextForMenu(
      const input_method::InputMethodDescriptor& input_method);

 protected:
  // Prepares menu: saves user metrics and rebuilds.
  void PrepareForMenuOpen();

 private:
  // Updates UI of a container of the menu (e.g. the "US" menu button in the
  // status area). Sub classes have to implement the interface for their own UI.
  virtual void UpdateUI(const std::string& input_method_id,  // e.g. "mozc"
                        const string16& name,  // e.g. "US", "INTL"
                        const string16& tooltip,
                        size_t num_active_input_methods) = 0;

  // Sub classes have to implement the interface. This interface should return
  // true if the dropdown menu should show an item like "Customize languages
  // and input..." WebUI.
  virtual bool ShouldSupportConfigUI() = 0;

  // Sub classes have to implement the interface which opens an UI for
  // customizing languages and input.
  virtual void OpenConfigUI() = 0;

  // Parses |input_method| and then calls UpdateUI().
  void UpdateUIFromInputMethod(
      const input_method::InputMethodDescriptor& input_method,
      size_t num_active_input_methods);

  // Rebuilds |model_|. This function should be called whenever
  // |input_method_descriptors_| is updated, or ImePropertiesChanged() is
  // called.
  void RebuildModel();

  // Returns true if the zero-origin |index| points to one of the input methods.
  bool IndexIsInInputMethodList(int index) const;

  // Returns true if the zero-origin |index| points to one of the IME
  // properties. When returning true, |property_index| is updated so that
  // property_list.at(property_index) points to the menu item.
  bool GetPropertyIndex(int index, int* property_index) const;

  // Returns true if the zero-origin |index| points to the "Configure IME" menu
  // item.
  bool IndexPointsToConfigureImeMenuItem(int index) const;

  // Add / Remove InputMethodManager observer.
  void AddObserver();
  void RemoveObserver();

  // The current input method list.
  scoped_ptr<input_method::InputMethodDescriptors> input_method_descriptors_;

  // We borrow ui::SimpleMenuModel implementation to maintain the current
  // content of the pop-up menu. The ui::MenuModel is implemented using this
  // |model_|.  The MenuModelAdapter wraps the model with the
  // views::MenuDelegate interface required for MenuItemView.
  scoped_ptr<ui::SimpleMenuModel> model_;
  scoped_ptr<views::MenuModelAdapter> input_method_menu_delegate_;
  views::MenuItemView* input_method_menu_;
  scoped_ptr<views::MenuRunner> input_method_menu_runner_;

  int minimum_input_method_menu_width_;

  // Menu alignment (default TOPRIGHT).
  views::MenuItemView::AnchorPosition menu_alignment_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMenu);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_INPUT_METHOD_MENU_H_
