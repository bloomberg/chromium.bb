// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_controller_observer.h"

#include <memory>

#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "extensions/common/extension_messages.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"

namespace virtual_keyboard_private = extensions::api::virtual_keyboard_private;

ChromeKeyboardControllerObserver::ChromeKeyboardControllerObserver(
    content::BrowserContext* context,
    keyboard::KeyboardController* controller)
    : context_(context), controller_(controller) {
  controller_->AddObserver(this);
}

ChromeKeyboardControllerObserver::~ChromeKeyboardControllerObserver() {
  controller_->RemoveObserver(this);
}

void ChromeKeyboardControllerObserver::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& bounds) {
  extensions::EventRouter* router = extensions::EventRouter::Get(context_);

  if (!router->HasEventListener(
          virtual_keyboard_private::OnBoundsChanged::kEventName)) {
    return;
  }

  auto event_args = std::make_unique<base::ListValue>();
  auto new_bounds = std::make_unique<base::DictionaryValue>();
  new_bounds->SetInteger("left", bounds.x());
  new_bounds->SetInteger("top", bounds.y());
  new_bounds->SetInteger("width", bounds.width());
  new_bounds->SetInteger("height", bounds.height());
  event_args->Append(std::move(new_bounds));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_BOUNDS_CHANGED,
      virtual_keyboard_private::OnBoundsChanged::kEventName,
      std::move(event_args), context_);
  router->BroadcastEvent(std::move(event));
}

void ChromeKeyboardControllerObserver::OnKeyboardDisabled() {
  extensions::EventRouter* router = extensions::EventRouter::Get(context_);

  if (!router->HasEventListener(
          virtual_keyboard_private::OnKeyboardClosed::kEventName)) {
    return;
  }

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_KEYBOARD_CLOSED,
      virtual_keyboard_private::OnKeyboardClosed::kEventName,
      std::make_unique<base::ListValue>(), context_);
  router->BroadcastEvent(std::move(event));
}

void ChromeKeyboardControllerObserver::OnKeyboardConfigChanged() {
  extensions::VirtualKeyboardAPI* api =
      extensions::BrowserContextKeyedAPIFactory<
          extensions::VirtualKeyboardAPI>::Get(context_);
  api->delegate()->OnKeyboardConfigChanged();
}
