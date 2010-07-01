// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_LANGUAGE_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_LANGUAGE_MENU_BUTTON_H_

#include "app/menus/simple_menu_model.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/pref_member.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

class SkBitmap;

namespace chromeos {

class StatusAreaHost;

// The language menu button in the status area.
// This class will handle getting the IME/XKB status and populating the menu.
class LanguageMenuButton : public views::MenuButton,
                           public views::ViewMenuDelegate,
                           public menus::MenuModel,
                           public InputMethodLibrary::Observer,
                           public NotificationObserver {
 public:
  explicit LanguageMenuButton(StatusAreaHost* host);
  virtual ~LanguageMenuButton();

  // menus::MenuModel implementation.
  virtual bool HasIcons() const;
  virtual int GetItemCount() const;
  virtual menus::MenuModel::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsLabelDynamicAt(int index) const;
  virtual bool GetAcceleratorAt(int index,
                                menus::Accelerator* accelerator) const;
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const;
  virtual bool GetIconAt(int index, SkBitmap* icon) const;
  virtual menus::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const;
  virtual bool IsEnabledAt(int index) const;
  virtual menus::MenuModel* GetSubmenuModelAt(int index) const;
  virtual void HighlightChangedTo(int index);
  virtual void ActivatedAt(int index);
  virtual void MenuWillShow();

  // InputMethodLibrary::Observer implementation.
  virtual void InputMethodChanged(InputMethodLibrary* obj);
  virtual void ImePropertiesChanged(InputMethodLibrary* obj);
  virtual void ActiveInputMethodsChanged(InputMethodLibrary* obj);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Converts an InputMethodDescriptor object into human readable string.
  // Returns a text for the indicator on top right corner of the Chrome window.
  static std::wstring GetTextForIndicator(
      const InputMethodDescriptor& input_method);

  // Converts an InputMethodDescriptor object into human readable string.
  // Returns a string for the drop-down menu and the tooltip for the indicator.
  static std::wstring GetTextForMenu(
      const InputMethodDescriptor& input_method, bool add_method_name);

  // Registers input method preferences for the login screen.
  static void RegisterPrefs(PrefService* local_state);

 protected:
  // views::View implementation.
  virtual void LocaleChanged();

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Updates the status area with |name| and tooltip with |tooltip|.
  void UpdateIndicator(const std::wstring& name, const std::wstring& tooltip);

  // Updates the status area from the given input method.
  void UpdateIndicatorFromInputMethod(
      const InputMethodDescriptor& input_method);

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

  // The current input method list.
  scoped_ptr<InputMethodDescriptors> input_method_descriptors_;

  // Objects for reading/writing the Chrome prefs.
  StringPrefMember previous_input_method_pref_;
  StringPrefMember current_input_method_pref_;

  // Languages that need the input method name displayed.
  std::set<std::string> need_method_name_;

  // We borrow menus::SimpleMenuModel implementation to maintain the current
  // content of the pop-up menu. The menus::MenuModel is implemented using this
  // |model_|.
  scoped_ptr<menus::SimpleMenuModel> model_;

  // The language menu which pops up when the button in status area is clicked.
  views::Menu2 language_menu_;

  StatusAreaHost* host_;
  NotificationRegistrar registrar_;
  bool logged_in_;

  DISALLOW_COPY_AND_ASSIGN(LanguageMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_LANGUAGE_MENU_BUTTON_H_
