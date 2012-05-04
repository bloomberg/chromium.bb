// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_commands.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_manifest_constants.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;

namespace extensions {

Command::Command() {}

Command::~Command() {}

ui::Accelerator Command::ParseImpl(
    const std::string& shortcut,
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
  base::SplitString(shortcut, '+', &tokens);
  if (tokens.size() < 2 || tokens.size() > 3) {
    *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidKeyBinding,
        base::IntToString(index),
        platform_key,
        shortcut);
    return ui::Accelerator();
  }

  // Now, parse it into an accelerator.
  bool ctrl = false;
  bool alt = false;
  bool shift = false;
  ui::KeyboardCode key = ui::VKEY_UNKNOWN;
  for (size_t i = 0; i < tokens.size(); i++) {
    if (tokens[i] == "Ctrl") {
      ctrl = true;
    } else if (tokens[i] == "Alt") {
      alt = true;
    } else if (tokens[i] == "Shift") {
      shift = true;
    } else if (tokens[i] == "Command" && platform_key == "mac") {
      // TODO(finnur): Implement for Mac.
    } else if (tokens[i] == "Option" && platform_key == "mac") {
      // TODO(finnur): Implement for Mac.
    } else if (tokens[i].size() == 1 &&
               tokens[i][0] >= 'A' && tokens[i][0] <= 'Z') {
      if (key != ui::VKEY_UNKNOWN) {
        // Multiple key assignments.
        key = ui::VKEY_UNKNOWN;
        break;
      }

      key = static_cast<ui::KeyboardCode>(ui::VKEY_A + (tokens[i][0] - 'A'));
    } else {
      *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidKeyBinding,
          base::IntToString(index),
          platform_key,
          shortcut);
      return ui::Accelerator();
    }
  }

  // We support Ctrl+foo, Alt+foo, Ctrl+Shift+foo, Alt+Shift+foo, but not
  // Ctrl+Alt+foo. For a more detailed reason why we don't support Ctrl+Alt+foo:
  // http://blogs.msdn.com/b/oldnewthing/archive/2004/03/29/101121.aspx.
  if (key == ui::VKEY_UNKNOWN || (ctrl && alt)) {
    *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidKeyBinding,
        base::IntToString(index),
        platform_key,
        shortcut);
    return ui::Accelerator();
  }

  return ui::Accelerator(key, shift, ctrl, alt);
}

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
      // If only a signle string is provided, it must be default for all.
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

}  // namespace extensions
