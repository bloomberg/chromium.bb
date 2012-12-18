// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "input_method_event_router.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/web_socket_proxy_controller.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

ExtensionInputMethodEventRouter::ExtensionInputMethodEventRouter() {
  input_method::GetInputMethodManager()->AddObserver(this);
}

ExtensionInputMethodEventRouter::~ExtensionInputMethodEventRouter() {
  input_method::GetInputMethodManager()->RemoveObserver(this);
}

void ExtensionInputMethodEventRouter::InputMethodChanged(
    input_method::InputMethodManager *manager,
    bool show_message) {
  Profile *profile = ProfileManager::GetDefaultProfile();
  extensions::EventRouter *router =
      extensions::ExtensionSystem::Get(profile)->event_router();

  if (!router->HasEventListener(extensions::event_names::kOnInputMethodChanged))
    return;

  scoped_ptr<ListValue> args(new ListValue());
  StringValue *input_method_name = new StringValue(
      extensions::InputMethodAPI::GetInputMethodForXkb(
          manager->GetCurrentInputMethod().id()));
  args->Append(input_method_name);

  // The router will only send the event to extensions that are listening.
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::event_names::kOnInputMethodChanged, args.Pass()));
  event->restrict_to_profile = profile;
  extensions::ExtensionSystem::Get(profile)->event_router()->
      BroadcastEvent(event.Pass());
}

}  // namespace chromeos
