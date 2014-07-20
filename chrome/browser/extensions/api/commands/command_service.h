// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMAND_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMAND_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_observer.h"
#include "chrome/common/extensions/command.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class Profile;

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
}

namespace ui {
class Accelerator;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {
class ExtensionRegistry;

// This service keeps track of preferences related to extension commands
// (assigning initial keybindings on install and removing them on deletion
// and answers questions related to which commands are active.
class CommandService : public BrowserContextKeyedAPI,
                       public ExtensionRegistryObserver {
 public:
  // An enum specifying whether to fetch all extension commands or only active
  // ones.
  enum QueryType {
    ALL,
    ACTIVE_ONLY,
  };

  // An enum specifying whether the command is global in scope or not. Global
  // commands -- unlike regular commands -- have a global keyboard hook
  // associated with them (and therefore work when Chrome doesn't have focus).
  enum CommandScope {
    REGULAR,    // Regular (non-globally scoped) command.
    GLOBAL,     // Global command (works when Chrome doesn't have focus)
    ANY_SCOPE,  // All commands, regardless of scope (used when querying).
  };

  // An enum specifying the types of commands that can be used by an extension.
  enum ExtensionCommandType {
    NAMED,
    BROWSER_ACTION,
    PAGE_ACTION
  };

  // Register prefs for keybinding.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Constructs a CommandService object for the given profile.
  explicit CommandService(content::BrowserContext* context);
  virtual ~CommandService();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<CommandService>* GetFactoryInstance();

  // Convenience method to get the CommandService for a profile.
  static CommandService* Get(content::BrowserContext* context);

  // Returns true if |extension| is permitted to and does remove the bookmark
  // shortcut key.
  static bool RemovesBookmarkShortcut(const Extension* extension);

  // Returns true if |extension| is permitted to and does remove the bookmark
  // open pages shortcut key.
  static bool RemovesBookmarkOpenPagesShortcut(const Extension* extension);

  // Gets the command (if any) for the browser action of an extension given
  // its |extension_id|. The function consults the master list to see if
  // the command is active. Returns false if the extension has no browser
  // action. Returns false if the command is not active and |type| requested
  // is ACTIVE_ONLY. |command| contains the command found and |active| (if not
  // NULL) contains whether |command| is active.
  bool GetBrowserActionCommand(const std::string& extension_id,
                               QueryType type,
                               Command* command,
                               bool* active) const;

  // Gets the command (if any) for the page action of an extension given
  // its |extension_id|. The function consults the master list to see if
  // the command is active. Returns false if the extension has no page
  // action. Returns false if the command is not active and |type| requested
  // is ACTIVE_ONLY. |command| contains the command found and |active| (if not
  // NULL) contains whether |command| is active.
  bool GetPageActionCommand(const std::string& extension_id,
                            QueryType type,
                            Command* command,
                            bool* active) const;

  // Gets the active named commands (if any) for the extension with
  // |extension_id|. The function consults the master list to see if the
  // commands are active. Returns an empty map if the extension has no named
  // commands of the right |scope| or no such active named commands when |type|
  // requested is ACTIVE_ONLY.
  bool GetNamedCommands(const std::string& extension_id,
                        QueryType type,
                        CommandScope scope,
                        CommandMap* command_map) const;

  // Records a keybinding |accelerator| as active for an extension with id
  // |extension_id| and command with the name |command_name|. If
  // |allow_overrides| is false, the keybinding must be free for the change to
  // be recorded (as determined by the master list in |user_prefs|). If
  // |allow_overwrites| is true, any previously recorded keybinding for this
  // |accelerator| will be overwritten. If |global| is true, the command will
  // be registered as a global command (be active even when Chrome does not have
  // focus. Returns true if the change was successfully recorded.
  bool AddKeybindingPref(const ui::Accelerator& accelerator,
                         std::string extension_id,
                         std::string command_name,
                         bool allow_overrides,
                         bool global);

  // Removes all keybindings for a given extension by its |extension_id|.
  // |command_name| is optional and if specified, causes only the command with
  // the name |command_name| to be removed.
  void RemoveKeybindingPrefs(const std::string& extension_id,
                             const std::string& command_name);

  // Update the keybinding prefs (for a command with a matching |extension_id|
  // and |command_name|) to |keystroke|. If the command had another key assigned
  // that key assignment will be removed.
  void UpdateKeybindingPrefs(const std::string& extension_id,
                             const std::string& command_name,
                             const std::string& keystroke);

  // Set the scope of the keybinding. If |global| is true, the keybinding works
  // even when Chrome does not have focus. If the scope requested is already
  // set, the function returns false, otherwise true.
  bool SetScope(const std::string& extension_id,
                const std::string& command_name,
                bool global);

  // Finds the command with the name |command_name| within an extension with id
  // |extension_id| . Returns an empty Command object (with keycode
  // VKEY_UNKNOWN) if the command is not found.
  Command FindCommandByName(const std::string& extension_id,
                            const std::string& command) const;

  // If the extension with |extension_id| binds a command to |accelerator|,
  // returns true and assigns *|command| and *|command_type| to the command and
  // its type if non-NULL.
  bool GetBoundExtensionCommand(const std::string& extension_id,
                                const ui::Accelerator& accelerator,
                                Command* command,
                                ExtensionCommandType* command_type) const;

  // Returns true if |extension| is permitted to and does override the bookmark
  // shortcut key.
  bool OverridesBookmarkShortcut(const Extension* extension) const;

 private:
  friend class BrowserContextKeyedAPIFactory<CommandService>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "CommandService";
  }
  static const bool kServiceRedirectedInIncognito = true;

