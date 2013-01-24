// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/commands/commands_handler.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "extensions/common/error_utils.h"

namespace extensions {

namespace {
// The maximum number of commands (including page action/browser actions) an
// extension can have.
const size_t kMaxCommandsPerExtension = 4;
}  // namespace

CommandsInfo::CommandsInfo() {
}

CommandsInfo::~CommandsInfo() {
}

// static
const Command* CommandsInfo::GetBrowserActionCommand(
   const Extension* extension) {
  CommandsInfo* info = static_cast<CommandsInfo*>(
      extension->GetManifestData(extension_manifest_keys::kCommands));
  return info ? info->browser_action_command.get() : NULL;
}

// static
const Command* CommandsInfo::GetPageActionCommand(const Extension* extension) {
  CommandsInfo* info = static_cast<CommandsInfo*>(
      extension->GetManifestData(extension_manifest_keys::kCommands));
  return info ? info->page_action_command.get() : NULL;
}

// static
const Command* CommandsInfo::GetScriptBadgeCommand(const Extension* extension) {
  CommandsInfo* info = static_cast<CommandsInfo*>(
      extension->GetManifestData(extension_manifest_keys::kCommands));
  return info ? info->script_badge_command.get() : NULL;
}

// static
const CommandMap* CommandsInfo::GetNamedCommands(const Extension* extension) {
  CommandsInfo* info = static_cast<CommandsInfo*>(
      extension->GetManifestData(extension_manifest_keys::kCommands));
  return info ? &info->named_commands : NULL;
}

CommandsHandler::CommandsHandler() {
}

CommandsHandler::~CommandsHandler() {
}

bool CommandsHandler::Parse(const base::Value* value,
                            Extension* extension,
                            string16* error) {
  const base::DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidCommandsKey);
    return false;
  }

  if (dict->size() > kMaxCommandsPerExtension) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        extension_manifest_errors::kInvalidKeyBindingTooMany,
        base::IntToString(kMaxCommandsPerExtension));
    return false;
  }

  scoped_ptr<CommandsInfo> commands_info(new CommandsInfo);

  int command_index = 0;
  for (DictionaryValue::key_iterator iter = dict->begin_keys();
       iter != dict->end_keys(); ++iter) {
    ++command_index;

    const DictionaryValue* command = NULL;
    if (!dict->GetDictionary(*iter, &command)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          extension_manifest_errors::kInvalidKeyBindingDictionary,
          base::IntToString(command_index));
      return false;
    }

    scoped_ptr<extensions::Command> binding(new Command());
    if (!binding->Parse(command, *iter, command_index, error))
      return false;  // |error| already set.

    std::string command_name = binding->command_name();
    if (command_name == extension_manifest_values::kBrowserActionCommandEvent) {
      commands_info->browser_action_command.reset(binding.release());
    } else if (command_name ==
                   extension_manifest_values::kPageActionCommandEvent) {
      commands_info->page_action_command.reset(binding.release());
    } else if (command_name ==
                   extension_manifest_values::kScriptBadgeCommandEvent) {
      commands_info->script_badge_command.reset(binding.release());
    } else {
      if (command_name[0] != '_')  // All commands w/underscore are reserved.
        commands_info->named_commands[command_name] = *binding.get();
    }
  }

  MaybeSetBrowserActionDefault(extension, commands_info.get());

  extension->SetManifestData(extension_manifest_keys::kCommands,
                             commands_info.release());
  return true;
}

bool CommandsHandler::HasNoKey(Extension* extension,
                               string16* error) {
  scoped_ptr<CommandsInfo> commands_info(new CommandsInfo);
  MaybeSetBrowserActionDefault(extension, commands_info.get());
  extension->SetManifestData(extension_manifest_keys::kCommands,
                             commands_info.release());
  return true;
}

void CommandsHandler::MaybeSetBrowserActionDefault(const Extension* extension,
                                                   CommandsInfo* info) {
  if (extension->manifest()->HasKey(extension_manifest_keys::kBrowserAction) &&
      !info->browser_action_command.get()) {
    info->browser_action_command.reset(new Command(
        extension_manifest_values::kBrowserActionCommandEvent, string16(), ""));
  }
}

}  // namespace extensions
