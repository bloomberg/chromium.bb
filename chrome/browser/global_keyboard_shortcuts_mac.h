// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_
#define CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_

#include "base/basictypes.h"

struct KeyboardShortcutData {
  bool command_key;
  bool shift_key;
  bool cntrl_key;
  bool opt_key;
  int vkey_code;  // Virtual Key code for the command.
  int chrome_command;  // The chrome command # to execute for this shortcut.
};

// Check if a given keycode + modifiers correspond to a given Chrome command.
// returns: Command number (as passed to Browser::ExecuteCommand) or -1 if there
// was no match.
//
// |performKeyEquivalent:| bubbles events up from the window to the views.
// If we let it bubble up to the Omnibox, then the Omnibox handles
// cmd-left/right just fine, but it swallows cmd-1 and doesn't give us a chance
// to intercept this. Hence, we need two types of keyboard shortcuts.
//
// This means cmd-left doesn't work if you hit cmd-l tab, which focusses
// something that's neither omnibox nor tab contents. This behavior is
// consistent with safari and camino, and I think it's the best we can do
// without rewriting event dispatching ( http://crbug.com/251069 ).

// This returns shortcuts that should work no matter what component of the
// browser is focused. They are executed by the window, before any view has the
// opportunity to override the shortcut (with the exception of the tab contents,
// which first checks if the current web page wants to handle the shortcut).
int CommandForWindowKeyboardShortcut(
    bool command_key, bool shift_key, bool cntrl_key, bool opt_key,
    int vkey_code);

// This returns shortcuts that should work only if the tab contents have focus
// (e.g. cmd-left, which shouldn't do history navigation if e.g. the omnibox has
// focus).
int CommandForBrowserKeyboardShortcut(
    bool command_key, bool shift_key, bool cntrl_key, bool opt_key,
    int vkey_code);

// For testing purposes.
const KeyboardShortcutData* GetWindowKeyboardShortcutTable(size_t* num_entries);
const KeyboardShortcutData*
    GetBrowserKeyboardShortcutTable(size_t* num_entries);

#endif  // #ifndef CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_
