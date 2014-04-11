// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_app_runtime_api.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension.h"

namespace extensions {

// static
void ShellAppRuntimeAPI::DispatchOnLaunchedEvent(EventRouter* event_router,
                                                 const Extension* extension) {
  // This is similar to apps::AppEventRouter::DispatchOnLaunchedEvent but
  // avoids the dependency on apps.
  scoped_ptr<base::DictionaryValue> launch_data(new base::DictionaryValue());
  launch_data->SetBoolean("isKioskSession", false);
  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  event_args->Append(launch_data.release());
  scoped_ptr<Event> event(
      new Event("app.runtime.onLaunched", event_args.Pass()));
  event_router->DispatchEventWithLazyListener(extension->id(), event.Pass());
}

}  // namespace extensions
