// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/keyboard_hook_handler.h"

#include <map>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace {
// Some helper routines used to construct keyboard event.

// Return true of WPARAM corresponds to a UP keyboard event.
bool IsKeyUp(WPARAM w_param) {
  return (w_param == WM_KEYUP) || (w_param == WM_SYSKEYUP);
}

// Check if the given bit is set.
bool IsBitSet(ULONG value, ULONG mask) {
  return ((value & mask) != 0);
}

// Get Window handle from widget.
HWND GetWindowHandle(views::Widget* widget) {
  return widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
}

// Return the location independent keycode corresponding to given keycode (e.g.
// return shift when left/right shift is pressed). This is needed as low level
// hooks get location information which is not returned as part of normal window
// keyboard events.
DWORD RemoveLocationOnKeycode(DWORD vk_code) {
  // Virtual keycode from low level hook include location while window messages
  // does not. So convert them to be without location.
  switch (vk_code) {
    case VK_LSHIFT:
    case VK_RSHIFT:
      return VK_SHIFT;
    case VK_LCONTROL:
    case VK_RCONTROL:
      return VK_CONTROL;
    case VK_LMENU:
    case VK_RMENU:
      return VK_MENU;
  }
  return vk_code;
}

// Construct LPARAM corresponding to the given low level hook callback
// structure.
LPARAM GetLParamFromHookStruct(WPARAM w_param, KBDLLHOOKSTRUCT* hook_struct) {
  ULONG key_state = 0;
  // There is no way to get repeat count so always set it to 1.
  key_state = 1;

  // Scan code.
  key_state |= (hook_struct->scanCode & 0xFF) << 16;

  // Extended key when the event is received as part window event and so skip
  // it.

  // Context code.
  key_state |= IsBitSet(hook_struct->flags, LLKHF_ALTDOWN) << 29;

  // Previous key state - set to 1 for KEYUP events.
  key_state |= IsKeyUp(w_param) << 30;

  // Transition state.
  key_state |= IsBitSet(hook_struct->flags, LLKHF_UP) << 31;

  return static_cast<LPARAM>(key_state);
}

// List of key state that we want to save.
const int kKeysToSave[] = {VK_SHIFT, VK_CONTROL, VK_MENU};

// Make sure that we are not going to run out of bits saving the state.
C_ASSERT((arraysize(kKeysToSave) * 2) <= (sizeof(WPARAM) * 8));

// Save keyboard state to WPARAM so it can be restored later before the keyboard
// message is processed in the main thread. This is necessary for
// GetKeyboardState() to work as keyboard state will be different by the time
// main thread processes the message.
WPARAM SaveKeyboardState() {
  WPARAM value = 0;

  for (int index = 0; index < arraysize(kKeysToSave); index++) {
    value <<= 2;
    SHORT key_state = GetAsyncKeyState(kKeysToSave[index]);
    value |= ((IsBitSet(key_state, 0x8000) ? 0x2 : 0) |
              (IsBitSet(key_state, 0x1) ? 0x1 : 0));
  }
  return value;
}

// Restore keyboard state based on saved values.
bool RestoreKeyboardState(WPARAM w_param) {
  const int kKeyboardStateLength = 256;
  BYTE keyboard_state[kKeyboardStateLength];
  if (!GetKeyboardState(keyboard_state)) {
    DVLOG(ERROR) << "Error getting keyboard state";
    return false;
  }

  // restore in the reverse order of what was saved so we have the right bit for
  // each key that was saved.
  for (int index = arraysize(kKeysToSave) - 1; index >= 0; index--) {
    int key = kKeysToSave[index];
    keyboard_state[key] =
        (IsBitSet(w_param, 0x2) ? 0x80 : 0) | (IsBitSet(w_param, 0x1) ? 1 : 0);
    w_param >>= 2;
  }

  if (!SetKeyboardState(keyboard_state)) {
    DVLOG(ERROR) << "Error setting keyboard state";
    return false;
  }

  return true;
}

// Data corresponding to keyboard event.
struct KeyboardEventInfo {
  UINT message_id;
  WPARAM event_w_param;
  LPARAM event_l_param;
  WPARAM keyboard_state_to_restore;
};

// Maintains low level registration for a window.
class KeyboardInterceptRegistration {
 public:
  KeyboardInterceptRegistration();

  // Are there any keyboard events queued.
  bool IsKeyboardEventQueueEmpty();

  // Insert keyboard event in the queue.
  void QueueKeyboardEvent(const KeyboardEventInfo& info);

  KeyboardEventInfo DequeueKeyboardEvent();

 private:
  std::queue<KeyboardEventInfo> keyboard_events_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardInterceptRegistration);
};

// Implements low level hook and manages registration for all the windows.
class LowLevelHookHandler {
 public:
  // Request all keyboard events to be routed to the given window.
  void Register(HWND window_handle);

