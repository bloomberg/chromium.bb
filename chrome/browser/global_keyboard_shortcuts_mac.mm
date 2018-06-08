// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/global_keyboard_shortcuts_mac.h"

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "build/buildflag.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#import "chrome/browser/ui/cocoa/nsmenuitem_additions.h"
#include "chrome/browser/ui/views_mode_controller.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

namespace {

// Returns the menu item associated with |key| in |menu|, or nil if not found.
NSMenuItem* FindMenuItem(NSEvent* key, NSMenu* menu) {
  NSMenuItem* result = nil;

  // Ask the delegate to update the menu first.
  if ([[menu delegate] respondsToSelector:@selector(menuNeedsUpdate:)])
    [[menu delegate] menuNeedsUpdate:menu];

  // Then update the menu, which will validate every user interface item.
  [menu update];

  // Then look for a matching NSMenuItem.
  for (NSMenuItem* item in [menu itemArray]) {
    NSMenu* submenu = [item submenu];
    if (submenu) {
      if (submenu != [NSApp servicesMenu])
        result = FindMenuItem(key, submenu);
    } else if ([item cr_firesForKeyEvent:key]) {
      result = item;
    }

    if (result)
      break;
  }

  return result;
}

int MenuCommandForKeyEvent(NSEvent* event) {
  if ([event type] != NSKeyDown)
    return -1;

  // Look in menu.
  NSMenuItem* item = FindMenuItem(event, [NSApp mainMenu]);

  if (!item)
    return -1;

  if ([item action] == @selector(commandDispatch:) && [item tag] > 0)
    return [item tag];

  // "Close window" doesn't use the |commandDispatch:| mechanism. Menu items
  // that do not correspond to IDC_ constants need no special treatment however,
  // as they can't be blacklisted in
  // |BrowserCommandController::IsReservedCommandOrKey()| anyhow.
  if ([item action] == @selector(performClose:))
    return IDC_CLOSE_WINDOW;

  // "Exit" doesn't use the |commandDispatch:| mechanism either.
  if ([item action] == @selector(terminate:))
    return IDC_EXIT;

  return -1;
}

// Returns a keyboard event character for the given |event|.  In most cases
// this returns the first character of [NSEvent charactersIgnoringModifiers],
// but when [NSEvent character] has different printable ascii character
// we may return the first character of [NSEvent characters] instead.
// (E.g. for dvorak-qwerty layout we want [NSEvent characters] rather than
// [charactersIgnoringModifiers] for command keys.  Similarly, on german
// layout we want '{' character rather than '8' for opt-8.)
unichar KeyCharacterForEvent(NSEvent* event) {
  NSString* eventString = [event charactersIgnoringModifiers];
  NSString* characters = [event characters];

  if ([eventString length] != 1)
    return 0;

  if ([characters length] != 1)
    return [eventString characterAtIndex:0];

  // Some characters are BiDi mirrored.  The mirroring is different
  // for different OS versions.  Instead of having a mirror table, map
  // raw/processed pairs to desired outputs.
  const struct {
    unichar rawChar;
    unichar unmodChar;
    unichar targetChar;
  } kCharMapping[] = {
      // OSX 10.8 mirrors certain chars.
      {'{', '}', '{'},
      {'}', '{', '}'},
      {'(', ')', '('},
      {')', '(', ')'},

      // OSX 10.9 has the unshifted raw char.
      {'[', '}', '{'},
      {']', '{', '}'},
      {'9', ')', '('},
      {'0', '(', ')'},

      // These are the same either way.
      {'[', ']', '['},
      {']', '[', ']'},
  };

  unichar noModifiersChar = [eventString characterAtIndex:0];
  unichar rawChar = [characters characterAtIndex:0];

  // Only apply transformation table for ascii.
  if (isascii(noModifiersChar) && isascii(rawChar)) {
    // Alphabetic characters aren't mirrored, go with the raw character.
    // [A previous partial comment said something about Dvorak?]
    if (isalpha(rawChar))
      return rawChar;

    // http://crbug.com/42517
    // http://crbug.com/315379
    // In RTL keyboard layouts, Cocoa mirrors characters in the string
    // returned by [event charactersIgnoringModifiers].  In this case, return
    // the raw (unmirrored) char.
    for (size_t i = 0; i < base::size(kCharMapping); ++i) {
      if (rawChar == kCharMapping[i].rawChar &&
          noModifiersChar == kCharMapping[i].unmodChar) {
        return kCharMapping[i].targetChar;
      }
    }

    // opt/alt modifier is set (e.g. on german layout we want '{' for opt-8).
    if ([event modifierFlags] & NSAlternateKeyMask)
      return rawChar;
  }

  return noModifiersChar;
}

bool MatchesEventForKeyboardShortcut(const KeyboardShortcutData& shortcut,
                                     bool command_key,
                                     bool shift_key,
                                     bool cntrl_key,
                                     bool opt_key,
                                     int vkey_code,
                                     unichar key_char) {
  // Expects that one of |key_char| or |vkey_code| is 0.
  DCHECK((shortcut.key_char == 0) ^ (shortcut.vkey_code == 0));
  if (shortcut.key_char) {
    // Shortcuts that have a |key_char| and have |opt_key| set are mistakes,
    // since |opt_key| is not checked when there is a |key_char|.
    DCHECK(!shortcut.opt_key);
    // The given shortcut key is to be matched by a keyboard character.
    // In this case we ignore shift and opt (alt) key modifiers, because
    // the character may be generated by a combination with those keys.
    if (shortcut.command_key == command_key &&
        shortcut.cntrl_key == cntrl_key && shortcut.key_char == key_char) {
      return true;
    }
  } else if (shortcut.vkey_code) {
    // The given shortcut key is to be matched by a virtual key code.
    if (shortcut.command_key == command_key &&
        shortcut.shift_key == shift_key &&
        shortcut.cntrl_key == cntrl_key &&
        shortcut.opt_key == opt_key &&
        shortcut.vkey_code == vkey_code)
      return true;
  } else {
    NOTREACHED();  // Shouldn't happen.
  }
  return false;
}

}  // namespace

