// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"

class Profile;

namespace extensions {

class Extension;

// The ExtensionKeybindingRegistry is a class that handles the cross-platform
// logic for keyboard accelerators. See platform-specific implementations for
// implementation details for each platform.
class ExtensionKeybindingRegistry : public content::NotificationObserver {
 public:
   enum ExtensionFilter {
     ALL_EXTENSIONS,
     PLATFORM_APPS_ONLY
   };

   // If |extension_filter| is not ALL_EXTENSIONS, only keybindings by
  // by extensions that match the filter will be registered.
  ExtensionKeybindingRegistry(Profile* profile,
                              ExtensionFilter extension_filter);
  virtual ~ExtensionKeybindingRegistry();

  // Enables/Disables general shortcut handing in Chrome. Implemented in
  // platform-specific ExtensionKeybindingsRegistry* files.
  static void SetShortcutHandlingSuspended(bool suspended);

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  // Add extension keybinding for the events defined by the |extension|.
  // |command_name| is optional, but if not blank then only the command
  // specified will be added.
  virtual void AddExtensionKeybinding(
      const Extension* extension,
      const std::string& command_name) = 0;
  // Remove extension bindings for |extension|. |command_name| is optional,
  // but if not blank then only the command specified will be removed.
  virtual void RemoveExtensionKeybinding(
      const Extension* extension,
      const std::string& command_name) = 0;

  // Make sure all extensions registered have keybindings added.
  void Init();

  // Whether to ignore this command. Only browserAction commands and pageAction
  // commands are currently ignored, since they are handled elsewhere.
  bool ShouldIgnoreCommand(const std::string& command) const;

 private:
  // Returns true if the |extension| matches our extension filter.
  bool ExtensionMatchesFilter(const extensions::Extension* extension);

  // The content notification registrar for listening to extension events.
  content::NotificationRegistrar registrar_;

  // Weak pointer to our profile. Not owned by us.
  Profile* profile_;

  // What extensions to register keybindings for.
  ExtensionFilter extension_filter_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionKeybindingRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
