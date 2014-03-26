// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_EVENT_ROUTER_H_

#include <string>

#include "base/macros.h"
#include "chrome/common/extensions/api/runtime.h"

namespace base {
class DictionaryValue;
class Version;
}

namespace content {
class BrowserContext;
};

namespace extensions {

// Dispatches events to extensions such as onStartup, onInstalled, etc.
class RuntimeEventRouter {
 public:
  // Dispatches the onStartup event to all currently-loaded extensions.
  static void DispatchOnStartupEvent(content::BrowserContext* context,
                                     const std::string& extension_id);

  // Dispatches the onInstalled event to the given extension.
  static void DispatchOnInstalledEvent(content::BrowserContext* context,
                                       const std::string& extension_id,
                                       const base::Version& old_version,
                                       bool chrome_updated);

  // Dispatches the onUpdateAvailable event to the given extension.
  static void DispatchOnUpdateAvailableEvent(
      content::BrowserContext* context,
      const std::string& extension_id,
      const base::DictionaryValue* manifest);

  // Dispatches the onBrowserUpdateAvailable event to all extensions.
  static void DispatchOnBrowserUpdateAvailableEvent(
      content::BrowserContext* context);

  // Dispatches the onRestartRequired event to the given app.
  static void DispatchOnRestartRequiredEvent(
      content::BrowserContext* context,
      const std::string& app_id,
      api::runtime::OnRestartRequired::Reason reason);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(RuntimeEventRouter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_EVENT_ROUTER_H_
