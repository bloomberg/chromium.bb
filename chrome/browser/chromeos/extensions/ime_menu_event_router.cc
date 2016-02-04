// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/ime_menu_event_router.h"

#include "base/values.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"

namespace OnImeMenuActivationChanged =
    extensions::api::input_method_private::OnImeMenuActivationChanged;

namespace chromeos {

ExtensionImeMenuEventRouter::ExtensionImeMenuEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  input_method::InputMethodManager::Get()->AddImeMenuObserver(this);
}

ExtensionImeMenuEventRouter::~ExtensionImeMenuEventRouter() {
  input_method::InputMethodManager::Get()->RemoveImeMenuObserver(this);
}

void ExtensionImeMenuEventRouter::ImeMenuActivationChanged(bool activation) {
  extensions::EventRouter* router = extensions::EventRouter::Get(context_);

  if (!router->HasEventListener(OnImeMenuActivationChanged::kEventName))
    return;

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->AppendBoolean(activation);

  // The router will only send the event to extensions that are listening.
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::INPUT_METHOD_PRIVATE_ON_IME_MENU_ACTIVATION_CHANGED,
      OnImeMenuActivationChanged::kEventName, std::move(args)));
  event->restrict_to_browser_context = context_;
  router->BroadcastEvent(std::move(event));
}

}  // namespace chromeos