  // Release the request for all keyboard events.
  void Deregister(HWND window_handle);

  // Get singleton instance.
  static LowLevelHookHandler* GetInstance();

 private:
  // Private constructor/destructor so it is accessible only
  // DefaultSingletonTraits.
  friend struct DefaultSingletonTraits<LowLevelHookHandler>;
  LowLevelHookHandler();

  ~LowLevelHookHandler();

  // Low level keyboard hook processing related functions.
  // Hook callback called from the OS.
  static LRESULT CALLBACK
  KeyboardHook(int code, WPARAM w_param, LPARAM l_param);

  // Low level keyboard hook handler.
  LRESULT HandleKeyboardHook(int code, WPARAM w_param, LPARAM l_param);

  // Message filter to set keyboard state based on private message.
  static LRESULT CALLBACK
  MessageFilterHook(int code, WPARAM w_param, LPARAM l_param);

  // Message filter handler.
  LRESULT HandleMessageFilterHook(int code, WPARAM w_param, LPARAM l_param);

  bool EnableHooks();
  void DisableHooks();

  // Hook handle for window message to set keyboard state based on private
  // message.
  HHOOK message_filter_hook_;

  // Hook handle for low level keyboard hook.
  HHOOK keyboard_hook_;

  // Private window message to set keyboard state for the thread.
  UINT restore_keyboard_state_message_id_;

  // Private message to inject keyboard event after current injected message is
  // processed.
  UINT inject_keyboard_event_message_id_;

  // There is no lock protecting this list as the low level hook callbacks are
  // executed on same thread that registered the hook and there is only one
  // thread
  // that execute all view code in browser.
  base::ScopedPtrHashMap<HWND, KeyboardInterceptRegistration> registrations_;

  DISALLOW_COPY_AND_ASSIGN(LowLevelHookHandler);
};

KeyboardInterceptRegistration::KeyboardInterceptRegistration() {
}

bool KeyboardInterceptRegistration::IsKeyboardEventQueueEmpty() {
  return keyboard_events_.empty();
}

void KeyboardInterceptRegistration::QueueKeyboardEvent(
    const KeyboardEventInfo& info) {
  keyboard_events_.push(info);
}

KeyboardEventInfo KeyboardInterceptRegistration::DequeueKeyboardEvent() {
  KeyboardEventInfo info = keyboard_events_.front();
  keyboard_events_.pop();
  return info;
}

LowLevelHookHandler::LowLevelHookHandler()
    : message_filter_hook_(NULL),
      keyboard_hook_(NULL),
      restore_keyboard_state_message_id_(0),
      inject_keyboard_event_message_id_(0) {
  restore_keyboard_state_message_id_ =
      RegisterWindowMessage(L"chrome:restore_keyboard_state");
  inject_keyboard_event_message_id_ =
      RegisterWindowMessage(L"chrome:inject_keyboard_event");
}

LowLevelHookHandler::~LowLevelHookHandler() {
  DisableHooks();
}

// static
LRESULT CALLBACK
LowLevelHookHandler::KeyboardHook(int code, WPARAM w_param, LPARAM l_param) {
  return GetInstance()->HandleKeyboardHook(code, w_param, l_param);
}

// static
LRESULT CALLBACK LowLevelHookHandler::MessageFilterHook(int code,
                                                        WPARAM w_param,
                                                        LPARAM l_param) {
  return GetInstance()->HandleMessageFilterHook(code, w_param, l_param);
}

// static
LowLevelHookHandler* LowLevelHookHandler::GetInstance() {
  return Singleton<LowLevelHookHandler,
                   DefaultSingletonTraits<LowLevelHookHandler>>::get();
}

void LowLevelHookHandler::Register(HWND window_handle) {
  if (registrations_.contains(window_handle))
    return;

  if (!EnableHooks())
    return;

  scoped_ptr<KeyboardInterceptRegistration> registration(
      new KeyboardInterceptRegistration());
  registrations_.add(window_handle, registration.Pass());
}

void LowLevelHookHandler::Deregister(HWND window_handle) {
  registrations_.erase(window_handle);
  if (registrations_.empty())
    DisableHooks();

  DVLOG(1) << "Keyboard hook unregistered for handle = " << window_handle;
}

bool LowLevelHookHandler::EnableHooks() {
  // Make sure that hook is set from main thread as it has to be valid for
  // the lifetime of the registration.
  DCHECK(base::MessageLoopForUI::IsCurrent());

  if (keyboard_hook_)
    return true;

  message_filter_hook_ = SetWindowsHookEx(WH_MSGFILTER, MessageFilterHook, NULL,
                                          GetCurrentThreadId());
  if (message_filter_hook_ == NULL) {
    DVLOG(ERROR) << "Error calling SetWindowsHookEx() to set message hook, "
                 << "gle = " << GetLastError();
    return false;
  }

  DCHECK(keyboard_hook_ == NULL) << "Keyboard hook already registered";

  keyboard_hook_ = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHook, NULL, 0);
  if (keyboard_hook_ == NULL) {
    DVLOG(ERROR) << "Error calling SetWindowsHookEx() - GLE = "
                 << GetLastError();
    DisableHooks();
    return false;
  }

  return true;
}

