// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_
#define CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_

#include <stddef.h>

#include <vector>

#if defined(__OBJC__)
@class NSEvent;
#else   // __OBJC__
class NSEvent;
#endif  // __OBJC__

namespace ui {
class Accelerator;
}

struct KeyboardShortcutData {
  bool command_key;
  bool shift_key;
  bool cntrl_key;
  bool opt_key;
  // Either one of vkey_code or key_char must be specified.  For keys
  // whose virtual key code is hardware-dependent (kVK_ANSI_*) key_char
  // should be specified instead.
  // Set 0 for the one you do not want to specify.
  int vkey_code;  // Virtual Key code for the command.

  // Key event characters for the command as reported by
  // [NSEvent charactersIgnoringModifiers].
  // This should be a unichar, but the type is defined in
  // Foundation.framework/.../NSString.h, which is an ObjC header. This header
  // is included in non-ObjC translation units, so we cannot rely on that
  // include.
  unsigned short key_char;

  int chrome_command;  // The chrome command # to execute for this shortcut.
};

// macOS applications are supposed to put all keyEquivalents [hotkeys] in the
// menu bar. For legacy reasons, Chrome does not. There are around 30 hotkeys
// that are explicitly coded to virtual keycodes. This has the following
// downsides:
//  * There is no way for the user to configure or disable these keyEquivalents.
//  * This can cause keyEquivalent conflicts for non-US keyboard layouts with
//    different default keyEquivalents, see https://crbug.com/841299.
//
// This function first searches the menu bar for a matching keyEquivalent. If
// nothing is found, then it searches through the explicitly coded virtual
// keycodes not present in the NSMenu.
//
// Note: AppKit exposes symbolic hotkeys [e.g. cmd + `] not present in the
// NSMenu as well. The user can remap these to conflict with Chrome hotkeys.
// This function will return the Chrome hotkey, regardless of whether there's a
// conflicting symbolic hotkey.
int CommandForKeyEvent(NSEvent* event);

// Whether the event goes through the performKeyEquivalent: path and is handled
// by CommandDispatcher.
bool EventUsesPerformKeyEquivalent(NSEvent* event);

// On macOS, most accelerators are defined in MainMenu.xib and are user
// configurable. Furthermore, their values and enabled state depends on the key
// window. Views code relies on a static mapping that is not dependent on the
// key window. Thus, we provide the default Mac accelerator for each CommandId,
// which is static. This may be inaccurate, but is at least sufficiently well
// defined for Views to use.
bool GetDefaultMacAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator);

// For testing purposes.
const std::vector<KeyboardShortcutData>& GetShortcutsNotPresentInMainMenu();

#endif  // #ifndef CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_
