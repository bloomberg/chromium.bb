// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_

#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
struct UnloadedExtensionInfo;

// Observer for ExtensionRegistry. Exists in a separate header file to reduce
// the include file burden for typical clients of ExtensionRegistry.
class ExtensionRegistryObserver {
 public:
  virtual ~ExtensionRegistryObserver() {}

  // Called after an extension is loaded. The extension will exclusively exist
  // in the enabled_extensions set of ExtensionRegistry.
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const Extension* extension) {}

  // Called after an extension is unloaded. The extension no longer exists in
  // any of the ExtensionRegistry sets (enabled, disabled, etc.).
  virtual void OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   UnloadedExtensionInfo::Reason reason) {}

  // Called when |extension| is about to be installed. |is_update| is true if
  // the installation is the result of it updating, in which case |old_name| is
  // the name of the extension's previous version.
  // The ExtensionRegistry will not be tracking |extension| at the time this
  // event is fired, but will be immediately afterwards (note: not necessarily
  // enabled; it might be installed in the disabled or even blacklisted sets,
  // for example).
  // Note that it's much more common to care about extensions being loaded
  // (OnExtensionLoaded).
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      const std::string& old_name) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_
