// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COMMANDS_EXTENSION_COMMAND_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_COMMANDS_EXTENSION_COMMAND_SERVICE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_commands.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"

class PrefService;
class Profile;

namespace ui {
  class Accelerator;
}

// This service keeps track of preferences related to extension commands
// (assigning initial keybindings on install and removing them on deletion
// and answers questions related to which commands are active.
class ExtensionCommandService : public ProfileKeyedService,
                                public content::NotificationObserver {
 public:
  // Register prefs for keybinding.
  static void RegisterUserPrefs(PrefService* user_prefs);

  // Constructs an ExtensionCommandService object for the given profile.
  explicit ExtensionCommandService(Profile* profile);
  virtual ~ExtensionCommandService();

  // Gets the active keybinding (if any) for the browser action of an extension
  // given its |extension_id|. The function consults the master list to see if
  // the keybinding is active. Returns NULL if the extension has no browser
  // action or no active keybinding for it.
  const extensions::Command* GetActiveBrowserActionCommand(
      const std::string& extension_id);

  // Gets the active keybinding (if any) for the page action of an extension
  // given its |extension_id|. The function consults the master list to see if
  // the keybinding is active. Returns NULL if the extension has no page action
  // or no active keybinding for it.
  const extensions::Command* GetActivePageActionCommand(
      const std::string& extension_id);

  // Gets the active keybinding (if any) for the named commands of an extension
  // given its |extension_id|. The function consults the master list to see if
  // the keybinding is active. Returns an empty map if the extension has no
  // named commands or no active keybinding for the commands.
  extensions::CommandMap GetActiveNamedCommands(
      const std::string& extension_id);

  // Checks to see if a keybinding |accelerator| for a given |command_name| in
  // an extension with id |extension_id| is registered as active (by consulting
  // the master list in |user_prefs|).
  bool IsKeybindingActive(const ui::Accelerator& accelerator,
                          std::string extension_id,
                          std::string command_name);

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

  // Overridden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Assigns initial keybinding for a given |extension|'s page action, browser
  // action and named commands. In each case, if the suggested keybinding is
  // free, it will be taken by this extension. If not, that keybinding request
  // is ignored. |user_pref| is the PrefService used to record the new
  // keybinding assignment.
  void AssignInitialKeybindings(const Extension* extension);

  // Removes all keybindings for a given extension by its |extension_id|.
  void RemoveKeybindingPrefs(std::string extension_id);

  // The content notification registrar for listening to extension events.
  content::NotificationRegistrar registrar_;

  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCommandService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_COMMANDS_EXTENSION_COMMAND_SERVICE_H_
