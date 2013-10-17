// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/global_shortcut_listener_win.h"

#include "base/win/win_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"

using content::BrowserThread;

namespace {

static base::LazyInstance<extensions::GlobalShortcutListenerWin> instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

// static
GlobalShortcutListener* GlobalShortcutListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return instance.Pointer();
}

GlobalShortcutListenerWin::GlobalShortcutListenerWin()
    : is_listening_(false) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GlobalShortcutListenerWin::~GlobalShortcutListenerWin() {
  if (is_listening_)
    StopListening();
}

void GlobalShortcutListenerWin::StartListening() {
  DCHECK(!is_listening_);  // Don't start twice.
  DCHECK(!hotkey_ids_.empty());  // Also don't start if no hotkey is registered.
  gfx::SingletonHwnd::GetInstance()->AddObserver(this);
  is_listening_ = true;
}

void GlobalShortcutListenerWin::StopListening() {
  DCHECK(is_listening_);  // No point if we are not already listening.
  DCHECK(hotkey_ids_.empty());  // Make sure the map is clean before ending.
  gfx::SingletonHwnd::GetInstance()->RemoveObserver(this);
  is_listening_ = false;
}

void GlobalShortcutListenerWin::OnWndProc(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
  if (message != WM_HOTKEY)
    return;

  int key_code = HIWORD(lparam);
  int modifiers = 0;
  modifiers |= (LOWORD(lparam) & MOD_SHIFT) ? ui::EF_SHIFT_DOWN : 0;
  modifiers |= (LOWORD(lparam) & MOD_ALT) ? ui::EF_ALT_DOWN : 0;
  modifiers |= (LOWORD(lparam) & MOD_CONTROL) ? ui::EF_CONTROL_DOWN : 0;
  ui::Accelerator accelerator(
      ui::KeyboardCodeForWindowsKeyCode(key_code), modifiers);

  instance.Get().NotifyKeyPressed(accelerator);
}

void GlobalShortcutListenerWin::RegisterAccelerator(
    const ui::Accelerator& accelerator,
    GlobalShortcutListener::Observer* observer) {
  int modifiers = 0;
  modifiers |= accelerator.IsShiftDown() ? MOD_SHIFT : 0;
  modifiers |= accelerator.IsCtrlDown() ? MOD_CONTROL : 0;
  modifiers |= accelerator.IsAltDown() ? MOD_ALT : 0;
  static int hotkey_id = 0;
  bool success = !!RegisterHotKey(
      gfx::SingletonHwnd::GetInstance()->hwnd(),
      hotkey_id,
      modifiers,
      accelerator.key_code());

  if (!success) {
    // Most likely error: 1409 (Hotkey already registered).
    LOG(ERROR) << "RegisterHotKey failed, error: " << GetLastError();
    return;
  }

  hotkey_ids_[accelerator] = hotkey_id++;
  GlobalShortcutListener::RegisterAccelerator(accelerator, observer);
}

void GlobalShortcutListenerWin::UnregisterAccelerator(
    const ui::Accelerator& accelerator,
    GlobalShortcutListener::Observer* observer) {
  // We may get asked to unregister something that we couldn't register (for
  // example if the shortcut was already taken by another app), so we
  // need to handle that gracefully.
  HotkeyIdMap::iterator it = hotkey_ids_.find(accelerator);
  if (it == hotkey_ids_.end())
    return;

  UnregisterHotKey(gfx::SingletonHwnd::GetInstance()->hwnd(), it->second);
  hotkey_ids_.erase(it);

  GlobalShortcutListener::UnregisterAccelerator(accelerator, observer);
}

}  // namespace extensions
