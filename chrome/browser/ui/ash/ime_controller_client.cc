// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ime_controller_client.h"

#include <memory>
#include <vector>

#include "ash/ime/ime_controller.h"
#include "ash/public/interfaces/ime_info.mojom.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_util.h"

using chromeos::input_method::InputMethodDescriptor;
using chromeos::input_method::InputMethodManager;
using chromeos::input_method::InputMethodUtil;
using ui::ime::InputMethodMenuManager;

namespace {

ImeControllerClient* g_instance = nullptr;

}  // namespace

ImeControllerClient::ImeControllerClient(InputMethodManager* manager)
    : input_method_manager_(manager) {
  DCHECK(input_method_manager_);
  input_method_manager_->AddObserver(this);
  input_method_manager_->AddImeMenuObserver(this);
  InputMethodMenuManager::GetInstance()->AddObserver(this);

  // This does not need send the initial state to ash because that happens
  // via observers when the InputMethodManager initializes its list of IMEs.

  DCHECK(!g_instance);
  g_instance = this;
}

ImeControllerClient::~ImeControllerClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;

  InputMethodMenuManager::GetInstance()->RemoveObserver(this);
  input_method_manager_->RemoveImeMenuObserver(this);
  input_method_manager_->RemoveObserver(this);
}

// static
ImeControllerClient* ImeControllerClient::Get() {
  return g_instance;
}

void ImeControllerClient::SetImesManagedByPolicy(bool managed) {
  ash::Shell::Get()->ime_controller()->SetImesManagedByPolicy(managed);
}

// chromeos::input_method::InputMethodManager::Observer:
void ImeControllerClient::InputMethodChanged(InputMethodManager* manager,
                                             Profile* profile,
                                             bool show_message) {
  RefreshIme();
}

// chromeos::input_method::InputMethodManager::ImeMenuObserver:
void ImeControllerClient::ImeMenuActivationChanged(bool is_active) {
  ash::Shell::Get()->ime_controller()->ShowImeMenuOnShelf(is_active);
}

void ImeControllerClient::ImeMenuListChanged() {
  RefreshIme();
}

void ImeControllerClient::ImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<InputMethodManager::MenuItem>& items) {}

// ui::ime::InputMethodMenuManager::Observer:
void ImeControllerClient::InputMethodMenuItemChanged(
    InputMethodMenuManager* manager) {
  RefreshIme();
}

ash::mojom::ImeInfo ImeControllerClient::GetAshImeInfo(
    const InputMethodDescriptor& ime) const {
  InputMethodUtil* util = input_method_manager_->GetInputMethodUtil();
  ash::mojom::ImeInfo info;
  info.id = ime.id();
  info.name = util->GetInputMethodLongName(ime);
  info.medium_name = util->GetInputMethodMediumName(ime);
  info.short_name = util->GetInputMethodShortName(ime);
  info.third_party = chromeos::extension_ime_util::IsExtensionIME(ime.id());
  return info;
}

void ImeControllerClient::RefreshIme() {
  ash::ImeController* ime_controller = ash::Shell::Get()->ime_controller();
  InputMethodManager::State* state =
      input_method_manager_->GetActiveIMEState().get();
  if (!state) {
    ime_controller->RefreshIme(ash::mojom::ImeInfo(),
                               std::vector<ash::mojom::ImeInfo>(),
                               std::vector<ash::mojom::ImeMenuItem>());
    return;
  }

  InputMethodDescriptor current_descriptor = state->GetCurrentInputMethod();
  ash::mojom::ImeInfo current_ime = GetAshImeInfo(current_descriptor);
  current_ime.selected = true;

  std::vector<ash::mojom::ImeInfo> available_imes;
  std::unique_ptr<std::vector<InputMethodDescriptor>>
      available_ime_descriptors = state->GetActiveInputMethods();
  for (const InputMethodDescriptor& descriptor : *available_ime_descriptors) {
    ash::mojom::ImeInfo info = GetAshImeInfo(descriptor);
    info.selected = descriptor.id() == current_ime.id;
    available_imes.push_back(info);
  }

  std::vector<ash::mojom::ImeMenuItem> ash_menu_items;
  ui::ime::InputMethodMenuItemList menu_list =
      ui::ime::InputMethodMenuManager::GetInstance()
          ->GetCurrentInputMethodMenuItemList();
  for (const ui::ime::InputMethodMenuItem& menu_item : menu_list) {
    ash::mojom::ImeMenuItem ash_item;
    ash_item.key = menu_item.key;
    ash_item.label = base::UTF8ToUTF16(menu_item.label);
    ash_item.checked = menu_item.is_selection_item_checked;
    ash_menu_items.push_back(ash_item);
  }
  ash::Shell::Get()->ime_controller()->RefreshIme(current_ime, available_imes,
                                                  ash_menu_items);
}
