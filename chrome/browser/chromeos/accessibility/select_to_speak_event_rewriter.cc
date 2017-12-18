// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/select_to_speak_event_rewriter.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/display/display.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"

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

SelectToSpeakEventRewriter::SelectToSpeakEventRewriter(
    aura::Window* root_window)
    : root_window_(root_window) {
  DCHECK(root_window_);
}

SelectToSpeakEventRewriter::~SelectToSpeakEventRewriter() = default;

void SelectToSpeakEventRewriter::CaptureForwardedEventsForTesting(
    SelectToSpeakEventDelegateForTesting* delegate) {
  event_delegate_for_testing_ = delegate;
}

bool SelectToSpeakEventRewriter::IsSelectToSpeakEnabled() {
  if (event_delegate_for_testing_)
    return true;
  return chromeos::AccessibilityManager::Get()->IsSelectToSpeakEnabled();
}

bool SelectToSpeakEventRewriter::OnKeyEvent(const ui::KeyEvent* event) {
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
      if (state_ == CAPTURING) {
        cancel_event = true;
        state_ = WAIT_FOR_MOUSE_RELEASE;
      } else if (state_ == MOUSE_RELEASED) {
        cancel_event = true;
        state_ = INACTIVE;
      } else if (state_ == SEARCH_DOWN) {
        // They just tapped the search key without clicking the mouse.
        // Don't cancel this event -- the search key may still be used
        // by another part of Chrome, and we didn't use it here.
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

  return cancel_event;
}

bool SelectToSpeakEventRewriter::OnMouseEvent(const ui::MouseEvent* event) {
  DCHECK(event);
  if (state_ == INACTIVE)
    return false;

  if ((state_ == SEARCH_DOWN || state_ == MOUSE_RELEASED) &&
      event->type() == ui::ET_MOUSE_PRESSED) {
    state_ = CAPTURING;
  }

  if (state_ == WAIT_FOR_MOUSE_RELEASE &&
      event->type() == ui::ET_MOUSE_RELEASED) {
    state_ = INACTIVE;
    return false;
  }

  if (state_ != CAPTURING)
    return false;

  if (event->type() == ui::ET_MOUSE_RELEASED)
    state_ = MOUSE_RELEASED;

  ui::MouseEvent mutable_event(*event);
  ConvertMouseEventToDIPs(&mutable_event);

  // If we're in the capturing state, forward the mouse event to
  // select-to-speak.
  if (event_delegate_for_testing_) {
    event_delegate_for_testing_->OnForwardEventToSelectToSpeakExtension(
        mutable_event);
  } else {
    extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
        extension_misc::kSelectToSpeakExtensionId);
    if (!host)
      return false;

    content::RenderViewHost* rvh = host->render_view_host();
    if (!rvh)
      return false;

    const blink::WebMouseEvent web_event = ui::MakeWebMouseEvent(
        mutable_event, base::Bind(&GetScreenLocationFromEvent));
    rvh->GetWidget()->ForwardMouseEvent(web_event);
  }

  return true;
}

void SelectToSpeakEventRewriter::ConvertMouseEventToDIPs(
    ui::MouseEvent* mouse_event) {
  // The event is in Pixels, and needs to be scaled to DIPs.
  gfx::Point location = mouse_event->location();
  gfx::Point root_location = mouse_event->root_location();
  root_window_->GetHost()->ConvertPixelsToDIP(&location);
  root_window_->GetHost()->ConvertPixelsToDIP(&root_location);
  mouse_event->set_location(location);
  mouse_event->set_root_location(root_location);
}

ui::EventRewriteStatus SelectToSpeakEventRewriter::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* new_event) {
  if (!IsSelectToSpeakEnabled())
    return ui::EVENT_REWRITE_CONTINUE;

  if (event.type() == ui::ET_KEY_PRESSED ||
      event.type() == ui::ET_KEY_RELEASED) {
    const ui::KeyEvent key_event = static_cast<const ui::KeyEvent&>(event);
    if (OnKeyEvent(&key_event))
      return ui::EVENT_REWRITE_DISCARD;
  }

  if (event.type() == ui::ET_MOUSE_PRESSED ||
      event.type() == ui::ET_MOUSE_DRAGGED ||
      event.type() == ui::ET_MOUSE_RELEASED ||
      event.type() == ui::ET_MOUSE_MOVED) {
    const ui::MouseEvent mouse_event =
        static_cast<const ui::MouseEvent&>(event);
    if (OnMouseEvent(&mouse_event))
      return ui::EVENT_REWRITE_DISCARD;
  }

  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus SelectToSpeakEventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  return ui::EVENT_REWRITE_CONTINUE;
}
