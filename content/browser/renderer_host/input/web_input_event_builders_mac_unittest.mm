// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebKeyboardEvent;
using blink::WebInputEvent;
using content::WebKeyboardEventBuilder;

namespace {

struct KeyMappingEntry {
  int mac_key_code;
  unichar character;
  int windows_key_code;
  ui::DomCode dom_code;
  ui::DomKey dom_key;
};

struct ModifierKey {
  int mac_key_code;
  int left_or_right_mask;
  int non_specific_mask;
};

// Modifier keys, grouped into left/right pairs.
const ModifierKey kModifierKeys[] = {
    {56, 1 << 1, NSShiftKeyMask},      // Left Shift
    {60, 1 << 2, NSShiftKeyMask},      // Right Shift
    {55, 1 << 3, NSCommandKeyMask},    // Left Command
    {54, 1 << 4, NSCommandKeyMask},    // Right Command
    {58, 1 << 5, NSAlternateKeyMask},  // Left Alt
    {61, 1 << 6, NSAlternateKeyMask},  // Right Alt
    {59, 1 << 0, NSControlKeyMask},    // Left Control
    {62, 1 << 13, NSControlKeyMask},   // Right Control
};

NSEvent* BuildFakeKeyEvent(NSUInteger key_code,
                           unichar character,
                           NSUInteger modifier_flags,
                           NSEventType event_type) {
  NSString* string = [NSString stringWithCharacters:&character length:1];
  return [NSEvent keyEventWithType:event_type
                          location:NSZeroPoint
                     modifierFlags:modifier_flags
                         timestamp:0.0
                      windowNumber:0
                           context:nil
                        characters:string
       charactersIgnoringModifiers:string
                         isARepeat:NO
                           keyCode:key_code];
}

}  // namespace

// Test that arrow keys don't have numpad modifier set.
TEST(WebInputEventBuilderMacTest, ArrowKeyNumPad) {
  // Left
  NSEvent* mac_event = BuildFakeKeyEvent(0x7B, NSLeftArrowFunctionKey,
                                         NSNumericPadKeyMask, NSKeyDown);
  WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);
  EXPECT_EQ(ui::DomCode::ARROW_LEFT,
            static_cast<ui::DomCode>(web_event.domCode));
  EXPECT_EQ(ui::DomKey::ARROW_LEFT, web_event.domKey);

  // Right
  mac_event = BuildFakeKeyEvent(0x7C, NSRightArrowFunctionKey,
                                NSNumericPadKeyMask, NSKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);
  EXPECT_EQ(ui::DomCode::ARROW_RIGHT,
            static_cast<ui::DomCode>(web_event.domCode));
  EXPECT_EQ(ui::DomKey::ARROW_RIGHT, web_event.domKey);

  // Down
  mac_event = BuildFakeKeyEvent(0x7D, NSDownArrowFunctionKey,
                                NSNumericPadKeyMask, NSKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);
  EXPECT_EQ(ui::DomCode::ARROW_DOWN,
            static_cast<ui::DomCode>(web_event.domCode));
  EXPECT_EQ(ui::DomKey::ARROW_DOWN, web_event.domKey);

  // Up
  mac_event = BuildFakeKeyEvent(0x7E, NSUpArrowFunctionKey, NSNumericPadKeyMask,
                                NSKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);
  EXPECT_EQ(ui::DomCode::ARROW_UP, static_cast<ui::DomCode>(web_event.domCode));
  EXPECT_EQ(ui::DomKey::ARROW_UP, web_event.domKey);
}

// Test that control sequence generate the correct vkey code.
TEST(WebInputEventBuilderMacTest, ControlSequence) {
  // Ctrl-[ generates escape.
  NSEvent* mac_event =
      BuildFakeKeyEvent(0x21, 0x1b, NSControlKeyMask, NSKeyDown);
  WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(ui::VKEY_OEM_4, web_event.windowsKeyCode);
  EXPECT_EQ(ui::DomCode::BRACKET_LEFT,
            static_cast<ui::DomCode>(web_event.domCode));
  EXPECT_EQ(ui::DomKey::FromCharacter(0x1b), web_event.domKey);
}

