// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_COMMAND_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_COMMAND_HANDLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {
class Command;
class CommandService;
}

class Extension;

namespace extensions {

// The handler page for the Extension Commands UI overlay.
class CommandHandler : public content::WebUIMessageHandler {
 public:
  CommandHandler();
  virtual ~CommandHandler();

  // Fetches the localized values for the page and deposits them into
  // |localized_strings|.
  void GetLocalizedValues(base::DictionaryValue* localized_strings);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
   // Update the list of extension commands in the config UI.
  void UpdateCommandDataOnPage();

  // Handles requests from javascript to fetch the extensions data, including
  // the commands it contains.
  // Replies back through: ExtensionCommandsOverlay.returnExtensionsData.
  void HandleRequestExtensionsData(const base::ListValue* args);

  // Handles requests from javascript to set a particular keyboard shortcut
  // for a given extension command.
  void HandleSetExtensionCommandShortcut(const base::ListValue* args);

  // Handles requests from javascript to temporarily disable general Chrome
  // shortcut handling while the web page is capturing which shortcut to use.
  void HandleSetShortcutHandlingSuspended(const base::ListValue* args);

  // Fetches all known commands, active and inactive and returns them through
  // |commands|.
  void GetAllCommands(base::DictionaryValue* commands);

  DISALLOW_COPY_AND_ASSIGN(CommandHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_COMMAND_HANDLER_H_
