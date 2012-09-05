// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_input_api.h"

#include <string>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/key_identifier_conversion_views.h"
#include "chrome/browser/ui/top_level_widget.h"
#include "chrome/common/chrome_notification_types.h"
#include "ui/base/events/event.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/widget/widget.h"

namespace {

// Keys.
const char kType[] = "type";
const char kKeyIdentifier[] = "keyIdentifier";
const char kAlt[] = "altKey";
const char kCtrl[] = "ctrlKey";
const char kMeta[] = "metaKey";
const char kShift[] = "shiftKey";
const char kKeyDown[] = "keydown";
const char kKeyUp[] = "keyup";

// Errors.
const char kUnknownEventTypeError[] = "Unknown event type.";
const char kUnknownOrUnsupportedKeyIdentiferError[] = "Unknown or unsupported "
    "key identifier.";
const char kUnsupportedModifier[] = "Unsupported modifier.";
const char kNoValidRecipientError[] = "No valid recipient for event.";
const char kKeyEventUnprocessedError[] = "Event was not handled.";
const char kInvalidHeight[] = "Invalid height.";

ui::EventType GetTypeFromString(const std::string& type) {
  if (type == kKeyDown) {
    return ui::ET_KEY_PRESSED;
  } else if (type == kKeyUp) {
    return ui::ET_KEY_RELEASED;
  }
  return ui::ET_UNKNOWN;
}

// Converts a hex string "U+NNNN" to uint16. Returns 0 on error.
uint16 UnicodeIdentifierStringToInt(const std::string& key_identifier) {
  int character = 0;
  if ((key_identifier.length() == 6) &&
      (key_identifier.substr(0, 2) == "U+") &&
      (key_identifier.substr(2).find_first_not_of("0123456789abcdefABCDEF") ==
       std::string::npos)) {
    const bool result =
        base::HexStringToInt(key_identifier.substr(2), &character);
    DCHECK(result) << key_identifier;
  }
  return character;
}

}  // namespace

bool SendKeyboardEventInputFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  std::string type_name;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(kType, &type_name));
  ui::EventType type = GetTypeFromString(type_name);
  if (type == ui::ET_UNKNOWN) {
    error_ = kUnknownEventTypeError;
    return false;
  }

  std::string identifier;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(kKeyIdentifier, &identifier));
  TrimWhitespaceASCII(identifier, TRIM_ALL, &identifier);

  const ui::KeyEvent& prototype_event = KeyEventFromKeyIdentifier(identifier);
  uint16 character = 0;
  if (prototype_event.key_code() == ui::VKEY_UNKNOWN) {
    // Check if |identifier| is "U+NNNN" format.
    character = UnicodeIdentifierStringToInt(identifier);
    if (!character) {
      error_ = kUnknownOrUnsupportedKeyIdentiferError;
      return false;
    }
  }

  bool flag = false;
  int flags = 0;
  if (prototype_event.key_code() != ui::VKEY_UNKNOWN)
    flags = prototype_event.flags();
  flags |= (args->GetBoolean(kAlt, &flag) && flag) ? ui::EF_ALT_DOWN : 0;
  flags |= (args->GetBoolean(kCtrl, &flag) && flag) ? ui::EF_CONTROL_DOWN : 0;
  flags |= (args->GetBoolean(kShift, &flag) && flag) ? ui::EF_SHIFT_DOWN : 0;
  if (args->GetBoolean(kMeta, &flag) && flag) {
    // Views does not have a Meta event flag, so return an error for now.
    error_ = kUnsupportedModifier;
    return false;
  }

  views::Widget* widget =
      chrome::GetTopLevelWidgetForBrowser(GetCurrentBrowser());
  if (!widget) {
    error_ = kNoValidRecipientError;
    return false;
  }

  ui::KeyEvent event(type, prototype_event.key_code(), flags);
  if (character) {
    event.set_character(character);
    event.set_unmodified_character(character);
  }

  views::InputMethod* ime = widget->GetInputMethod();
  if (ime) {
    ime->DispatchKeyEvent(event);
  } else if (!widget->OnKeyEvent(event)) {
    error_ = kKeyEventUnprocessedError;
    return false;
  }

  return true;
}
