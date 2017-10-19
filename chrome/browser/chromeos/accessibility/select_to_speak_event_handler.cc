// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/select_to_speak_event_handler.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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

SelectToSpeakEventHandler::SelectToSpeakEventHandler() {
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->GetPrimaryRootWindow()->AddPreTargetHandler(this);
}

SelectToSpeakEventHandler::~SelectToSpeakEventHandler() {
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->GetPrimaryRootWindow()->RemovePreTargetHandler(this);
}

void SelectToSpeakEventHandler::CaptureForwardedEventsForTesting(
    SelectToSpeakForwardedEventDelegateForTesting* delegate) {
  event_delegate_for_testing_ = delegate;
}

void SelectToSpeakEventHandler::OnKeyEvent(ui::KeyEvent* event) {
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
      }
    }
  } else if (state_ == SEARCH_DOWN) {
    state_ = INACTIVE;
  }

  // Forward the key to the extension.
  extensions::ExtensionHost* host =
      GetAccessibilityExtensionHost(extension_misc::kSelectToSpeakExtensionId);
  if (host)
    ForwardKeyToExtension(*event, host);

  if (cancel_event)
    CancelEvent(event);
}

void SelectToSpeakEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(event);
  if (state_ == INACTIVE)
    return;

  if ((state_ == SEARCH_DOWN || state_ == MOUSE_RELEASED) &&
      event->type() == ui::ET_MOUSE_PRESSED) {
    state_ = CAPTURING;
  }

  if (state_ == WAIT_FOR_MOUSE_RELEASE &&
      event->type() == ui::ET_MOUSE_RELEASED) {
    CancelEvent(event);
    state_ = INACTIVE;
    return;
  }

  if (state_ != CAPTURING)
    return;

  if (event->type() == ui::ET_MOUSE_RELEASED)
    state_ = MOUSE_RELEASED;

  // If we're in the capturing state, forward the mouse event to
  // select-to-speak.
  if (event_delegate_for_testing_) {
    event_delegate_for_testing_->OnForwardEventToSelectToSpeakExtension(*event);
  } else {
    extensions::ExtensionHost* host = GetAccessibilityExtensionHost(
        extension_misc::kSelectToSpeakExtensionId);
    if (!host)
      return;

    content::RenderViewHost* rvh = host->render_view_host();
    if (!rvh)
      return;

    const blink::WebMouseEvent web_event =
        ui::MakeWebMouseEvent(*event, base::Bind(&GetScreenLocationFromEvent));
    rvh->GetWidget()->ForwardMouseEvent(web_event);
  }

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
