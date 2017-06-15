// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_IME_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_IME_CONTROLLER_CLIENT_H_

#include "base/macros.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"

namespace ash {
namespace mojom {
class ImeInfo;
}
}

// Connects the ImeController in ash to the InputMethodManagerImpl in chrome.
// TODO(jamescook): Convert to using mojo interfaces.
class ImeControllerClient
    : public chromeos::input_method::InputMethodManager::Observer,
      public chromeos::input_method::InputMethodManager::ImeMenuObserver,
      public ui::ime::InputMethodMenuManager::Observer {
 public:
  explicit ImeControllerClient(
      chromeos::input_method::InputMethodManager* manager);
  ~ImeControllerClient() override;

  static ImeControllerClient* Get();

  // Sets whether the list of IMEs is managed by device policy.
  void SetImesManagedByPolicy(bool managed);

  // chromeos::input_method::InputMethodManager::Observer:
  void InputMethodChanged(chromeos::input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // chromeos::input_method::InputMethodManager::ImeMenuObserver:
  void ImeMenuActivationChanged(bool is_active) override;
  void ImeMenuListChanged() override;
  void ImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<chromeos::input_method::InputMethodManager::MenuItem>&
          items) override;

  // ui::ime::InputMethodMenuManager::Observer:
  void InputMethodMenuItemChanged(
      ui::ime::InputMethodMenuManager* manager) override;

 private:
  // Converts IME information from |descriptor| into the ash mojo format.
  ash::mojom::ImeInfo GetAshImeInfo(
      const chromeos::input_method::InputMethodDescriptor& descriptor) const;

  // Sends information about current and available IMEs to ash.
  void RefreshIme();

  chromeos::input_method::InputMethodManager* const input_method_manager_;

  DISALLOW_COPY_AND_ASSIGN(ImeControllerClient);
};

#endif  // CHROME_BROWSER_UI_ASH_IME_CONTROLLER_CLIENT_H_
