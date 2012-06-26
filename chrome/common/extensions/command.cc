// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/command.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;

namespace {

ui::Accelerator ParseImpl(const std::string& accelerator,
                          const std::string& platform_key,
                          int index,
                          string16* error) {
  if (platform_key != values::kKeybindingPlatformWin &&
      platform_key != values::kKeybindingPlatformMac &&
      platform_key != values::kKeybindingPlatformChromeOs &&
      platform_key != values::kKeybindingPlatformLinux &&
      platform_key != values::kKeybindingPlatformDefault) {
    *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidKeyBindingUnknownPlatform,
        base::IntToString(index),
        platform_key);
    return ui::Accelerator();
  }

  std::vector<std::string> tokens;
  base::SplitString(accelerator, '+', &tokens);
  if (tokens.size() < 2 || tokens.size() > 3) {
    *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidKeyBinding,
        base::IntToString(index),
        platform_key,
        accelerator);
    return ui::Accelerator();
  }

  // Now, parse it into an accelerator.
  int modifiers = ui::EF_NONE;
  ui::KeyboardCode key = ui::VKEY_UNKNOWN;
  for (size_t i = 0; i < tokens.size(); i++) {
    if (tokens[i] == "Ctrl") {
      modifiers |= ui::EF_CONTROL_DOWN;
    } else if (tokens[i] == "Alt") {
      modifiers |= ui::EF_ALT_DOWN;
    } else if (tokens[i] == "Shift") {
      modifiers |= ui::EF_SHIFT_DOWN;
    } else if (tokens[i] == "Command" && platform_key == "mac") {
      // TODO(finnur): Implement for Mac.
      // TODO(finnur): Reject Shift modifier if no Cmd/Opt (see below).
    } else if (tokens[i] == "Option" && platform_key == "mac") {
      // TODO(finnur): Implement for Mac.
    } else if (tokens[i].size() == 1) {
      if (key != ui::VKEY_UNKNOWN) {
        // Multiple key assignments.
        key = ui::VKEY_UNKNOWN;
        break;
      }
      if (tokens[i][0] >= 'A' && tokens[i][0] <= 'Z') {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_A + (tokens[i][0] - 'A'));
      } else if (tokens[i][0] >= '0' && tokens[i][0] <= '9') {
        key = static_cast<ui::KeyboardCode>(ui::VKEY_0 + (tokens[i][0] - '0'));
      } else {
        key = ui::VKEY_UNKNOWN;
        break;
      }
    } else {
      *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidKeyBinding,
          base::IntToString(index),
          platform_key,
          accelerator);
      return ui::Accelerator();
    }
  }
  bool ctrl = (modifiers & ui::EF_CONTROL_DOWN) != 0;
  bool alt = (modifiers & ui::EF_ALT_DOWN) != 0;
  bool shift = (modifiers & ui::EF_SHIFT_DOWN) != 0;
  // We support Ctrl+foo, Alt+foo, Ctrl+Shift+foo, Alt+Shift+foo, but not
  // Ctrl+Alt+foo and not Shift+foo either. For a more detailed reason why we
  // don't support Ctrl+Alt+foo see this article:
  // http://blogs.msdn.com/b/oldnewthing/archive/2004/03/29/101121.aspx.
  if (key == ui::VKEY_UNKNOWN || (ctrl && alt) ||
      ((platform_key != "mac") && shift && !ctrl && !alt)) {
    *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidKeyBinding,
        base::IntToString(index),
        platform_key,
        accelerator);
    return ui::Accelerator();
  }

  return ui::Accelerator(key, modifiers);
}

}  // namespace

