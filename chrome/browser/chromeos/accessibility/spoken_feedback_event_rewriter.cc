// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/spoken_feedback_event_rewriter.h"

#include <string>
#include <utility>

#include "ash/shell.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "ui/aura/window_tree_host.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"

SpokenFeedbackEventRewriterDelegate::SpokenFeedbackEventRewriterDelegate() {}

bool SpokenFeedbackEventRewriterDelegate::IsSpokenFeedbackEnabled() const {
  return chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
}

bool SpokenFeedbackEventRewriterDelegate::DispatchKeyEventToChromeVox(
    const ui::KeyEvent& key_event,
    bool capture) {

  // Always capture the Search key.
  capture |= key_event.IsCommandDown();

  // Don't capture tab as it gets consumed by Blink so never comes back
  // unhandled. In third_party/WebKit/Source/core/input/EventHandler.cpp, a
  // default tab handler consumes tab even when no focusable nodes are found; it
  // sets focus to Chrome and eats the event.
  if (key_event.GetDomKey() == ui::DomKey::TAB)
    capture = false;

  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kChromeVoxExtensionId);
  if (!host)
    return false;

  // Listen for any unhandled keyboard events from ChromeVox's background page
  // when capturing keys to reinject.
  if (capture)
    host->host_contents()->SetDelegate(this);
  else
    host->host_contents()->SetDelegate(nullptr);

  // Forward all key events to ChromeVox's background page.
  chromeos::ForwardKeyToExtension(key_event, host);

  if ((key_event.key_code() >= ui::VKEY_F1) &&
      (key_event.key_code() <= ui::VKEY_F12))
    return false;

  return capture;
}

void SpokenFeedbackEventRewriterDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  ui::KeyEvent key_event(*static_cast<ui::KeyEvent*>(event.os_event));

  if ((key_event.key_code() >= ui::VKEY_F1) &&
      (key_event.key_code() <= ui::VKEY_F12))
    return;

  ui::EventProcessor* processor =
      ash::Shell::GetPrimaryRootWindow()->GetHost()->event_processor();

  if (processor->OnEventFromSource(&key_event).dispatcher_destroyed) {
    VLOG(0) << "Undispatched key " << key_event.key_code()
            << " due to destroyed dispatcher.";
  }
}

SpokenFeedbackEventRewriter::SpokenFeedbackEventRewriter() {
  delegate_.reset(new SpokenFeedbackEventRewriterDelegate());
}

SpokenFeedbackEventRewriter::~SpokenFeedbackEventRewriter() {
}

void SpokenFeedbackEventRewriter::SetDelegateForTest(
    std::unique_ptr<SpokenFeedbackEventRewriterDelegate> delegate) {
  delegate_ = std::move(delegate);
}

ui::EventRewriteStatus SpokenFeedbackEventRewriter::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* new_event) {
  if (!delegate_->IsSpokenFeedbackEnabled())
    return ui::EVENT_REWRITE_CONTINUE;

  if ((event.type() != ui::ET_KEY_PRESSED &&
       event.type() != ui::ET_KEY_RELEASED))
    return ui::EVENT_REWRITE_CONTINUE;

  std::string extension_id =
      chromeos::AccessibilityManager::Get()->keyboard_listener_extension_id();
  if (extension_id.empty())
    return ui::EVENT_REWRITE_CONTINUE;

  bool capture =
      chromeos::AccessibilityManager::Get()->keyboard_listener_capture();
  const ui::KeyEvent key_event = static_cast<const ui::KeyEvent&>(event);
  if (delegate_->DispatchKeyEventToChromeVox(key_event, capture))
    return ui::EVENT_REWRITE_DISCARD;
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus SpokenFeedbackEventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  return ui::EVENT_REWRITE_CONTINUE;
}
