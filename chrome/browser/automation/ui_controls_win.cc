// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"
#include "views/view.h"

namespace ui_controls {

namespace {

// InputDispatcher ------------------------------------------------------------

// InputDispatcher is used to listen for a mouse/keyboard event. When the
// appropriate event is received the task is notified.
class InputDispatcher : public base::RefCounted<InputDispatcher> {
 public:
  InputDispatcher(Task* task, WPARAM message_waiting_for);

  // Invoked from the hook. If mouse_message matches message_waiting_for_
  // MatchingMessageFound is invoked.
  void DispatchedMessage(WPARAM mouse_message);

  // Invoked when a matching event is found. Uninstalls the hook and schedules
  // an event that notifies the task.
  void MatchingMessageFound();

 private:
  friend class base::RefCounted<InputDispatcher>;

  ~InputDispatcher();

  // Notifies the task and release this (which should delete it).
  void NotifyTask();

  // The task we notify.
  scoped_ptr<Task> task_;

  // Message we're waiting for. Not used for keyboard events.
  const WPARAM message_waiting_for_;

  DISALLOW_COPY_AND_ASSIGN(InputDispatcher);
};

// Have we installed the hook?
bool installed_hook_ = false;

// Return value from SetWindowsHookEx.
HHOOK next_hook_ = NULL;

// If a hook is installed, this is the dispatcher.
InputDispatcher* current_dispatcher_ = NULL;

// Callback from hook when a mouse message is received.
LRESULT CALLBACK MouseHook(int n_code, WPARAM w_param, LPARAM l_param) {
  HHOOK next_hook = next_hook_;
  if (n_code == HC_ACTION) {
    DCHECK(current_dispatcher_);
    current_dispatcher_->DispatchedMessage(w_param);
  }
  return CallNextHookEx(next_hook, n_code, w_param, l_param);
}

// Callback from hook when a key message is received.
LRESULT CALLBACK KeyHook(int n_code, WPARAM w_param, LPARAM l_param) {
  HHOOK next_hook = next_hook_;
  if (n_code == HC_ACTION) {
    DCHECK(current_dispatcher_);
    if (l_param & (1 << 30)) {
      // Only send on key up.
      current_dispatcher_->MatchingMessageFound();
    }
  }
  return CallNextHookEx(next_hook, n_code, w_param, l_param);
}

// Installs dispatcher as the current hook.
void InstallHook(InputDispatcher* dispatcher, bool key_hook) {
  DCHECK(!installed_hook_);
  current_dispatcher_ = dispatcher;
  installed_hook_ = true;
  if (key_hook) {
    next_hook_ = SetWindowsHookEx(WH_KEYBOARD, &KeyHook, NULL,
                                  GetCurrentThreadId());
  } else {
    // NOTE: I originally tried WH_CALLWNDPROCRET, but for some reason I
    // didn't get a mouse message like I do with MouseHook.
    next_hook_ = SetWindowsHookEx(WH_MOUSE, &MouseHook, NULL,
                                  GetCurrentThreadId());
  }
  DCHECK(next_hook_);
}

// Uninstalls the hook set in InstallHook.
void UninstallHook(InputDispatcher* dispatcher) {
  if (current_dispatcher_ == dispatcher) {
    installed_hook_ = false;
    current_dispatcher_ = NULL;
    UnhookWindowsHookEx(next_hook_);
  }
}

InputDispatcher::InputDispatcher(Task* task, UINT message_waiting_for)
    : task_(task), message_waiting_for_(message_waiting_for) {
  InstallHook(this, message_waiting_for == WM_KEYUP);
}

InputDispatcher::~InputDispatcher() {
  // Make sure the hook isn't installed.
  UninstallHook(this);
}

void InputDispatcher::DispatchedMessage(WPARAM message) {
  if (message == message_waiting_for_)
    MatchingMessageFound();
}

void InputDispatcher::MatchingMessageFound() {
  UninstallHook(this);
  // At the time we're invoked the event has not actually been processed.
  // Use PostTask to make sure the event has been processed before notifying.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, NewRunnableMethod(this, &InputDispatcher::NotifyTask), 0);
}

void InputDispatcher::NotifyTask() {
  task_->Run();
  Release();
}

// Private functions ----------------------------------------------------------

// Populate the INPUT structure with the appropriate keyboard event
// parameters required by SendInput
bool FillKeyboardInput(ui::KeyboardCode key, INPUT* input, bool key_up) {
  memset(input, 0, sizeof(INPUT));
  input->type = INPUT_KEYBOARD;
  input->ki.wVk = ui::WindowsKeyCodeForKeyboardCode(key);
  input->ki.dwFlags = key_up ? KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP :
                               KEYEVENTF_EXTENDEDKEY;

  return true;
}

// Send a key event (up/down)
bool SendKeyEvent(ui::KeyboardCode key, bool up) {
  INPUT input = { 0 };

  if (!FillKeyboardInput(key, &input, up))
    return false;

  if (!::SendInput(1, &input, sizeof(INPUT)))
    return false;

  return true;
}

bool SendKeyPressImpl(ui::KeyboardCode key,
                      bool control, bool shift, bool alt,
                      Task* task) {
  scoped_refptr<InputDispatcher> dispatcher(
      task ? new InputDispatcher(task, WM_KEYUP) : NULL);

  // If a pop-up menu is open, it won't receive events sent using SendInput.
  // Check for a pop-up menu using its window class (#32768) and if one
  // exists, send the key event directly there.
  HWND popup_menu = ::FindWindow(L"#32768", 0);
  if (popup_menu != NULL && popup_menu == ::GetTopWindow(NULL)) {
    WPARAM w_param = ui::WindowsKeyCodeForKeyboardCode(key);
    LPARAM l_param = 0;
    ::SendMessage(popup_menu, WM_KEYDOWN, w_param, l_param);
    ::SendMessage(popup_menu, WM_KEYUP, w_param, l_param);

    if (dispatcher.get())
      dispatcher->AddRef();
    return true;
  }

  INPUT input[8] = { 0 };  // 8, assuming all the modifiers are activated.

  UINT i = 0;
  if (control) {
    if (!FillKeyboardInput(ui::VKEY_CONTROL, &input[i], false))
      return false;
    i++;
  }

  if (shift) {
    if (!FillKeyboardInput(ui::VKEY_SHIFT, &input[i], false))
      return false;
    i++;
  }

  if (alt) {
    if (!FillKeyboardInput(ui::VKEY_MENU, &input[i], false))
      return false;
    i++;
  }

  if (!FillKeyboardInput(key, &input[i], false))
    return false;
  i++;

  if (!FillKeyboardInput(key, &input[i], true))
    return false;
  i++;

  if (alt) {
    if (!FillKeyboardInput(ui::VKEY_MENU, &input[i], true))
      return false;
    i++;
  }

  if (shift) {
    if (!FillKeyboardInput(ui::VKEY_SHIFT, &input[i], true))
      return false;
    i++;
  }

  if (control) {
    if (!FillKeyboardInput(ui::VKEY_CONTROL, &input[i], true))
      return false;
    i++;
  }

  if (::SendInput(i, input, sizeof(INPUT)) != i)
    return false;

  if (dispatcher.get())
    dispatcher->AddRef();

  return true;
}

bool SendMouseMoveImpl(long x, long y, Task* task) {
  // First check if the mouse is already there.
  POINT current_pos;
  ::GetCursorPos(&current_pos);
  if (x == current_pos.x && y == current_pos.y) {
    if (task)
      MessageLoop::current()->PostTask(FROM_HERE, task);
    return true;
  }

  INPUT input = { 0 };

  int screen_width = ::GetSystemMetrics(SM_CXSCREEN) - 1;
  int screen_height  = ::GetSystemMetrics(SM_CYSCREEN) - 1;
  LONG pixel_x  = static_cast<LONG>(x * (65535.0f / screen_width));
  LONG pixel_y = static_cast<LONG>(y * (65535.0f / screen_height));

  input.type = INPUT_MOUSE;
  input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
  input.mi.dx = pixel_x;
  input.mi.dy = pixel_y;

  scoped_refptr<InputDispatcher> dispatcher(
      task ? new InputDispatcher(task, WM_MOUSEMOVE) : NULL);

  if (!::SendInput(1, &input, sizeof(INPUT)))
    return false;

  if (dispatcher.get())
    dispatcher->AddRef();

  return true;
}

bool SendMouseEventsImpl(MouseButton type, int state, Task* task) {
  DWORD down_flags = MOUSEEVENTF_ABSOLUTE;
  DWORD up_flags = MOUSEEVENTF_ABSOLUTE;
  UINT last_event;

  switch (type) {
    case LEFT:
      down_flags |= MOUSEEVENTF_LEFTDOWN;
      up_flags |= MOUSEEVENTF_LEFTUP;
      last_event = (state & UP) ? WM_LBUTTONUP : WM_LBUTTONDOWN;
      break;

    case MIDDLE:
      down_flags |= MOUSEEVENTF_MIDDLEDOWN;
      up_flags |= MOUSEEVENTF_MIDDLEUP;
      last_event = (state & UP) ? WM_MBUTTONUP : WM_MBUTTONDOWN;
      break;

    case RIGHT:
      down_flags |= MOUSEEVENTF_RIGHTDOWN;
      up_flags |= MOUSEEVENTF_RIGHTUP;
      last_event = (state & UP) ? WM_RBUTTONUP : WM_RBUTTONDOWN;
      break;

    default:
      NOTREACHED();
      return false;
  }

  scoped_refptr<InputDispatcher> dispatcher(
      task ? new InputDispatcher(task, last_event) : NULL);

  INPUT input = { 0 };
  input.type = INPUT_MOUSE;
  input.mi.dwFlags = down_flags;
  if ((state & DOWN) && !::SendInput(1, &input, sizeof(INPUT)))
    return false;

  input.mi.dwFlags = up_flags;
  if ((state & UP) && !::SendInput(1, &input, sizeof(INPUT)))
    return false;

  if (dispatcher.get())
    dispatcher->AddRef();

  return true;
}

}  // namespace

// public functions -----------------------------------------------------------

bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  DCHECK(!command);  // No command key on Windows
  return SendKeyPressImpl(key, control, shift, alt, NULL);
}

bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                Task* task) {
  DCHECK(!command);  // No command key on Windows
  return SendKeyPressImpl(key, control, shift, alt, task);
}

bool SendMouseMove(long x, long y) {
  return SendMouseMoveImpl(x, y, NULL);
}

bool SendMouseMoveNotifyWhenDone(long x, long y, Task* task) {
  return SendMouseMoveImpl(x, y, task);
}

bool SendMouseEvents(MouseButton type, int state) {
  return SendMouseEventsImpl(type, state, NULL);
}

bool SendMouseEventsNotifyWhenDone(MouseButton type, int state, Task* task) {
  return SendMouseEventsImpl(type, state, task);
}

bool SendMouseClick(MouseButton type) {
  return SendMouseEventsImpl(type, UP | DOWN, NULL);
}

void MoveMouseToCenterAndPress(views::View* view,
                               MouseButton button,
                               int state,
                               Task* task) {
  DCHECK(view);
  DCHECK(view->GetWidget());
  gfx::Point view_center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &view_center);
  SendMouseMove(view_center.x(), view_center.y());
  SendMouseEventsNotifyWhenDone(button, state, task);
}

}  // ui_controls
