// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMAND_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMAND_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/common/extensions/command.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"

class PrefServiceSyncable;
class Profile;

namespace base {
  class DictionaryValue;
}

namespace ui {
  class Accelerator;
}

namespace extensions {

// This service keeps track of preferences related to extension commands
// (assigning initial keybindings on install and removing them on deletion
// and answers questions related to which commands are active.
class CommandService : public ProfileKeyedAPI,
                       public content::NotificationObserver {
 public:
  // An enum specifying whether to fetch all extension commands or only active
  // ones.
  enum QueryType {
    ALL,
    ACTIVE_ONLY,
  };

  // Register prefs for keybinding.
  static void RegisterUserPrefs(PrefServiceSyncable* user_prefs);

  // Constructs a CommandService object for the given profile.
  explicit CommandService(Profile* profile);
  virtual ~CommandService();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<CommandService>* GetFactoryInstance();

  // Convenience method to get the CommandService for a profile.
  static CommandService* Get(Profile* profile);

  // Gets the command (if any) for the browser action of an extension given
  // its |extension_id|. The function consults the master list to see if
  // the command is active. Returns false if the extension has no browser
  // action. Returns false if the command is not active and |type| requested
  // is ACTIVE_ONLY. |command| contains the command found and |active| (if not
  // NULL) contains whether |command| is active.
  bool GetBrowserActionCommand(const std::string& extension_id,
                               QueryType type,
                               extensions::Command* command,
                               bool* active);

  // Gets the command (if any) for the page action of an extension given
  // its |extension_id|. The function consults the master list to see if
  // the command is active. Returns false if the extension has no page
  // action. Returns false if the command is not active and |type| requested
  // is ACTIVE_ONLY. |command| contains the command found and |active| (if not
  // NULL) contains whether |command| is active.
  bool GetPageActionCommand(const std::string& extension_id,
                            QueryType type,
                            extensions::Command* command,
                            bool* active);

  // Gets the command (if any) for the script badge of an extension given
  // its |extension_id|. The function consults the master list to see if
  // the command is active. Returns false if the extension has no script
  // badge. Returns false if the command is not active and |type| requested
  // is ACTIVE_ONLY. |command| contains the command found and |active| (if not
  // NULL) contains whether |command| is active.
  bool GetScriptBadgeCommand(const std::string& extension_id,
                             QueryType type,
                             extensions::Command* command,
                             bool* active);

  // Gets the active command (if any) for the named commands of an extension
  // given its |extension_id|. The function consults the master list to see if
  // the command is active. Returns an empty map if the extension has no
  // named commands or no active named commands when |type| requested is
  // ACTIVE_ONLY.
  bool GetNamedCommands(const std::string& extension_id,
                        QueryType type,
                        extensions::CommandMap* command_map);

  // Records a keybinding |accelerator| as active for an extension with id
  // |extension_id| and command with the name |command_name|. If
  // |allow_overrides| is false, the keybinding must be free for the change to
  // be recorded (as determined by the master list in |user_prefs|). If
  // |allow_overwrites| is true, any previously recorded keybinding for this
  // |accelerator| will be overwritten. Returns true if the change was
  // successfully recorded.
  bool AddKeybindingPref(const ui::Accelerator& accelerator,
                         std::string extension_id,
                         std::string command_name,
                         bool allow_overrides);

  // Update the keybinding prefs (for a command with a matching |extension_id|
  // and |command_name|) to |keystroke|. If the command had another key assigned
  // that key assignment will be removed.
  void UpdateKeybindingPrefs(const std::string& extension_id,
                             const std::string& command_name,
                             const std::string& keystroke);

  // Finds the shortcut assigned to a command with the name |command_name|
  // within an extension with id |extension_id|. Returns an empty Accelerator
  // object (with keycode VKEY_UNKNOWN) if no shortcut is assigned or the
  // command is not found.
  ui::Accelerator FindShortcutForCommand(const std::string& extension_id,
                                         const std::string& command);

  // Overridden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<CommandService>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "CommandService";
  }
  static const bool kServiceRedirectedInIncognito = true;

  // An enum specifying the types of icons that can have a command.
  enum ExtensionActionType {
    BROWSER_ACTION,
    PAGE_ACTION,
    SCRIPT_BADGE,
  };

  // Assigns initial keybinding for a given |extension|'s page action, browser
  // action and named commands. In each case, if the suggested keybinding is
  // free, it will be taken by this extension. If not, that keybinding request
  // is ignored. |user_pref| is the PrefService used to record the new
  // keybinding assignment.
  void AssignInitialKeybindings(const extensions::Extension* extension);

  // Removes all keybindings for a given extension by its |extension_id|.
  // |command_name| is optional and if specified, causes only the command with
  // the name |command_name| to be removed.
  void RemoveKeybindingPrefs(const std::string& extension_id,
                             const std::string& command_name);

  bool GetExtensionActionCommand(const std::string& extension_id,
                                 QueryType query_type,
                                 extensions::Command* command,
                                 bool* active,
                                 ExtensionActionType action_type);

  // The content notification registrar for listening to extension events.
  content::NotificationRegistrar registrar_;

  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(CommandService);
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMAND_SERVICE_H_
