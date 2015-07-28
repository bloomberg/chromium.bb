// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/spoken_feedback_event_rewriter.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "ui/events/event.h"

SpokenFeedbackEventRewriterDelegate::SpokenFeedbackEventRewriterDelegate()
    : is_sequencing_(false) {}

bool SpokenFeedbackEventRewriterDelegate::IsSpokenFeedbackEnabled() const {
  return true;
}

bool SpokenFeedbackEventRewriterDelegate::DispatchKeyEventToChromeVox(
    const ui::KeyEvent& key_event) {
  int kAllModifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN;

  if (!chromeos::AccessibilityManager::Get())
    return false;
  content::BrowserContext* context =
      chromeos::AccessibilityManager::Get()->profile();
  if (!context)
    return false;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          extension_misc::kChromeVoxExtensionId);
  if (!extension)
    return false;

  extensions::EventRouter* event_router = extensions::EventRouter::Get(context);
  if (!event_router->ExtensionHasEventListener(
          extension_misc::kChromeVoxExtensionId, "commands.onCommand"))
    return false;

  const extensions::CommandMap* commands =
      extensions::CommandsInfo::GetNamedCommands(extension);
  if (!commands)
    return false;

  int modifiers = key_event.flags() & kAllModifiers;
  std::string command_name;

  if (is_sequencing_) {
    is_sequencing_ = false;
    modifiers |= ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN;
    // This command name doesn't exist; used simply to cause ChromeVox to reset
    // its sequencing state in case command lookup fails.
    command_name = "sequenceCommand";
  }

  for (auto iter : *commands) {
    int command_modifiers =
        iter.second.accelerator().modifiers() & kAllModifiers;
    if (iter.second.accelerator().key_code() == key_event.key_code() &&
        command_modifiers == modifiers) {
      command_name = iter.second.command_name();
      if (!is_sequencing_ && command_name.substr(0, 14) == "sequencePrefix")
        is_sequencing_ = true;
    }
  }

  if (command_name.empty())
    return false;

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(new base::StringValue(command_name));

  scoped_ptr<extensions::Event> extension_event(
      new extensions::Event(extensions::events::COMMANDS_ON_COMMAND,
                            "commands.onCommand", args.Pass()));
  extension_event->restrict_to_browser_context = context;

  event_router->DispatchEventToExtension(extension_misc::kChromeVoxExtensionId,
                                         extension_event.Pass());

  return true;
}

SpokenFeedbackEventRewriter::SpokenFeedbackEventRewriter() {
  delegate_.reset(new SpokenFeedbackEventRewriterDelegate());
}

SpokenFeedbackEventRewriter::~SpokenFeedbackEventRewriter() {
}

void SpokenFeedbackEventRewriter::SetDelegateForTest(
    scoped_ptr<SpokenFeedbackEventRewriterDelegate> delegate) {
  delegate_ = delegate.Pass();
}

ui::EventRewriteStatus SpokenFeedbackEventRewriter::RewriteEvent(
    const ui::Event& event,
    scoped_ptr<ui::Event>* new_event) {
  if (!delegate_->IsSpokenFeedbackEnabled())
    return ui::EVENT_REWRITE_CONTINUE;

  if ((event.type() != ui::ET_KEY_PRESSED &&
       event.type() != ui::ET_KEY_RELEASED))
    return ui::EVENT_REWRITE_CONTINUE;

  const ui::KeyEvent key_event = static_cast<const ui::KeyEvent&>(event);
  if (event.type() == ui::ET_KEY_RELEASED) {
    std::vector<int>::iterator it =
        std::find(captured_key_codes_.begin(), captured_key_codes_.end(),
                  key_event.key_code());
    if (it != captured_key_codes_.end()) {
      captured_key_codes_.erase(it);
      return ui::EVENT_REWRITE_DISCARD;
    }
    return ui::EVENT_REWRITE_CONTINUE;
  }

  if (delegate_->DispatchKeyEventToChromeVox(key_event)) {
    captured_key_codes_.push_back(key_event.key_code());
    return ui::EVENT_REWRITE_DISCARD;
  }
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus SpokenFeedbackEventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    scoped_ptr<ui::Event>* new_event) {
  return ui::EVENT_REWRITE_CONTINUE;
}
