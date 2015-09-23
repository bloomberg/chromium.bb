// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"

#import <Cocoa/Cocoa.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebKeyboardEvent;
using blink::WebInputEvent;
using content::WebKeyboardEventBuilder;

namespace {

struct KeyMappingEntry {
  int mac_key_code;
  unichar character;
  int windows_key_code;
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

  // Right
  mac_event = BuildFakeKeyEvent(0x7C, NSRightArrowFunctionKey,
                                NSNumericPadKeyMask, NSKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);

  // Down
  mac_event = BuildFakeKeyEvent(0x7D, NSDownArrowFunctionKey,
                                NSNumericPadKeyMask, NSKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);

  // Up
  mac_event = BuildFakeKeyEvent(0x7E, NSUpArrowFunctionKey, NSNumericPadKeyMask,
                                NSKeyDown);
  web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(0, web_event.modifiers);
}

// Test that control sequence generate the correct vkey code.
TEST(WebInputEventBuilderMacTest, ControlSequence) {
  // Ctrl-[ generates escape.
  NSEvent* mac_event =
      BuildFakeKeyEvent(0x21, 0x1b, NSControlKeyMask, NSKeyDown);
  WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
  EXPECT_EQ(ui::VKEY_OEM_4, web_event.windowsKeyCode);
}

// Test that numpad keys get mapped correctly.
TEST(WebInputEventBuilderMacTest, NumPadMapping) {
  KeyMappingEntry table[] = {
      {65, '.', ui::VKEY_DECIMAL},
      {67, '*', ui::VKEY_MULTIPLY},
      {69, '+', ui::VKEY_ADD},
      {71, NSClearLineFunctionKey, ui::VKEY_CLEAR},
      {75, '/', ui::VKEY_DIVIDE},
      {76, 3, ui::VKEY_RETURN},
      {78, '-', ui::VKEY_SUBTRACT},
      {81, '=', ui::VKEY_OEM_PLUS},
      {82, '0', ui::VKEY_0},
      {83, '1', ui::VKEY_1},
      {84, '2', ui::VKEY_2},
      {85, '3', ui::VKEY_3},
      {86, '4', ui::VKEY_4},
      {87, '5', ui::VKEY_5},
      {88, '6', ui::VKEY_6},
      {89, '7', ui::VKEY_7},
      {91, '8', ui::VKEY_8},
      {92, '9', ui::VKEY_9},
  };

  for (size_t i = 0; i < arraysize(table); ++i) {
    NSEvent* mac_event = BuildFakeKeyEvent(table[i].mac_key_code,
                                           table[i].character, 0, NSKeyDown);
    WebKeyboardEvent web_event = WebKeyboardEventBuilder::Build(mac_event);
    EXPECT_EQ(table[i].windows_key_code, web_event.windowsKeyCode);
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
