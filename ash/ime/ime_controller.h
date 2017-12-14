// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_IME_CONTROLLER_H_
#define ASH_IME_IME_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/interfaces/ime_controller.mojom.h"
#include "ash/public/interfaces/ime_info.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ui {
class Accelerator;
}

namespace ash {

// Connects ash IME users (e.g. the system tray) to the IME implementation,
// which might live in Chrome browser or in a separate mojo service.
class ASH_EXPORT ImeController : public mojom::ImeController {
 public:
  class Observer {
   public:
    // Called when the caps lock state has changed.
    virtual void OnCapsLockChanged(bool enabled) = 0;
  };

  ImeController();
  ~ImeController() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const mojom::ImeInfo& current_ime() const { return current_ime_; }

  const std::vector<mojom::ImeInfo>& available_imes() const {
    return available_imes_;
  }

  bool is_extra_input_options_enabled() const {
    return is_extra_input_options_enabled_;
  }
  bool is_emoji_enabled() const { return is_emoji_enabled_; }
  bool is_handwriting_enabled() const { return is_handwriting_enabled_; }
  bool is_voice_enabled() const { return is_voice_enabled_; }

  bool managed_by_policy() const { return managed_by_policy_; }

  const std::vector<mojom::ImeMenuItem>& current_ime_menu_items() const {
    return current_ime_menu_items_;
  }

  // Binds the mojo interface to this object.
  void BindRequest(mojom::ImeControllerRequest request);

  // Returns true if switching to next/previous IME is allowed.
  bool CanSwitchIme() const;

  // Wrappers for mojom::ImeControllerClient methods.
  void SwitchToNextIme();
  void SwitchToPreviousIme();
  void SwitchImeById(const std::string& ime_id, bool show_message);
  void ActivateImeMenuItem(const std::string& key);
  void SetCapsLockFromTray(bool caps_enabled);

  // Returns true if the switch is allowed and the keystroke should be
  // consumed.
  bool CanSwitchImeWithAccelerator(const ui::Accelerator& accelerator) const;

  void SwitchImeWithAccelerator(const ui::Accelerator& accelerator);

  // mojom::ImeController:
  void SetClient(mojom::ImeControllerClientPtr client) override;
  void RefreshIme(const std::string& current_ime_id,
                  std::vector<mojom::ImeInfoPtr> available_imes,
                  std::vector<mojom::ImeMenuItemPtr> menu_items) override;
  void SetImesManagedByPolicy(bool managed) override;
  void ShowImeMenuOnShelf(bool show) override;
  void SetCapsLockState(bool caps_enabled) override;

  void SetExtraInputOptionsEnabledState(bool is_extra_input_options_enabled,
                                        bool is_emoji_enabled,
                                        bool is_handwriting_enabled,
                                        bool is_voice_enabled) override;

  // Synchronously returns the cached caps lock state.
  bool IsCapsLockEnabled() const;

  void FlushMojoForTesting();

 private:
  // Returns the IDs of the subset of input methods which are active and are
  // associated with |accelerator|. For example, two Japanese IMEs can be
  // returned for ui::VKEY_DBE_SBCSCHAR if both are active.
  std::vector<std::string> GetCandidateImesForAccelerator(
      const ui::Accelerator& accelerator) const;

  // Bindings for users of the mojo interface.
  mojo::BindingSet<mojom::ImeController> bindings_;

  // Client interface back to IME code in chrome.
  mojom::ImeControllerClientPtr client_;

  // Copy of the current IME so we can return it by reference.
  mojom::ImeInfo current_ime_;

  // "Available" IMEs are both installed and enabled by the user in settings.
  std::vector<mojom::ImeInfo> available_imes_;

  // True if the available IMEs are currently managed by enterprise policy.
  // For example, can occur at the login screen with device-level policy.
  bool managed_by_policy_ = false;

  // Additional menu items for properties of the currently selected IME.
  std::vector<mojom::ImeMenuItem> current_ime_menu_items_;

  // A slightly delayed state value that is updated by asynchronously reported
  // changes from the ImeControllerClient client (source of truth) which is in
  // another process. This is required for synchronous method calls in ash.
  bool is_caps_lock_enabled_ = false;

  // True if the extended inputs should be available in general (emoji,
  // handwriting, voice).
  bool is_extra_input_options_enabled_ = false;

  // True if emoji input should be available from the IME menu.
  bool is_emoji_enabled_ = false;

  // True if handwriting input should be available from the IME menu.
  bool is_handwriting_enabled_ = false;

  // True if voice input should be available from the IME menu.
  bool is_voice_enabled_ = false;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ImeController);
};

}  // namespace ash

#endif  // ASH_IME_IME_CONTROLLER_H_
