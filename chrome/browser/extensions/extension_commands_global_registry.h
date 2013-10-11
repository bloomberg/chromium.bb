// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COMMANDS_GLOBAL_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COMMANDS_GLOBAL_REGISTRY_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "ui/base/accelerators/accelerator.h"

class Profile;

namespace extensions {
class Extension;
}

namespace extensions {

// ExtensionCommandsGlobalRegistry is a class that handles the cross-platform
// implementation of the global shortcut registration for the Extension Commands
// API).
// Note: It handles regular extension commands (not browserAction and pageAction
// popups, which are not bindable to global shortcuts). This class registers the
// accelerators on behalf of the extensions and routes the commands to them via
// the BrowserEventRouter.
class ExtensionCommandsGlobalRegistry
    : public ProfileKeyedAPI,
      public ExtensionKeybindingRegistry,
      public GlobalShortcutListener::Observer {
 public:
  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<
      ExtensionCommandsGlobalRegistry>* GetFactoryInstance();

  // Convenience method to get the ExtensionCommandsGlobalRegistry for a
  // profile.
  static ExtensionCommandsGlobalRegistry* Get(Profile* profile);

  explicit ExtensionCommandsGlobalRegistry(Profile* profile);
  virtual ~ExtensionCommandsGlobalRegistry();

 private:
  friend class ProfileKeyedAPIFactory<ExtensionCommandsGlobalRegistry>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "ExtensionCommandsGlobalRegistry";
  }

  // Overridden from ExtensionKeybindingRegistry:
  virtual void AddExtensionKeybinding(
      const Extension* extension,
      const std::string& command_name) OVERRIDE;
  virtual void RemoveExtensionKeybindingImpl(
      const ui::Accelerator& accelerator,
      const std::string& command_name) OVERRIDE;

  // Called by the GlobalShortcutListener object when a shortcut this class has
  // registered for has been pressed.
  virtual void OnKeyPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // Weak pointer to our profile. Not owned by us.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCommandsGlobalRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COMMANDS_GLOBAL_REGISTRY_H_
