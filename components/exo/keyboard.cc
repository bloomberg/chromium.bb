// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/keyboard.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/seat.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace {

// Delay until a key state change expected to be acknowledged is expired.
const int kExpirationDelayForPendingKeyAcksMs = 1000;

// These modifiers reflect what clients are supposed to be aware of.
// I.e. EF_SCROLL_LOCK_ON is missing because clients are not supposed
// to be aware scroll lock.
const int kModifierMask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                          ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN |
                          ui::EF_NUM_LOCK_ON | ui::EF_CAPS_LOCK_ON;

// The accelerator keys reserved to be processed by chrome.
const struct {
  ui::KeyboardCode keycode;
  int modifiers;
} kReservedAccelerators[] = {
    {ui::VKEY_SPACE, ui::EF_CONTROL_DOWN},
    {ui::VKEY_SPACE, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN},
    {ui::VKEY_F13, ui::EF_NONE},
    {ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN}};

bool ProcessAccelerator(Surface* surface, const ui::KeyEvent* event) {
  views::Widget* widget =
      views::Widget::GetTopLevelWidgetForNativeView(surface->window());
  if (widget) {
    views::FocusManager* focus_manager = widget->GetFocusManager();
    return focus_manager->ProcessAccelerator(ui::Accelerator(*event));
  }
  return false;
}