namespace extensions {

Command::Command() {}

Command::Command(const std::string& command_name,
                 const string16& description,
                 const std::string& accelerator)
    : command_name_(command_name),
      description_(description) {
  string16 error;
  accelerator_ = ParseImpl(accelerator, CommandPlatform(), 0, &error);
}

Command::~Command() {}

// static
std::string Command::CommandPlatform() {
#if defined(OS_WIN)
  return values::kKeybindingPlatformWin;
#elif defined(OS_MACOSX)
  return values::kKeybindingPlatformMac;
#elif defined(OS_CHROMEOS)
  return values::kKeybindingPlatformChromeOs;
#elif defined(OS_LINUX)
  return values::kKeybindingPlatformLinux;
#else
  return "";
#endif
}

// static
ui::Accelerator Command::StringToAccelerator(const std::string& accelerator) {
  string16 error;
  Command command;
  ui::Accelerator parsed =
      ParseImpl(accelerator, Command::CommandPlatform(), 0, &error);
  return parsed;
}

bool Command::Parse(DictionaryValue* command,
                    const std::string& command_name,
                    int index,
                    string16* error) {
  DCHECK(!command_name.empty());

  // We'll build up a map of platform-to-shortcut suggestions.
  std::map<const std::string, std::string> suggestions;

  // First try to parse the |suggested_key| as a dictionary.
  DictionaryValue* suggested_key_dict;
  if (command->GetDictionary(keys::kSuggestedKey, &suggested_key_dict)) {
    DictionaryValue::key_iterator iter = suggested_key_dict->begin_keys();
    for ( ; iter != suggested_key_dict->end_keys(); ++iter) {
      // For each item in the dictionary, extract the platforms specified.
      std::string suggested_key_string;
      if (suggested_key_dict->GetString(*iter, &suggested_key_string) &&
          !suggested_key_string.empty()) {
        // Found a platform, add it to the suggestions list.
        suggestions[*iter] = suggested_key_string;
      } else {
        *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidKeyBinding,
            base::IntToString(index),
            keys::kSuggestedKey,
            "Missing");
        return false;
      }
    }
  } else {
    // No dictionary was found, fall back to using just a string, so developers
    // don't have to specify a dictionary if they just want to use one default
    // for all platforms.
    std::string suggested_key_string;
    if (command->GetString(keys::kSuggestedKey, &suggested_key_string) &&
        !suggested_key_string.empty()) {
      // If only a single string is provided, it must be default for all.
      suggestions["default"] = suggested_key_string;
    } else {
      *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidKeyBinding,
          base::IntToString(index),
          keys::kSuggestedKey,
          "Missing");
       return false;
    }
  }

  std::string platform = CommandPlatform();
  std::string key = platform;
  if (suggestions.find(key) == suggestions.end())
    key = values::kKeybindingPlatformDefault;
  if (suggestions.find(key) == suggestions.end()) {
    *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidKeyBindingMissingPlatform,
        base::IntToString(index),
        keys::kSuggestedKey,
        platform);
    return false;  // No platform specified and no fallback. Bail.
  }

  // For developer convenience, we parse all the suggestions (and complain about
  // errors for platforms other than the current one) but use only what we need.
  std::map<const std::string, std::string>::const_iterator iter =
      suggestions.begin();
  for ( ; iter != suggestions.end(); ++iter) {
    // Note that we pass iter->first to pretend we are on a platform we're not
    // on.
    ui::Accelerator accelerator =
        ParseImpl(iter->second, iter->first, index, error);
    if (accelerator.key_code() == ui::VKEY_UNKNOWN) {
      *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidKeyBinding,
          base::IntToString(index),
          iter->first,
          iter->second);
      return false;
    }

    if (iter->first == key) {
      // This platform is our platform, so grab this key.
      accelerator_ = accelerator;
      command_name_ = command_name;

      if (command_name !=
              extension_manifest_values::kPageActionKeybindingEvent &&
          command_name !=
              extension_manifest_values::kBrowserActionKeybindingEvent) {
        if (!command->GetString(keys::kDescription, &description_) ||
            description_.empty()) {
          *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidKeyBindingDescription,
              base::IntToString(index));
          return false;
        }
      }
    }
  }
  return true;
}

DictionaryValue* Command::ToValue(const Extension* extension,
                                  bool active) const {
  DictionaryValue* extension_data = new DictionaryValue();

  string16 command_description;
  if (command_name() == values::kBrowserActionKeybindingEvent ||
      command_name() == values::kPageActionKeybindingEvent) {
    command_description =
        l10n_util::GetStringUTF16(IDS_EXTENSION_COMMANDS_GENERIC_ACTIVATE);
  } else {
    command_description = description();
  }
  extension_data->SetString("description", command_description);
  extension_data->SetBoolean("active", active);
  extension_data->SetString("keybinding", accelerator().GetShortcutText());
  extension_data->SetString("command_name", command_name());
  extension_data->SetString("extension_id", extension->id());

  return extension_data;
}

}  // namespace extensions
