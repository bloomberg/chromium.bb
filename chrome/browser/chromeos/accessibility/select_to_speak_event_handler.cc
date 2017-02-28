// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/select_to_speak_event_handler.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

SelectToSpeakEventHandler::SelectToSpeakEventHandler() {
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

SelectToSpeakEventHandler::~SelectToSpeakEventHandler() {
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void SelectToSpeakEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(event);

  // We can only call TtsController on the UI thread, make sure we
  // don't ever try to run this code on some other thread.
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ui::KeyboardCode key_code = event->key_code();

  // Stop speech when the user taps and releases Control or Search
  // without pressing any other keys along the way.
  if (state_ != MOUSE_RELEASED && event->type() == ui::ET_KEY_RELEASED &&
      (key_code == ui::VKEY_CONTROL || key_code == ui::VKEY_LWIN) &&
      keys_pressed_together_.find(key_code) != keys_pressed_together_.end() &&
      keys_pressed_together_.size() == 1) {
    TtsController::GetInstance()->Stop();
  }

  // Update keys_currently_down_ and keys_pressed_together_.
  if (event->type() == ui::ET_KEY_PRESSED) {
    keys_currently_down_.insert(key_code);
    keys_pressed_together_.insert(key_code);
  } else if (event->type() == ui::ET_KEY_RELEASED) {
    keys_currently_down_.erase(key_code);
    if (keys_currently_down_.empty())
      keys_pressed_together_.clear();
  }

  // Update the state when pressing and releasing the Search key (VKEY_LWIN).
  if (key_code == ui::VKEY_LWIN) {
    if (event->type() == ui::ET_KEY_PRESSED && state_ == INACTIVE) {
      state_ = SEARCH_DOWN;
    } else if (event->type() == ui::ET_KEY_RELEASED) {
      if (state_ == CAPTURING) {
        SendCancelAXEvent();
        CancelEvent(event);
        state_ = WAIT_FOR_MOUSE_RELEASE;
      } else if (state_ == MOUSE_RELEASED) {
        CancelEvent(event);
        state_ = INACTIVE;
      }
    }
  } else if (state_ == SEARCH_DOWN) {
    state_ = INACTIVE;
  }
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

  // If we're in the capturing state, send accessibility events to
  // the Select-to-speak extension based on the mouse event.
  // First, figure out what event to send.
  ui::AXEvent ax_event = ui::AX_EVENT_NONE;
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      ax_event = ui::AX_EVENT_MOUSE_PRESSED;
      break;
    case ui::ET_MOUSE_DRAGGED:
      ax_event = ui::AX_EVENT_MOUSE_DRAGGED;
      break;
    case ui::ET_MOUSE_RELEASED:
      state_ = MOUSE_RELEASED;
      ax_event = ui::AX_EVENT_MOUSE_RELEASED;
      break;
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      ax_event = ui::AX_EVENT_MOUSE_MOVED;
      break;
    default:
      return;
  }

  CancelEvent(event);

  // Find the View to post the accessibility event on.
  aura::Window* event_target = static_cast<aura::Window*>(event->target());
  aura::Window* hit_window = event_target;
  if (!hit_window)
    return;

  views::Widget* hit_widget = views::Widget::GetWidgetForNativeView(hit_window);
  while (!hit_widget) {
    hit_window = hit_window->parent();
    if (!hit_window)
      break;

    hit_widget = views::Widget::GetWidgetForNativeView(hit_window);
  }

  if (!hit_window || !hit_widget)
    return;

  gfx::Point window_location = event->location();
  aura::Window::ConvertPointToTarget(event_target, hit_window,
                                     &window_location);

  views::View* root_view = hit_widget->GetRootView();
  views::View* hit_view = root_view->GetEventHandlerForPoint(window_location);

  if (hit_view) {
    // Send the accessibility event, then save the view so we can post the
    // cancel event on the same view if possible.
    hit_view->NotifyAccessibilityEvent(ax_event, true);
    if (!last_view_storage_id_) {
      last_view_storage_id_ =
          views::ViewStorage::GetInstance()->CreateStorageID();
    }
    views::ViewStorage::GetInstance()->RemoveView(last_view_storage_id_);
    views::ViewStorage::GetInstance()->StoreView(last_view_storage_id_,
                                                 hit_view);
  }
}

void SelectToSpeakEventHandler::CancelEvent(ui::Event* event) {
  DCHECK(event);
  if (event->cancelable()) {
    event->SetHandled();
    event->StopPropagation();
  }
}

void SelectToSpeakEventHandler::SendCancelAXEvent() {
  // If the user releases Search while the button is still down, cancel
  // the Select-to-speak gesture. Try to post it on the same View that
  // was last targeted, but if that's impossible, post it on the desktop.
  views::View* last_view =
      last_view_storage_id_
          ? views::ViewStorage::GetInstance()->RetrieveView(
                last_view_storage_id_)
          : nullptr;
  if (last_view) {
    last_view->NotifyAccessibilityEvent(ui::AX_EVENT_MOUSE_CANCELED, true);
  } else {
    AutomationManagerAura::GetInstance()->HandleEvent(
        nullptr, nullptr, ui::AX_EVENT_MOUSE_CANCELED);
  }
}

}  // namespace chromeos