bool ConsumedByIme(Surface* focus, const ui::KeyEvent* event) {
  // Check if IME consumed the event, to avoid it to be doubly processed.
  // First let us see whether IME is active and is in text input mode.
  views::Widget* widget =
      views::Widget::GetTopLevelWidgetForNativeView(focus->window());
  ui::InputMethod* ime = widget ? widget->GetInputMethod() : nullptr;
  if (!ime || ime->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return false;

  // Case 1:
  // When IME ate a key event but did not emit character insertion event yet
  // (e.g., when it is still showing a candidate list UI to the user,) the
  // consumed key event is re-sent after masked |key_code| by VKEY_PROCESSKEY.
  if (event->key_code() == ui::VKEY_PROCESSKEY)
    return true;

  // Case 2:
  // When IME ate a key event and generated a single character input, it leaves
  // the key event as-is, and in addition calls the active ui::TextInputClient's
  // InsertChar() method. (In our case, arc::ArcImeService::InsertChar()).
  //
  // In Chrome OS (and Web) convention, the two calls wont't cause duplicates,
  // because key-down events do not mean any character inputs there.
  // (InsertChar issues a DOM "keypress" event, which is distinct from keydown.)
  // Unfortunately, this is not necessary the case for our clients that may
  // treat keydown as a trigger of text inputs. We need suppression for keydown.
  if (event->type() == ui::ET_KEY_PRESSED) {
    // Same condition as components/arc/ime/arc_ime_service.cc#InsertChar.
    const base::char16 ch = event->GetCharacter();
    const bool is_control_char =
        (0x00 <= ch && ch <= 0x1f) || (0x7f <= ch && ch <= 0x9f);
    if (!is_control_char && !ui::IsSystemKeyModifier(event->flags()))
      return true;
  }

  // Case 3:
  // Workaround for apps that doesn't handle hardware keyboard events well.
  // Keys typically on software keyboard and lack of them are fatal, namely,
  // unmodified enter and backspace keys, are sent through IME.
  constexpr int kModifierMask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                                ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN;
  // Same condition as components/arc/ime/arc_ime_service.cc#InsertChar.
  if ((event->flags() & kModifierMask) == 0) {
    if (event->key_code() == ui::VKEY_RETURN ||
        event->key_code() == ui::VKEY_BACK) {
      return true;
    }
  }

  return false;
}

bool IsPhysicalKeyboardEnabled() {
  // The internal keyboard is enabled if tablet mode is not enabled.
  if (!WMHelper::GetInstance()->IsTabletModeWindowManagerEnabled())
    return true;

  for (auto& keyboard :
       ui::InputDeviceManager::GetInstance()->GetKeyboardDevices()) {
    if (keyboard.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL)
      return true;
  }
  return false;
}

bool IsReservedAccelerator(const ui::KeyEvent* event) {
  for (const auto& accelerator : kReservedAccelerators) {
    if (event->flags() == accelerator.modifiers &&
        event->key_code() == accelerator.keycode) {
      return true;
    }
  }
  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Keyboard, public:

Keyboard::Keyboard(KeyboardDelegate* delegate, Seat* seat)
    : delegate_(delegate),
      seat_(seat),
      expiration_delay_for_pending_key_acks_(base::TimeDelta::FromMilliseconds(
          kExpirationDelayForPendingKeyAcksMs)),
      weak_ptr_factory_(this) {
  auto* helper = WMHelper::GetInstance();
  AddEventHandler();
  seat_->AddObserver(this);
  helper->AddTabletModeObserver(this);
  helper->AddInputDeviceEventObserver(this);
  OnSurfaceFocused(seat_->GetFocusedSurface());
}

Keyboard::~Keyboard() {
  for (KeyboardObserver& observer : observer_list_)
    observer.OnKeyboardDestroying(this);
  if (focus_)
    focus_->RemoveSurfaceObserver(this);
  auto* helper = WMHelper::GetInstance();
  RemoveEventHandler();
  seat_->RemoveObserver(this);
  helper->RemoveTabletModeObserver(this);
  helper->RemoveInputDeviceEventObserver(this);
}

bool Keyboard::HasDeviceConfigurationDelegate() const {
  return !!device_configuration_delegate_;
}

void Keyboard::SetDeviceConfigurationDelegate(
    KeyboardDeviceConfigurationDelegate* delegate) {
  device_configuration_delegate_ = delegate;
  OnKeyboardDeviceConfigurationChanged();
}

void Keyboard::AddObserver(KeyboardObserver* observer) {
  observer_list_.AddObserver(observer);
}

bool Keyboard::HasObserver(KeyboardObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void Keyboard::RemoveObserver(KeyboardObserver* observer) {
  observer_list_.HasObserver(observer);
}

void Keyboard::SetNeedKeyboardKeyAcks(bool need_acks) {
  RemoveEventHandler();
  are_keyboard_key_acks_needed_ = need_acks;
  AddEventHandler();
}

bool Keyboard::AreKeyboardKeyAcksNeeded() const {
  return are_keyboard_key_acks_needed_;
}

void Keyboard::AckKeyboardKey(uint32_t serial, bool handled) {
  auto it = pending_key_acks_.find(serial);
  if (it == pending_key_acks_.end())
    return;

  if (!handled && focus_)
    ProcessAccelerator(focus_, &it->second.first);
  pending_key_acks_.erase(serial);
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Keyboard::OnKeyEvent(ui::KeyEvent* event) {
  int modifier_flags = event->flags() & kModifierMask;
  if (modifier_flags != modifier_flags_) {
    modifier_flags_ = modifier_flags;
    if (focus_)
      delegate_->OnKeyboardModifiers(modifier_flags_);
  }

  // When IME ate a key event, we use the event only for tracking key states and
  // ignore for further processing. Otherwise it is handled in two places (IME
  // and client) and causes undesired behavior.
  bool consumed_by_ime = focus_ ? ConsumedByIme(focus_, event) : false;

  switch (event->type()) {
    case ui::ET_KEY_PRESSED:
      if (focus_ && !consumed_by_ime && !IsReservedAccelerator(event)) {
        uint32_t serial =
            delegate_->OnKeyboardKey(event->time_stamp(), event->code(), true);
        if (are_keyboard_key_acks_needed_) {
          pending_key_acks_.insert(
              {serial,
               {*event, base::TimeTicks::Now() +
                            expiration_delay_for_pending_key_acks_}});
          event->SetHandled();
        }
      }
      break;
    case ui::ET_KEY_RELEASED:
      if (focus_ && !consumed_by_ime && !IsReservedAccelerator(event)) {
        uint32_t serial =
            delegate_->OnKeyboardKey(event->time_stamp(), event->code(), false);
        if (are_keyboard_key_acks_needed_) {
          pending_key_acks_.insert(
              {serial,
               {*event, base::TimeTicks::Now() +
                            expiration_delay_for_pending_key_acks_}});
          event->SetHandled();
        }
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  if (pending_key_acks_.empty())
    return;
  if (process_expired_pending_key_acks_pending_)
    return;

  ScheduleProcessExpiredPendingKeyAcks(expiration_delay_for_pending_key_acks_);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Keyboard::OnSurfaceDestroying(Surface* surface) {
  DCHECK(surface == focus_);
  SetFocus(nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// ui::InputDeviceEventObserver overrides:

void Keyboard::OnKeyboardDeviceConfigurationChanged() {
  if (device_configuration_delegate_) {
    device_configuration_delegate_->OnKeyboardTypeChanged(
        IsPhysicalKeyboardEnabled());
  }
}

////////////////////////////////////////////////////////////////////////////////
// ash::TabletModeObserver overrides:

void Keyboard::OnTabletModeStarted() {
  OnKeyboardDeviceConfigurationChanged();
}

void Keyboard::OnTabletModeEnding() {}

void Keyboard::OnTabletModeEnded() {
  OnKeyboardDeviceConfigurationChanged();
}

////////////////////////////////////////////////////////////////////////////////
// SeatObserver overrides:

void Keyboard::OnSurfaceFocusing(Surface* gaining_focus) {}

void Keyboard::OnSurfaceFocused(Surface* gained_focus) {
  Surface* gained_focus_surface =
      gained_focus && delegate_->CanAcceptKeyboardEventsForSurface(gained_focus)
          ? gained_focus
          : nullptr;
  if (gained_focus_surface != focus_)
    SetFocus(gained_focus_surface);
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard, private:

void Keyboard::SetFocus(Surface* surface) {
  if (focus_) {
    delegate_->OnKeyboardLeave(focus_);
    focus_->RemoveSurfaceObserver(this);
    focus_ = nullptr;
    pending_key_acks_.clear();
  }
  if (surface) {
    modifier_flags_ = seat_->modifier_flags() & kModifierMask;
    delegate_->OnKeyboardModifiers(modifier_flags_);
    delegate_->OnKeyboardEnter(surface, seat_->pressed_keys());
    focus_ = surface;
    focus_->AddSurfaceObserver(this);
  }
}

void Keyboard::ProcessExpiredPendingKeyAcks() {
  DCHECK(process_expired_pending_key_acks_pending_);
  process_expired_pending_key_acks_pending_ = false;

  // Check pending acks and process them as if it's not handled if
  // expiration time passed.
  base::TimeTicks current_time = base::TimeTicks::Now();

  while (!pending_key_acks_.empty()) {
    auto it = pending_key_acks_.begin();
    const ui::KeyEvent event = it->second.first;

    if (it->second.second > current_time)
      break;

    pending_key_acks_.erase(it);

    // |pending_key_acks_| may change and an iterator of it become invalid when
    // |ProcessAccelerator| is called.
    if (focus_)
      ProcessAccelerator(focus_, &event);
  }

  if (pending_key_acks_.empty())
    return;

  base::TimeDelta delay_until_next_process_expired_pending_key_acks =
      pending_key_acks_.begin()->second.second - current_time;
  ScheduleProcessExpiredPendingKeyAcks(
      delay_until_next_process_expired_pending_key_acks);
}

void Keyboard::ScheduleProcessExpiredPendingKeyAcks(base::TimeDelta delay) {
  DCHECK(!process_expired_pending_key_acks_pending_);
  process_expired_pending_key_acks_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Keyboard::ProcessExpiredPendingKeyAcks,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void Keyboard::AddEventHandler() {
  auto* helper = WMHelper::GetInstance();
  if (are_keyboard_key_acks_needed_)
    helper->AddPreTargetHandler(this);
  else
    helper->AddPostTargetHandler(this);
}

void Keyboard::RemoveEventHandler() {
  auto* helper = WMHelper::GetInstance();
  if (are_keyboard_key_acks_needed_)
    helper->RemovePreTargetHandler(this);
  else
    helper->RemovePostTargetHandler(this);
}

}  // namespace exo