  // ExtensionRegistryObserver.
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  // Updates keybindings for a given |extension|'s page action, browser action
  // and named commands. Assigns new keybindings and removes relinquished
  // keybindings if not changed by the user. In the case of adding keybindings,
  // if the suggested keybinding is free, it will be taken by this extension. If
  // not, the keybinding request is ignored.
  void UpdateKeybindings(const Extension* extension);

  // On update, removes keybindings that the extension previously suggested but
  // now no longer does, as long as the user has not modified them.
  void RemoveRelinquishedKeybindings(const Extension* extension);

  // Assigns keybindings that the extension suggests, as long as they are not
  // already assigned.
  void AssignKeybindings(const Extension* extension);

  // Checks if |extension| is permitted to automatically assign the
  // |accelerator| key.
  bool CanAutoAssign(const Command &command,
                     const Extension* extension);

  // Updates the record of |extension|'s most recent suggested command shortcut
  // keys in the preferences.
  void UpdateExtensionSuggestedCommandPrefs(const Extension* extension);

  // Remove suggested key command prefs that apply to commands that have been
  // removed.
  void RemoveDefunctExtensionSuggestedCommandPrefs(const Extension* extension);

  // Returns true if the user modified a command's shortcut key from the
  // |extension|-suggested value.
  bool IsCommandShortcutUserModified(const Extension* extension,
                                     const std::string& command_name);

  // Returns true if the extension is changing the binding of |command_name| on
  // install.
  bool IsKeybindingChanging(const Extension* extension,
                            const std::string& command_name);

  // Returns |extension|'s previous suggested key for |command_name| in the
  // preferences, or the empty string if none.
  std::string GetSuggestedKeyPref(const Extension* extension,
                                  const std::string& command_name);

  bool GetExtensionActionCommand(const std::string& extension_id,
                                 QueryType query_type,
                                 Command* command,
                                 bool* active,
                                 ExtensionCommandType action_type) const;

  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(CommandService);
};

template <>
void
    BrowserContextKeyedAPIFactory<CommandService>::DeclareFactoryDependencies();

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMAND_SERVICE_H_
