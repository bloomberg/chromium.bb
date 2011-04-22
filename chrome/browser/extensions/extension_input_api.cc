// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_input_api.h"

#include <string>

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/key_identifier_conversion_views.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "views/events/event.h"
#include "views/ime/input_method.h"
#include "views/widget/widget.h"

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

ui::EventType GetTypeFromString(const std::string& type) {
  if (type == kKeyDown) {
    return ui::ET_KEY_PRESSED;
  } else if (type == kKeyUp) {
    return ui::ET_KEY_RELEASED;
  }
  return ui::ET_UNKNOWN;
}

}  // namespace

void InputFunction::Run() {
  SendResponse(RunImpl());
}

views::Widget* SendKeyboardEventInputFunction::GetTopLevelWidget() {
  Browser* browser = GetCurrentBrowser();
  if (!browser)
    return NULL;

  BrowserWindow* window = browser->window();
  if (!window)
    return NULL;

  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      window->GetNativeHandle());
  return browser_view ? browser_view->GetWidget() : NULL;
}

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

  const views::KeyEvent& prototype_event =
      KeyEventFromKeyIdentifier(identifier);
  if (prototype_event.key_code() == ui::VKEY_UNKNOWN) {
    error_ = kUnknownOrUnsupportedKeyIdentiferError;
    return false;
  }

  bool flag = false;
  int flags = prototype_event.flags();
  flags |= (args->GetBoolean(kAlt, &flag) && flag) ? ui::EF_ALT_DOWN : 0;
  flags |= (args->GetBoolean(kCtrl, &flag) && flag) ? ui::EF_CONTROL_DOWN : 0;
  flags |= (args->GetBoolean(kShift, &flag) && flag) ? ui::EF_SHIFT_DOWN : 0;
  if (args->GetBoolean(kMeta, &flag) && flag) {
    // Views does not have a Meta event flag, so return an error for now.
    error_ = kUnsupportedModifier;
    return false;
  }

  views::Widget* widget = GetTopLevelWidget();
  if (!widget) {
    error_ = kNoValidRecipientError;
    return false;
  }

  views::KeyEvent event(type, prototype_event.key_code(), flags);
  views::InputMethod* ime = widget->GetInputMethod();
  if (ime) {
    ime->DispatchKeyEvent(event);
  } else if (!widget->OnKeyEvent(event)) {
    error_ = kKeyEventUnprocessedError;
    return false;
  }

  return true;
}
