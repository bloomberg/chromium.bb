// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_COMMAND_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_COMMAND_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/scoped_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace content {
class WebUIDataSource;
}

namespace extensions {
class Command;
class CommandService;
class Extension;
class ExtensionRegistry;

// The handler page for the Extension Commands UI overlay.
class CommandHandler : public content::WebUIMessageHandler,
                       public ExtensionRegistryObserver {
 public:
  explicit CommandHandler(Profile* profile);
  virtual ~CommandHandler();

  // Fetches the localized values for the page and deposits them into |source|.
  void GetLocalizedValues(content::WebUIDataSource* source);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

   // Update the list of extension commands in the config UI.
  void UpdateCommandDataOnPage();

  // Handles requests from javascript to fetch the extensions data, including
  // the commands it contains.
  // Replies back through: ExtensionCommandsOverlay.returnExtensionsData.
  void HandleRequestExtensionsData(const base::ListValue* args);

  // Handles requests from javascript to set a particular keyboard shortcut
  // for a given extension command.
  void HandleSetExtensionCommandShortcut(const base::ListValue* args);

  // Handles requests from javascript to change the scope of a particular
  // keyboard shortcut for a given extension command.
  void HandleSetCommandScope(const base::ListValue* args);

  // Handles requests from javascript to temporarily disable general Chrome
  // shortcut handling while the web page is capturing which shortcut to use.
  void HandleSetShortcutHandlingSuspended(const base::ListValue* args);

  // Fetches all known commands, active and inactive and returns them through
  // |commands|.
  void GetAllCommands(base::DictionaryValue* commands);

  Profile* profile_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(CommandHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_COMMAND_HANDLER_H_