// Test that numpad keys get mapped correctly.
TEST(WebInputEventBuilderMacTest, NumPadMapping) {
  KeyMappingEntry table[] = {
      {65, '.', ui::VKEY_DECIMAL, ui::DomCode::NUMPAD_DECIMAL,
       ui::DomKey::FromCharacter('.')},
      {67, '*', ui::VKEY_MULTIPLY, ui::DomCode::NUMPAD_MULTIPLY,
       ui::DomKey::FromCharacter('*')},
      {69, '+', ui::VKEY_ADD, ui::DomCode::NUMPAD_ADD,
       ui::DomKey::FromCharacter('+')},
      {71, NSClearLineFunctionKey, ui::VKEY_CLEAR, ui::DomCode::NUM_LOCK,
       ui::DomKey::CLEAR},
      {75, '/', ui::VKEY_DIVIDE, ui::DomCode::NUMPAD_DIVIDE,
       ui::DomKey::FromCharacter('/')},
      {76, 3, ui::VKEY_RETURN, ui::DomCode::NUMPAD_ENTER, ui::DomKey::ENTER},
      {78, '-', ui::VKEY_SUBTRACT, ui::DomCode::NUMPAD_SUBTRACT,
       ui::DomKey::FromCharacter('-')},
      {81, '=', ui::VKEY_OEM_PLUS, ui::DomCode::NUMPAD_EQUAL,
       ui::DomKey::FromCharacter('=')},
      {82, '0', ui::VKEY_0, ui::DomCode::NUMPAD0,
       ui::DomKey::FromCharacter('0')},
      {83, '1', ui::VKEY_1, ui::DomCode::NUMPAD1,
       ui::DomKey::FromCharacter('1')},
      {84, '2', ui::VKEY_2, ui::DomCode::NUMPAD2,
       ui::DomKey::FromCharacter('2')},
      {85, '3', ui::VKEY_3, ui::DomCode::NUMPAD3,
       ui::DomKey::FromCharacter('3')},
      {86, '4', ui::VKEY_4, ui::DomCode::NUMPAD4,
       ui::DomKey::FromCharacter('4')},
      {87, '5', ui::VKEY_5, ui::DomCode::NUMPAD5,
       ui::DomKey::FromCharacter('5')},
      {88, '6', ui::VKEY_6, ui::DomCode::NUMPAD6,
       ui::DomKey::FromCharacter('6')},
      {89, '7', ui::VKEY_7, ui::DomCode::NUMPAD7,
       ui::DomKey::FromCharacter('7')},
      {91, '8', ui::VKEY_8, ui::DomCode::NUMPAD8,
       ui::DomKey::FromCharacter('8')},
      {92, '9', ui::VKEY_9, ui::DomCode::NUMPAD9,
       ui::DomKey::FromCharacter('9')},
  };

  for (size_t i = 0; i < arraysize(table); ++i) {
    NSEvent* mac_event = BuildFakeKeyEvent(table[i].mac_key_code,
                                           table[i].character, 0, NSKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(table[i].windows_key_code, web_event.windowsKeyCode);
    EXPECT_EQ(table[i].dom_code, static_cast<ui::DomCode>(web_event.domCode));
    EXPECT_EQ(table[i].dom_key, web_event.domKey);
  }
}

// Test that left- and right-hand modifier keys are interpreted correctly when
// pressed simultaneously.
TEST(WebInputEventFactoryTestMac, SimultaneousModifierKeys) {
  for (size_t i = 0; i < arraysize(kModifierKeys) / 2; ++i) {
    const ModifierKey& left = kModifierKeys[2 * i];
    const ModifierKey& right = kModifierKeys[2 * i + 1];
    // Press the left key.
    NSEvent* mac_event = BuildFakeKeyEvent(
        left.mac_key_code, 0, left.left_or_right_mask | left.non_specific_mask,
        NSFlagsChanged);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::RawKeyDown, web_event.type);
    // Press the right key
    mac_event =
        BuildFakeKeyEvent(right.mac_key_code, 0,
                          left.left_or_right_mask | right.left_or_right_mask |
                              left.non_specific_mask,
                          NSFlagsChanged);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::RawKeyDown, web_event.type);
    // Release the right key
    mac_event = BuildFakeKeyEvent(
        right.mac_key_code, 0, left.left_or_right_mask | left.non_specific_mask,
        NSFlagsChanged);
    // Release the left key
    mac_event = BuildFakeKeyEvent(left.mac_key_code, 0, 0, NSFlagsChanged);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::KeyUp, web_event.type);
  }
}

// Test that individual modifier keys are still reported correctly, even if the
// undocumented left- or right-hand flags are not set.
TEST(WebInputEventBuilderMacTest, MissingUndocumentedModifierFlags) {
  for (size_t i = 0; i < arraysize(kModifierKeys); ++i) {
    const ModifierKey& key = kModifierKeys[i];
    NSEvent* mac_event = BuildFakeKeyEvent(
        key.mac_key_code, 0, key.non_specific_mask, NSFlagsChanged);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::RawKeyDown, web_event.type);
    mac_event = BuildFakeKeyEvent(key.mac_key_code, 0, 0, NSFlagsChanged);
    web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(WebInputEvent::KeyUp, web_event.type);
  }
}

// Test system key events recognition.
TEST(WebInputEventBuilderMacTest, SystemKeyEvents) {
  // Cmd + B should not be treated as system event.
  NSEvent* macEvent =
      BuildFakeKeyEvent(kVK_ANSI_B, 'B', NSCommandKeyMask, NSKeyDown);
  WebKeyboardEvent webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_FALSE(webEvent.isSystemKey);

  // Cmd + I should not be treated as system event.
  macEvent = BuildFakeKeyEvent(kVK_ANSI_I, 'I', NSCommandKeyMask, NSKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_FALSE(webEvent.isSystemKey);

  // Cmd + <some other modifier> + <B|I> should be treated as system event.
  macEvent = BuildFakeKeyEvent(kVK_ANSI_B, 'B',
                               NSCommandKeyMask | NSShiftKeyMask, NSKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.isSystemKey);
  macEvent = BuildFakeKeyEvent(kVK_ANSI_I, 'I',
                               NSCommandKeyMask | NSControlKeyMask, NSKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.isSystemKey);

  // Cmd + <any letter other then B and I> should be treated as system event.
  macEvent = BuildFakeKeyEvent(kVK_ANSI_S, 'S', NSCommandKeyMask, NSKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.isSystemKey);

  // Cmd + <some other modifier> + <any letter other then B and I> should be
  // treated as system event.
  macEvent = BuildFakeKeyEvent(kVK_ANSI_S, 'S',
                               NSCommandKeyMask | NSShiftKeyMask, NSKeyDown);
  webEvent = WebKeyboardEventBuilder::Build(macEvent);
  EXPECT_TRUE(webEvent.isSystemKey);
}
