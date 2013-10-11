// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"

class Profile;

namespace ui {
class Accelerator;
}

namespace extensions {

class ActiveTabPermissionGranter;
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

  class Delegate {
   public:
    // Gets the ActiveTabPermissionGranter for the active tab, if any.
    // If there is no active tab then returns NULL.
    virtual ActiveTabPermissionGranter* GetActiveTabPermissionGranter() = 0;
  };

  // If |extension_filter| is not ALL_EXTENSIONS, only keybindings by
  // by extensions that match the filter will be registered.
  ExtensionKeybindingRegistry(Profile* profile,
                              ExtensionFilter extension_filter,
                              Delegate* delegate);

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
  void RemoveExtensionKeybinding(
      const Extension* extension,
      const std::string& command_name);
  // Overridden by platform specific implementations to provide additional
  // unregistration (which varies between platforms).
  virtual void RemoveExtensionKeybindingImpl(
      const ui::Accelerator& accelerator,
      const std::string& command_name) = 0;

  // Make sure all extensions registered have keybindings added.
  void Init();

  // Whether to ignore this command. Only browserAction commands and pageAction
  // commands are currently ignored, since they are handled elsewhere.
  bool ShouldIgnoreCommand(const std::string& command) const;

  // Notifies appropriate parties that a command has been executed.
  void CommandExecuted(const std::string& extension_id,
                       const std::string& command);

  // Maps an accelerator to a string pair (extension id, command name) for
  // commands that have been registered. This keeps track of the targets for the
  // keybinding event (which named command to call in which extension). On GTK,
  // this map contains registration for pageAction and browserAction commands,
  // whereas on other platforms it does not.
  typedef std::map< ui::Accelerator,
                    std::pair<std::string, std::string> > EventTargets;
  EventTargets event_targets_;

 private:
  // Returns true if the |extension| matches our extension filter.
  bool ExtensionMatchesFilter(const extensions::Extension* extension);

  // The content notification registrar for listening to extension events.
  content::NotificationRegistrar registrar_;

  // Weak pointer to our profile. Not owned by us.
  Profile* profile_;

  // What extensions to register keybindings for.
  ExtensionFilter extension_filter_;

  // Weak pointer to our delegate. Not owned by us. Must outlive this class.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionKeybindingRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
