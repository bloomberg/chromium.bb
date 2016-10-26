// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/keyboard_driven_event_rewriter.h"

#include "chrome/browser/chromeos/events/event_rewriter.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace chromeos {

namespace {

const int kModifierMask = ui::EF_SHIFT_DOWN;

KeyboardDrivenEventRewriter* instance = nullptr;

// Returns true if and only if it is on login screen (i.e. user is not logged
// in) and the keyboard driven flag in the OEM manifest is on.
bool ShouldStripModifiersForArrowKeysAndEnter() {
  if (session_manager::SessionManager::Get() &&
      !session_manager::SessionManager::Get()->IsSessionStarted()) {
    return system::InputDeviceSettings::Get()
        ->ForceKeyboardDrivenUINavigation();
  }

  return false;
}

}  // namespace

// static
KeyboardDrivenEventRewriter* KeyboardDrivenEventRewriter::GetInstance() {
  DCHECK(instance);
  return instance;
}

KeyboardDrivenEventRewriter::KeyboardDrivenEventRewriter() {
  DCHECK(!instance);
  instance = this;
}

KeyboardDrivenEventRewriter::~KeyboardDrivenEventRewriter() {
  DCHECK_EQ(instance, this);
  instance = nullptr;
}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::RewriteForTesting(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  return Rewrite(event, rewritten_event);
}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (!ShouldStripModifiersForArrowKeysAndEnter())
    return ui::EVENT_REWRITE_CONTINUE;

  return Rewrite(event, rewritten_event);
}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus KeyboardDrivenEventRewriter::Rewrite(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  int flags = event.flags();
  if ((flags & kModifierMask) != kModifierMask)
    return ui::EVENT_REWRITE_CONTINUE;

  DCHECK(event.type() == ui::ET_KEY_PRESSED ||
         event.type() == ui::ET_KEY_RELEASED)
      << "Unexpected event type " << event.type();
  const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
  ui::KeyboardCode key_code = key_event.key_code();

  if (key_code != ui::VKEY_LEFT && key_code != ui::VKEY_RIGHT &&
      key_code != ui::VKEY_UP && key_code != ui::VKEY_DOWN &&
      key_code != ui::VKEY_RETURN && key_code != ui::VKEY_F6) {
    return ui::EVENT_REWRITE_CONTINUE;
  }

  chromeos::EventRewriter::MutableKeyState state = {
      flags & ~(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN),
      key_event.code(),
      key_event.GetDomKey(),
      key_event.key_code()};

  if (rewritten_to_tab_) {
    if (key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT ||
        key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN) {
      const ui::KeyEvent tab_event(ui::ET_KEY_PRESSED, ui::VKEY_TAB,
                                   ui::EF_NONE);
      state.code = tab_event.code();
      state.key = tab_event.GetDomKey();
      state.key_code = tab_event.key_code();
      if (key_code == ui::VKEY_LEFT || key_code == ui::VKEY_UP)
        state.flags |= ui::EF_SHIFT_DOWN;
    }
  }

  chromeos::EventRewriter::BuildRewrittenKeyEvent(key_event, state,
                                                  rewritten_event);
  return ui::EVENT_REWRITE_REWRITTEN;
}

}  // namespace chromeos
