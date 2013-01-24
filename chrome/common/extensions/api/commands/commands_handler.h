// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_COMMANDS_COMMANDS_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_COMMANDS_COMMANDS_HANDLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/command.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

namespace extensions {

struct CommandsInfo : public Extension::ManifestData {
  CommandsInfo();
  virtual ~CommandsInfo();

  // Optional list of commands (keyboard shortcuts).
  // These commands are the commands which the extension wants to use, which are
  // not necessarily the ones it can use, as it might be inactive (see also
  // Get*Command[s] in CommandService).
  scoped_ptr<Command> browser_action_command;
  scoped_ptr<Command> page_action_command;
  scoped_ptr<Command> script_badge_command;
  CommandMap named_commands;

  static const Command* GetBrowserActionCommand(const Extension* extension);
  static const Command* GetPageActionCommand(const Extension* extension);
  static const Command* GetScriptBadgeCommand(const Extension* extension);
  static const CommandMap* GetNamedCommands(const Extension* extension);
};

// Parses the "commands" manifest key.
class CommandsHandler : public ManifestHandler {
 public:
  CommandsHandler();
  virtual ~CommandsHandler();

  virtual bool Parse(const base::Value* value,
                     Extension* extension,
                     string16* error) OVERRIDE;

  virtual bool HasNoKey(Extension* extension, string16* error) OVERRIDE;

 private:
  // If the extension defines a browser action, but no command for it, then
  // we synthesize a generic one, so the user can configure a shortcut for it.
  // No keyboard shortcut will be assigned to it, until the user selects one.
  void MaybeSetBrowserActionDefault(const Extension* extension,
                                    CommandsInfo* info);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_COMMANDS_COMMANDS_HANDLER_H_
