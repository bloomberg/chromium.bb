// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/input_method_menu_manager.h"

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace ash {
namespace ime {

InputMethodMenuManager::InputMethodMenuManager()
    : menu_list_(), observers_() {}

InputMethodMenuManager::~InputMethodMenuManager() {}

void InputMethodMenuManager::AddObserver(
    InputMethodMenuManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void InputMethodMenuManager::RemoveObserver(
    InputMethodMenuManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

InputMethodMenuItemList
InputMethodMenuManager::GetCurrentInputMethodMenuItemList() const {
  return menu_list_;
}

void InputMethodMenuManager::SetCurrentInputMethodMenuItemList(
    const InputMethodMenuItemList& menu_list) {
  menu_list_ = menu_list;
  FOR_EACH_OBSERVER(InputMethodMenuManager::Observer,
                    observers_,
                    InputMethodMenuItemChanged(this));
}

bool InputMethodMenuManager::HasInputMethodMenuItemForKey(
    const std::string& key) const {
  for (size_t i = 0; i < menu_list_.size(); ++i) {
    if (menu_list_[i].key == key) {
      return true;
    }
  }
  return false;
}

// static
InputMethodMenuManager* InputMethodMenuManager::GetInstance() {
  return Singleton<InputMethodMenuManager>::get();
}

}  // namespace ime
}  // namespace ash