const std::vector<KeyboardShortcutData>& GetShortcutsNotPresentInMainMenu() {
  // clang-format off
  static base::NoDestructor<std::vector<KeyboardShortcutData>> keys({
    //cmd   shift  cntrl  option vkeycode               char command
    //---   -----  -----  ------ --------               ---- -------
    // '{' / '}' characters should be matched earlier than virtual key codes
    // (so we can match alt-8 as '{' on German keyboards).
    {true,  false, false, false, 0,                     '}', IDC_SELECT_NEXT_TAB},
    {true,  false, false, false, 0,                     '{', IDC_SELECT_PREVIOUS_TAB},
    {true,  true,  false, false, kVK_ANSI_RightBracket, 0,   IDC_SELECT_NEXT_TAB},
    {true,  true,  false, false, kVK_ANSI_LeftBracket,  0,   IDC_SELECT_PREVIOUS_TAB},
    {false, false, true,  false, kVK_PageDown,          0,   IDC_SELECT_NEXT_TAB},
    {false, false, true,  false, kVK_Tab,               0,   IDC_SELECT_NEXT_TAB},
    {false, false, true,  false, kVK_PageUp,            0,   IDC_SELECT_PREVIOUS_TAB},
    {false, true,  true,  false, kVK_Tab,               0,   IDC_SELECT_PREVIOUS_TAB},

    //cmd  shift  cntrl  option vkeycode                char command
    //---  -----  -----  ------ --------                ---- -------
    // Cmd-0..8 select the nth tab, with cmd-9 being "last tab".
    {true, false, false, false, kVK_ANSI_1,             0,   IDC_SELECT_TAB_0},
    {true, false, false, false, kVK_ANSI_Keypad1,       0,   IDC_SELECT_TAB_0},
    {true, false, false, false, kVK_ANSI_2,             0,   IDC_SELECT_TAB_1},
    {true, false, false, false, kVK_ANSI_Keypad2,       0,   IDC_SELECT_TAB_1},
    {true, false, false, false, kVK_ANSI_3,             0,   IDC_SELECT_TAB_2},
    {true, false, false, false, kVK_ANSI_Keypad3,       0,   IDC_SELECT_TAB_2},
    {true, false, false, false, kVK_ANSI_4,             0,   IDC_SELECT_TAB_3},
    {true, false, false, false, kVK_ANSI_Keypad4,       0,   IDC_SELECT_TAB_3},
    {true, false, false, false, kVK_ANSI_5,             0,   IDC_SELECT_TAB_4},
    {true, false, false, false, kVK_ANSI_Keypad5,       0,   IDC_SELECT_TAB_4},
    {true, false, false, false, kVK_ANSI_6,             0,   IDC_SELECT_TAB_5},
    {true, false, false, false, kVK_ANSI_Keypad6,       0,   IDC_SELECT_TAB_5},
    {true, false, false, false, kVK_ANSI_7,             0,   IDC_SELECT_TAB_6},
    {true, false, false, false, kVK_ANSI_Keypad7,       0,   IDC_SELECT_TAB_6},
    {true, false, false, false, kVK_ANSI_8,             0,   IDC_SELECT_TAB_7},
    {true, false, false, false, kVK_ANSI_Keypad8,       0,   IDC_SELECT_TAB_7},
    {true, false, false, false, kVK_ANSI_9,             0,   IDC_SELECT_LAST_TAB},
    {true, false, false, false, kVK_ANSI_Keypad9,       0,   IDC_SELECT_LAST_TAB},
    {true, true,  false, false, kVK_ANSI_M,             0,   IDC_SHOW_AVATAR_MENU},
    {true, false, false, true,  kVK_ANSI_L,             0,   IDC_SHOW_DOWNLOADS},

    {true,  false, false, false, kVK_LeftArrow,         0,   IDC_BACK},
    {true,  false, false, false, kVK_RightArrow,        0,   IDC_FORWARD},
    {true,  true,  false, false, 0,                     'c', IDC_DEV_TOOLS_INSPECT},
  });
  // clang-format on
  return *keys;
}

