// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_

#include <list>
#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace ui {
class Accelerator;
}

namespace extensions {

class ActiveTabPermissionGranter;
class Extension;
class ExtensionRegistry;

// The ExtensionKeybindingRegistry is a class that handles the cross-platform
// logic for keyboard accelerators. See platform-specific implementations for
// implementation details for each platform.
class ExtensionKeybindingRegistry : public content::NotificationObserver,
                                    public ExtensionRegistryObserver {
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
  ExtensionKeybindingRegistry(content::BrowserContext* context,
                              ExtensionFilter extension_filter,
                              Delegate* delegate);

  virtual ~ExtensionKeybindingRegistry();

  // Enables/Disables general shortcut handling in Chrome. Implemented in
  // platform-specific ExtensionKeybindingsRegistry* files.
  static void SetShortcutHandlingSuspended(bool suspended);

  // Execute the command bound to |accelerator| and provided by the extension
  // with |extension_id|, if it exists.
  void ExecuteCommand(const std::string& extension_id,
                      const ui::Accelerator& accelerator);

  // Check whether the specified |accelerator| has been registered.
  bool IsAcceleratorRegistered(const ui::Accelerator& accelerator) const;

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

  // Fire event targets which the specified |accelerator| is binding with.
  // Returns true if we can find the appropriate event targets.
  bool NotifyEventTargets(const ui::Accelerator& accelerator);

  // Notifies appropriate parties that a command has been executed.
  void CommandExecuted(const std::string& extension_id,
                       const std::string& command);

  // Add event target (extension_id, command name) to the target list of
  // |accelerator|. Note that only media keys can have more than one event
  // target.
  void AddEventTarget(const ui::Accelerator& accelerator,
                      const std::string& extension_id,
                      const std::string& command_name);

  // Get the first event target by the given |accelerator|. For a valid
  // accelerator it should have only one event target, except for media keys.
  // Returns true if we can find it, |extension_id| and |command_name| will be
  // set to the right target; otherwise, false is returned and |extension_id|,
  // |command_name| are unchanged.
  bool GetFirstTarget(const ui::Accelerator& accelerator,
                      std::string* extension_id,
                      std::string* command_name) const;

  // Returns true if the |event_targets_| is empty; otherwise returns false.
  bool IsEventTargetsEmpty() const;

  // Returns the BrowserContext for this registry.
  content::BrowserContext* browser_context() const { return browser_context_; }

 private:
  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // Returns true if the |extension| matches our extension filter.
  bool ExtensionMatchesFilter(const extensions::Extension* extension);

  // Execute commands for |accelerator|. If |extension_id| is empty, execute all
  // commands bound to |accelerator|, otherwise execute only commands bound by
  // the corresponding extension. Returns true if at least one command was
  // executed.
  bool ExecuteCommands(const ui::Accelerator& accelerator,
                       const std::string& extension_id);

  // The content notification registrar for listening to extension events.
  content::NotificationRegistrar registrar_;

  content::BrowserContext* browser_context_;

  // What extensions to register keybindings for.
  ExtensionFilter extension_filter_;

  // Weak pointer to our delegate. Not owned by us. Must outlive this class.
  Delegate* delegate_;

  // Maps an accelerator to a list of string pairs (extension id, command name)
  // for commands that have been registered. This keeps track of the targets for
  // the keybinding event (which named command to call in which extension). On
  // GTK this map contains registration for pageAction and browserAction
  // commands, whereas on other platforms it does not. Note that normal
  // accelerator (which isn't media keys) has only one target, while the media
  // keys can have more than one.
  typedef std::list<std::pair<std::string, std::string> > TargetList;
  typedef std::map<ui::Accelerator, TargetList> EventTargets;
  EventTargets event_targets_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionKeybindingRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_KEYBINDING_REGISTRY_H_
