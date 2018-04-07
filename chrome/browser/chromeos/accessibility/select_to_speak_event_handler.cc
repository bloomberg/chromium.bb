// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/select_to_speak_event_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/display/display.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"

namespace chromeos {

namespace {

gfx::PointF GetScreenLocationFromEvent(const ui::LocatedEvent& event) {
  aura::Window* root =
      static_cast<aura::Window*>(event.target())->GetRootWindow();
  aura::client::ScreenPositionClient* spc =
      aura::client::GetScreenPositionClient(root);
  if (!spc)
    return event.root_location_f();

  gfx::PointF screen_location(event.root_location_f());
  spc->ConvertPointToScreen(root, &screen_location);
  return screen_location;
}
}  // namespace

const ui::KeyboardCode kSpeakSelectionKey = ui::VKEY_S;

SelectToSpeakEventHandler::SelectToSpeakEventHandler() {
  // Add this to the root level EventTarget so that it is called first.
  // TODO(katie): instead of using the root level EventTarget, just add this
  // handler to ash::Shell::Get()->GetPrimaryRootWindow().
  if (aura::Env::GetInstanceDontCreate())
    aura::Env::GetInstanceDontCreate()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kAccessibility);
}

SelectToSpeakEventHandler::~SelectToSpeakEventHandler() {
  if (aura::Env::GetInstanceDontCreate())
    aura::Env::GetInstanceDontCreate()->RemovePreTargetHandler(this);
}

void SelectToSpeakEventHandler::CaptureForwardedEventsForTesting(
    SelectToSpeakEventDelegateForTesting* delegate) {
  event_delegate_for_testing_ = delegate;
}

bool SelectToSpeakEventHandler::IsSelectToSpeakEnabled() {
  if (event_delegate_for_testing_)
    return true;
  return chromeos::AccessibilityManager::Get()->IsSelectToSpeakEnabled();
}

void SelectToSpeakEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (!IsSelectToSpeakEnabled())
    return;

  DCHECK(event);

  // We can only call TtsController on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ui::KeyboardCode key_code = event->key_code();
  bool cancel_event = false;

  // Update the state when pressing and releasing the Search key (VKEY_LWIN).
  if (key_code == ui::VKEY_LWIN) {
    if (event->type() == ui::ET_KEY_PRESSED && state_ == INACTIVE) {
      state_ = SEARCH_DOWN;
    } else if (event->type() == ui::ET_KEY_RELEASED) {
      if (state_ == CAPTURING_MOUSE) {
        cancel_event = true;
        state_ = WAIT_FOR_MOUSE_RELEASE;
      } else if (state_ == MOUSE_RELEASED) {
        cancel_event = true;
        state_ = INACTIVE;
      } else if (state_ == CAPTURING_SPEAK_SELECTION_KEY) {
        cancel_event = true;
        state_ = WAIT_FOR_SPEAK_SELECTION_KEY_RELEASE;
      } else if (state_ == SPEAK_SELECTION_KEY_RELEASED) {
        cancel_event = true;
        state_ = INACTIVE;
      } else if (state_ == SEARCH_DOWN) {
        // They just tapped the search key without clicking the mouse.
        // Don't cancel this event -- the search key may still be used
        // by another part of Chrome, and we didn't use it here.
        state_ = INACTIVE;
      }
    }
  } else if (key_code == kSpeakSelectionKey) {
    if (event->type() == ui::ET_KEY_PRESSED &&
        (state_ == SEARCH_DOWN || state_ == SPEAK_SELECTION_KEY_RELEASED)) {
      // They pressed the S key while search was down.
      // It's possible to press the selection key multiple times to read
      // the same region over and over, so state S_RELEASED can become state
      // CAPTURING_SPEAK_SELECTION_KEY if the search key is not lifted.
      cancel_event = true;
      state_ = CAPTURING_SPEAK_SELECTION_KEY;
    } else if (event->type() == ui::ET_KEY_RELEASED) {
      if (state_ == CAPTURING_SPEAK_SELECTION_KEY) {
        // They released the speak selection key while it was being captured.
        cancel_event = true;
        state_ = SPEAK_SELECTION_KEY_RELEASED;
      } else if (state_ == WAIT_FOR_SPEAK_SELECTION_KEY_RELEASE) {
        // They have already released the search key
        cancel_event = true;
        state_ = INACTIVE;
      }
    }
  } else if (state_ == SEARCH_DOWN) {
    state_ = INACTIVE;
  }

  // Forward the key to the extension.
  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kSelectToSpeakExtensionId);
  if (host)
    chromeos::ForwardKeyToExtension(*event, host);

  if (cancel_event)
    CancelEvent(event);
}

void SelectToSpeakEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  if (!IsSelectToSpeakEnabled())
    return;

  DCHECK(event);
  if (state_ == INACTIVE)
    return;

  if ((state_ == SEARCH_DOWN || state_ == MOUSE_RELEASED) &&
      event->type() == ui::ET_MOUSE_PRESSED) {
    state_ = CAPTURING_MOUSE;
  }

  if (state_ == WAIT_FOR_MOUSE_RELEASE &&
      event->type() == ui::ET_MOUSE_RELEASED) {
    state_ = INACTIVE;
    return;
  }

  if (state_ != CAPTURING_MOUSE)
    return;

  if (event->type() == ui::ET_MOUSE_RELEASED)
    state_ = MOUSE_RELEASED;

  ui::MouseEvent mutable_event(*event);

  // If we're in the capturing mouse state, forward the mouse event to
  // select-to-speak.
  if (event_delegate_for_testing_) {
    event_delegate_for_testing_->OnForwardEventToSelectToSpeakExtension(
        mutable_event);
  } else {
    extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
        extension_misc::kSelectToSpeakExtensionId);
    if (!host)
      return;

    content::RenderViewHost* rvh = host->render_view_host();
    if (!rvh)
      return;

    const blink::WebMouseEvent web_event = ui::MakeWebMouseEvent(
        mutable_event, base::Bind(&GetScreenLocationFromEvent));
    rvh->GetWidget()->ForwardMouseEvent(web_event);
  }

  if (event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSE_RELEASED)
    CancelEvent(event);
}

void SelectToSpeakEventHandler::CancelEvent(ui::Event* event) {
  DCHECK(event);
  if (event->cancelable()) {
    event->SetHandled();
    event->StopPropagation();
  }
}

}  // namespace chromeos
