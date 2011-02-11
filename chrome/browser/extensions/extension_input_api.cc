// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_input_api.h"

#include <string>

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/key_identifier_conversion_views.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "views/events/event.h"
#include "views/widget/root_view.h"

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

views::RootView* SendKeyboardEventInputFunction::GetRootView() {
  Browser* browser = GetCurrentBrowser();
  if (!browser)
    return NULL;

  BrowserWindow* window = browser->window();
  if (!window)
    return NULL;

  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      window->GetNativeHandle());
  if (!browser_view)
    return NULL;

  return browser_view->GetRootView();
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

  int flags = prototype_event.flags();
  bool alt = false;
  if (args->GetBoolean(kAlt, &alt))
    flags |= alt ? ui::EF_ALT_DOWN : 0;
  bool ctrl = false;
  if (args->GetBoolean(kCtrl, &ctrl))
    flags |= ctrl ? ui::EF_CONTROL_DOWN : 0;
  bool shift = false;
  if (args->GetBoolean(kShift, &shift))
    flags |= shift ? ui::EF_SHIFT_DOWN : 0;
  bool meta = false;
  if (args->GetBoolean(kMeta, &meta)) {
    // Views does not have a Meta event flag, so return an error for now.
    if (meta) {
      error_ = kUnsupportedModifier;
      return false;
    }
  }

  views::RootView* root_view = GetRootView();
  if (!root_view) {
    error_ = kNoValidRecipientError;
    return false;
  }

  views::KeyEvent event(type, prototype_event.key_code(), flags);
  if (!root_view->ProcessKeyEvent(event)) {
    error_ = kKeyEventUnprocessedError;
    return false;
  }

  return true;
}