int CommandForKeyEvent(NSEvent* event) {
  DCHECK(event);
  if ([event type] != NSKeyDown)
    return -1;

  int cmdNum = MenuCommandForKeyEvent(event);
  if (cmdNum != -1)
    return cmdNum;

  // Look in secondary keyboard shortcuts.
  NSUInteger modifiers = [event modifierFlags];
  const bool cmdKey = (modifiers & NSCommandKeyMask) != 0;
  const bool shiftKey = (modifiers & NSShiftKeyMask) != 0;
  const bool cntrlKey = (modifiers & NSControlKeyMask) != 0;
  const bool optKey = (modifiers & NSAlternateKeyMask) != 0;
  const int keyCode = [event keyCode];
  const unichar keyChar = KeyCharacterForEvent(event);

  // Scan through keycodes and see if it corresponds to one of the non-menu
  // shortcuts.
  for (const auto& shortcut : GetShortcutsNotPresentInMainMenu()) {
    if (MatchesEventForKeyboardShortcut(shortcut, cmdKey, shiftKey, cntrlKey,
                                        optKey, keyCode, keyChar)) {
      return shortcut.chrome_command;
    }
  }

  return -1;
}

// AppKit sends an event via performKeyEquivalent: if it has at least one of the
// command or control modifiers, and is an NSKeyDown event. CommandDispatcher
// supplements this by also sending event with the option modifier to
// performKeyEquivalent:.
bool EventUsesPerformKeyEquivalent(NSEvent* event) {
  NSUInteger modifiers = [event modifierFlags];
  if ((modifiers & (NSEventModifierFlagCommand | NSEventModifierFlagControl |
                    NSEventModifierFlagOption)) == 0) {
    return false;
  }
  return [event type] == NSKeyDown;
}

bool GetDefaultMacAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator) {
  // See if it corresponds to one of the non-menu shortcuts.
  for (const auto& shortcut : GetShortcutsNotPresentInMainMenu()) {
    // TODO(erikchen): If necessary, add support for shortcuts that do not have
    // a vkey_code. https://crbug.com/846893.
    if (shortcut.chrome_command == command_id && shortcut.vkey_code != 0) {
      NSUInteger cocoa_modifiers = 0;
      if (shortcut.command_key)
        cocoa_modifiers |= NSEventModifierFlagCommand;
      if (shortcut.shift_key)
        cocoa_modifiers |= NSEventModifierFlagShift;
      if (shortcut.cntrl_key)
        cocoa_modifiers |= NSEventModifierFlagControl;
      if (shortcut.opt_key)
        cocoa_modifiers |= NSEventModifierFlagOption;
      *accelerator = AcceleratorsCocoa::AcceleratorFromKeyCode(
          ui::KeyboardCodeFromKeyCode(shortcut.vkey_code), cocoa_modifiers);
      return true;
    }
  }

  // See if it corresponds to one of the default NSMenu keyEquivalents.
  const ui::Accelerator* default_nsmenu_equivalent =
      AcceleratorsCocoa::GetInstance()->GetAcceleratorForCommand(command_id);
  if (default_nsmenu_equivalent)
    *accelerator = *default_nsmenu_equivalent;
  return default_nsmenu_equivalent != nullptr;
}
