// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_export.h"

#include "ash/ime/input_method_menu_item.h"
#include "base/observer_list.h"

#ifndef ASH_IME_INPUT_METHOD_MENU_MANAGER_H_
#define ASH_IME_INPUT_METHOD_MENU_MANAGER_H_

template<typename Type> struct DefaultSingletonTraits;

namespace ash {
namespace ime {

class ASH_EXPORT InputMethodMenuManager {
public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the list of menu items is changed.
    virtual void InputMethodMenuItemChanged(
        InputMethodMenuManager* manager) = 0;
  };

  ~InputMethodMenuManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Obtains the singleton instance.
  static InputMethodMenuManager* GetInstance();

  // Sets the list of input method menu items. The list could be empty().
  void SetCurrentInputMethodMenuItemList(
      const InputMethodMenuItemList& menu_list);

  // Gets the list of input method menu items. The list could be empty().
  InputMethodMenuItemList GetCurrentInputMethodMenuItemList() const;

  // True if the key exists in the menu_list_.
  bool HasInputMethodMenuItemForKey(const std::string& key) const;

 private:
  InputMethodMenuManager();

  // For Singleton to be able to construct an instance.
  friend struct DefaultSingletonTraits<InputMethodMenuManager>;

  // Menu item list of the input method.  This is set by extension IMEs.
  InputMethodMenuItemList menu_list_;

  // Observers who will be notified when menu changes.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMenuManager);
};

}  // namespace ime
}  // namespace ash

#endif // ASH_IME_INPUT_METHOD_MENU_MANAGER_H_
