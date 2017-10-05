// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_controller.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"

namespace ash {

ImeController::ImeController() = default;

ImeController::~ImeController() = default;

void ImeController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ImeController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ImeController::BindRequest(mojom::ImeControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ImeController::SetClient(mojom::ImeControllerClientPtr client) {
  client_ = std::move(client);
}

bool ImeController::CanSwitchIme() const {
  // Cannot switch unless there is an active IME.
  if (current_ime_.id.empty())
    return false;

  // Do not consume key event if there is only one input method is enabled.
  // Ctrl+Space or Alt+Shift may be used by other application.
  return available_imes_.size() > 1;
}

void ImeController::SwitchToNextIme() {
  if (client_)
    client_->SwitchToNextIme();
}

void ImeController::SwitchToPreviousIme() {
  if (client_)
    client_->SwitchToPreviousIme();
}

void ImeController::SwitchImeById(const std::string& ime_id,
                                  bool show_message) {
  if (client_)
    client_->SwitchImeById(ime_id, show_message);
}

void ImeController::ActivateImeMenuItem(const std::string& key) {
  if (client_)
    client_->ActivateImeMenuItem(key);
}

bool ImeController::CanSwitchImeWithAccelerator(
    const ui::Accelerator& accelerator) const {
  // If none of the input methods associated with |accelerator| are active, we
  // should ignore the accelerator.
  std::vector<std::string> candidate_ids =
      GetCandidateImesForAccelerator(accelerator);
  return !candidate_ids.empty();
}

void ImeController::SwitchImeWithAccelerator(
    const ui::Accelerator& accelerator) {
  if (!client_)
    return;

  std::vector<std::string> candidate_ids =
      GetCandidateImesForAccelerator(accelerator);
  if (candidate_ids.empty())
    return;
  auto it =
      std::find(candidate_ids.begin(), candidate_ids.end(), current_ime_.id);
  if (it != candidate_ids.end())
    ++it;
  if (it == candidate_ids.end())
    it = candidate_ids.begin();
  client_->SwitchImeById(*it, true /* show_message */);
}

// mojom::ImeController:
void ImeController::RefreshIme(const std::string& current_ime_id,
                               std::vector<mojom::ImeInfoPtr> available_imes,
                               std::vector<mojom::ImeMenuItemPtr> menu_items) {
  if (current_ime_id.empty())
    current_ime_ = mojom::ImeInfo();

  available_imes_.clear();
  available_imes_.reserve(available_imes.size());
  for (const auto& ime : available_imes) {
    if (ime->id.empty()) {
      DLOG(ERROR) << "Received IME with invalid ID.";
      continue;
    }
    available_imes_.push_back(*ime);
    if (ime->id == current_ime_id)
      current_ime_ = *ime;
  }

  // Either there is no current IME or we found a valid one in the list of
  // available IMEs.
  DCHECK(current_ime_id.empty() || !current_ime_.id.empty());

  current_ime_menu_items_.clear();
  current_ime_menu_items_.reserve(menu_items.size());
  for (const auto& item : menu_items)
    current_ime_menu_items_.push_back(*item);

  Shell::Get()->system_tray_notifier()->NotifyRefreshIME();
}

void ImeController::SetImesManagedByPolicy(bool managed) {
  managed_by_policy_ = managed;
  Shell::Get()->system_tray_notifier()->NotifyRefreshIME();
}

void ImeController::ShowImeMenuOnShelf(bool show) {
  Shell::Get()->system_tray_notifier()->NotifyRefreshIMEMenu(show);
}

void ImeController::SetCapsLockState(bool caps_enabled) {
  is_caps_lock_enabled_ = caps_enabled;

  for (ImeController::Observer& observer : observers_)
    observer.OnCapsLockChanged(caps_enabled);
}

void ImeController::SetExtraInputOptionsEnabledState(
    bool is_extra_input_options_enabled,
    bool is_emoji_enabled,
    bool is_handwriting_enabled,
    bool is_voice_enabled) {
  is_extra_input_options_enabled_ = is_extra_input_options_enabled;
  is_emoji_enabled_ = is_emoji_enabled;
  is_handwriting_enabled_ = is_handwriting_enabled;
  is_voice_enabled_ = is_voice_enabled;
}

void ImeController::SetCapsLockFromTray(bool caps_enabled) {
  if (client_)
    client_->SetCapsLockFromTray(caps_enabled);
}

void ImeController::FlushMojoForTesting() {
  client_.FlushForTesting();
}

bool ImeController::IsCapsLockEnabled() const {
  return is_caps_lock_enabled_;
}

std::vector<std::string> ImeController::GetCandidateImesForAccelerator(
    const ui::Accelerator& accelerator) const {
  std::vector<std::string> candidate_ids;

  using chromeos::extension_ime_util::GetInputMethodIDByEngineID;
  std::vector<std::string> input_method_ids_to_switch;
  switch (accelerator.key_code()) {
    case ui::VKEY_CONVERT:  // Henkan key on JP106 keyboard
      input_method_ids_to_switch.push_back(
          GetInputMethodIDByEngineID("nacl_mozc_jp"));
      break;
    case ui::VKEY_NONCONVERT:  // Muhenkan key on JP106 keyboard
      input_method_ids_to_switch.push_back(
          GetInputMethodIDByEngineID("xkb:jp::jpn"));
      break;
    case ui::VKEY_DBE_SBCSCHAR:  // ZenkakuHankaku key on JP106 keyboard
    case ui::VKEY_DBE_DBCSCHAR:
      input_method_ids_to_switch.push_back(
          GetInputMethodIDByEngineID("nacl_mozc_jp"));
      input_method_ids_to_switch.push_back(
          GetInputMethodIDByEngineID("xkb:jp::jpn"));
      break;
    default:
      break;
  }
  if (input_method_ids_to_switch.empty()) {
    DVLOG(1) << "Unexpected VKEY: " << accelerator.key_code();
    return std::vector<std::string>();
  }

  // Obtain the intersection of input_method_ids_to_switch and available_imes_.
  for (const mojom::ImeInfo& ime : available_imes_) {
    if (base::ContainsValue(input_method_ids_to_switch, ime.id))
      candidate_ids.push_back(ime.id);
  }
  return candidate_ids;
}

}  // namespace ash