void LowLevelHookHandler::DisableHooks() {
  if (keyboard_hook_ != NULL) {
    UnhookWindowsHookEx(keyboard_hook_);
    keyboard_hook_ = NULL;
  }

  if (message_filter_hook_ != NULL) {
    UnhookWindowsHookEx(message_filter_hook_);
    message_filter_hook_ = NULL;
  }
}

LRESULT
LowLevelHookHandler::HandleMessageFilterHook(int code,
                                             WPARAM w_param,
                                             LPARAM l_param) {
  // Ignore if not called from main message loop.
  if (code != base::MessagePumpForUI::kMessageFilterCode)
    return CallNextHookEx(NULL, code, w_param, l_param);

  MSG* msg = reinterpret_cast<MSG*>(l_param);
  if (msg->message == restore_keyboard_state_message_id_) {
    RestoreKeyboardState(msg->wParam);
    return true;
  } else if (msg->message == inject_keyboard_event_message_id_) {
    KeyboardInterceptRegistration* registration = registrations_.get(msg->hwnd);
    if (registration) {
      // Post keyboard state and key event to main thread for processing.
      KeyboardEventInfo event_info = registration->DequeueKeyboardEvent();
      PostMessage(msg->hwnd, restore_keyboard_state_message_id_,
                  event_info.keyboard_state_to_restore, static_cast<LPARAM>(0));

      PostMessage(msg->hwnd, event_info.message_id, event_info.event_w_param,
                  event_info.event_l_param);

      if (!registration->IsKeyboardEventQueueEmpty()) {
        // Post another inject keyboard event if there are more key events to
        // process after the current injected event is processed.
        PostMessage(msg->hwnd, inject_keyboard_event_message_id_,
                    static_cast<WPARAM>(0), static_cast<LPARAM>(0));
      }

      return true;
    }
  }

  return CallNextHookEx(NULL, code, w_param, l_param);
}

LRESULT
LowLevelHookHandler::HandleKeyboardHook(int code,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  HWND current_active_window = GetForegroundWindow();

  // Additional check to make sure that the current window is indeed owned by
  // this proccess.
  DWORD pid = 0;
  ::GetWindowThreadProcessId(current_active_window, &pid);
  if (!pid || (pid != ::GetCurrentProcessId()))
    return CallNextHookEx(NULL, code, w_param, l_param);

  if ((code >= 0) && (current_active_window != NULL)) {
    KeyboardInterceptRegistration* registration =
        registrations_.get(current_active_window);
    if (registration) {
      // Save keyboard state to queue and post message to handle keyboard event
      // if the queue is not empty. It is done this way as keyboard state should
      // be preserved until char event corresponding to the keyboard event is
      // handled (so correct alt/shift/control key state is set). Also
      // SendMessage() cannot be used as it would bypass both message loop
      // delegates and TransalateMessage() calls (which will inserts char
      // events).
      PKBDLLHOOKSTRUCT hook_struct =
          reinterpret_cast<PKBDLLHOOKSTRUCT>(l_param);

      KeyboardEventInfo event_info = {0};
      event_info.message_id = w_param;
      event_info.event_w_param = RemoveLocationOnKeycode(hook_struct->vkCode);
      event_info.event_l_param = GetLParamFromHookStruct(w_param, hook_struct);
      event_info.keyboard_state_to_restore = SaveKeyboardState();

      bool should_queue_inject_event =
          registration->IsKeyboardEventQueueEmpty();

      registration->QueueKeyboardEvent(event_info);
      if (should_queue_inject_event) {
        PostMessage(current_active_window, inject_keyboard_event_message_id_,
                    static_cast<WPARAM>(0), static_cast<LPARAM>(0));
      }
      return 1;
    }
  }

  return CallNextHookEx(NULL, code, w_param, l_param);
}
}  // namespace

KeyboardHookHandler* KeyboardHookHandler::GetInstance() {
  return Singleton<KeyboardHookHandler,
                   DefaultSingletonTraits<KeyboardHookHandler>>::get();
}

void KeyboardHookHandler::Register(views::Widget* widget) {
  LowLevelHookHandler::GetInstance()->Register(GetWindowHandle(widget));
}

void KeyboardHookHandler::Deregister(views::Widget* widget) {
  LowLevelHookHandler::GetInstance()->Deregister(GetWindowHandle(widget));
}
