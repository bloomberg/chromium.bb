// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/keyboard_driven_event_rewriter.h"

#include "chrome/browser/chromeos/events/event_rewriter.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "components/user_manager/user_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace chromeos {

namespace {

const int kModifierMask = ui::EF_SHIFT_DOWN;

// Returns true if and only if it is on login screen (i.e. user is not logged
// in) and the keyboard driven flag in the OEM manifest is on.
bool ShouldStripModifiersForArrowKeysAndEnter() {
  if (user_manager::UserManager::IsInitialized() &&
      !user_manager::UserManager::Get()->IsSessionStarted()) {
    return system::InputDeviceSettings::Get()
        ->ForceKeyboardDrivenUINavigation();
  }

  return false;
}

}  // namespace

KeyboardDrivenEventRewriter::KeyboardDrivenEventRewriter() {}

KeyboardDrivenEventRewriter::~KeyboardDrivenEventRewriter() {}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::RewriteForTesting(
    const ui::Event& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  return Rewrite(event, rewritten_event);
}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::RewriteEvent(
    const ui::Event& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  if (!ShouldStripModifiersForArrowKeysAndEnter())
    return ui::EVENT_REWRITE_CONTINUE;

  return Rewrite(event, rewritten_event);
}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    scoped_ptr<ui::Event>* new_event) {
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::Rewrite(
    const ui::Event& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  int flags = event.flags();
  if ((flags & kModifierMask) != kModifierMask)
    return ui::EVENT_REWRITE_CONTINUE;

  DCHECK(event.type() == ui::ET_KEY_PRESSED ||
         event.type() == ui::ET_KEY_RELEASED)
      << "Unexpected event type " << event.type();
  const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
  ui::KeyboardCode key_code = key_event.key_code();

  if (key_code != ui::VKEY_LEFT &&
      key_code != ui::VKEY_RIGHT &&
      key_code != ui::VKEY_UP &&
      key_code != ui::VKEY_DOWN &&
      key_code != ui::VKEY_RETURN &&
      key_code != ui::VKEY_F6) {
    return ui::EVENT_REWRITE_CONTINUE;
  }

  chromeos::EventRewriter::BuildRewrittenKeyEvent(
      key_event,
      key_event.key_code(),
      flags & ~(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN),
      rewritten_event);
  return ui::EVENT_REWRITE_REWRITTEN;
}

}  // namespace chromeos
