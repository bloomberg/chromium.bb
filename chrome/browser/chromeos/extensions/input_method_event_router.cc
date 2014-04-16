// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "input_method_event_router.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace chromeos {

ExtensionInputMethodEventRouter::ExtensionInputMethodEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  input_method::InputMethodManager::Get()->AddObserver(this);
}

ExtensionInputMethodEventRouter::~ExtensionInputMethodEventRouter() {
  input_method::InputMethodManager::Get()->RemoveObserver(this);
}

void ExtensionInputMethodEventRouter::InputMethodChanged(
    input_method::InputMethodManager *manager,
    bool show_message) {
  extensions::EventRouter* router = extensions::EventRouter::Get(context_);

  if (!router->HasEventListener(
          extensions::InputMethodAPI::kOnInputMethodChanged)) {
    return;
  }

  scoped_ptr<base::ListValue> args(new base::ListValue());
  base::StringValue *input_method_name = new base::StringValue(
      extensions::InputMethodAPI::GetInputMethodForXkb(
          manager->GetCurrentInputMethod().id()));
  args->Append(input_method_name);

  // The router will only send the event to extensions that are listening.
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::InputMethodAPI::kOnInputMethodChanged, args.Pass()));
  event->restrict_to_browser_context = context_;
  router->BroadcastEvent(event.Pass());
}

}  // namespace chromeos
