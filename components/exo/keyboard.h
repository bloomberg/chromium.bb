// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEYBOARD_H_
#define COMPONENTS_EXO_KEYBOARD_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wm_helper.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace ui {
enum class DomCode;
class KeyEvent;
}

namespace exo {
class KeyboardDelegate;
class KeyboardDeviceConfigurationDelegate;
class Surface;

// This class implements a client keyboard that represents one or more keyboard
// devices.
class Keyboard : public ui::EventHandler,
                 public WMHelper::FocusObserver,
                 public WMHelper::InputDeviceEventObserver,
                 public WMHelper::TabletModeObserver,
                 public SurfaceObserver {
 public:
  explicit Keyboard(KeyboardDelegate* delegate);
  ~Keyboard() override;

  bool HasDeviceConfigurationDelegate() const;
  void SetDeviceConfigurationDelegate(
      KeyboardDeviceConfigurationDelegate* delegate);

  // Management of the observer list.
  void AddObserver(KeyboardObserver* observer);
  bool HasObserver(KeyboardObserver* observer) const;
  void RemoveObserver(KeyboardObserver* observer);

  void SetNeedKeyboardKeyAcks(bool need_acks);
  bool AreKeyboardKeyAcksNeeded() const;

  void AckKeyboardKey(uint32_t serial, bool handled);

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Overridden WMHelper::FocusObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // Overridden from ui::InputDeviceEventObserver:
  void OnKeyboardDeviceConfigurationChanged() override;

  // Overridden from WMHelper::TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnding() override;
  void OnTabletModeEnded() override;

 private:
  // Returns the effective focus for |window|.
  Surface* GetEffectiveFocus(aura::Window* window) const;

  // Processes expired key state changes in |pending_key_acks_| as they have not
  // been acknowledged.
  void ProcessExpiredPendingKeyAcks();

  // Schedule next call of ProcessExpiredPendingKeyAcks after |delay|
  void ScheduleProcessExpiredPendingKeyAcks(base::TimeDelta delay);

  // Adds/Removes pre or post event handler depending on if key acks are needed.
  // If key acks are needed, pre target handler will be added because this class
  // wants to dispatch keys before they are consumed by Chrome. Otherwise, post
  // target handler will be added because all accelerators should be handled by
  // Chrome before they are dispatched by this class.
  void AddEventHandler();
  void RemoveEventHandler();

  // The delegate instance that all events except for events about device
  // configuration are dispatched to.
  KeyboardDelegate* const delegate_;

  // The delegate instance that events about device configuration are dispatched
  // to.
  KeyboardDeviceConfigurationDelegate* device_configuration_delegate_ = nullptr;

  // Indicates that each key event is expected to be acknowledged.
  bool are_keyboard_key_acks_needed_ = false;

  // The current focus surface for the keyboard.
  Surface* focus_ = nullptr;

  // Vector of currently pressed keys.
  std::vector<ui::DomCode> pressed_keys_;

  // Current set of modifier flags.
  int modifier_flags_ = 0;

  // Key state changes that are expected to be acknowledged.
  using KeyStateChange = std::pair<ui::KeyEvent, base::TimeTicks>;
  base::flat_map<uint32_t, KeyStateChange> pending_key_acks_;

  // Indicates that a ProcessExpiredPendingKeyAcks call is pending.
  bool process_expired_pending_key_acks_pending_ = false;

  // Delay until a key state change expected to be acknowledged is expired.
  const base::TimeDelta expiration_delay_for_pending_key_acks_;

  base::ObserverList<KeyboardObserver> observer_list_;

  base::WeakPtrFactory<Keyboard> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Keyboard);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_H_
