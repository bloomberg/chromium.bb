// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_input_api.h"

#include <string>

#include "app/keyboard_code_conversion.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "views/event.h"
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
const char kNoValidRecipientError[] = "No valid recipient for event.";
const char kKeyEventUnprocessedError[] = "Event was not handled.";

views::Event::EventType GetTypeFromString(const std::string& type) {
  if (type == kKeyDown) {
    return views::Event::ET_KEY_PRESSED;
  } else if (type == kKeyUp) {
    return views::Event::ET_KEY_RELEASED;
  }
  return views::Event::ET_UNKNOWN;
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
  views::Event::EventType type = GetTypeFromString(type_name);
  if (type == views::Event::ET_UNKNOWN) {
    error_ = kUnknownEventTypeError;
    return false;
  }

  std::string identifier;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(kKeyIdentifier, &identifier));
  app::KeyboardCode code = app::KeyCodeFromKeyIdentifier(identifier);
  if (code == app::VKEY_UNKNOWN) {
    error_ = kUnknownOrUnsupportedKeyIdentiferError;
    return false;
  }

  int flags = 0;
  bool alt = false;
  if (args->GetBoolean(kAlt, &alt))
    flags |= alt ? WebKit::WebInputEvent::AltKey : 0;
  bool ctrl = false;
  if (args->GetBoolean(kCtrl, &ctrl))
    flags |= ctrl ? WebKit::WebInputEvent::ControlKey : 0;
  bool meta = false;
  if (args->GetBoolean(kMeta, &meta))
    flags |= meta ? WebKit::WebInputEvent::MetaKey : 0;
  bool shift = false;
  if (args->GetBoolean(kShift, &shift))
    flags |= shift ? WebKit::WebInputEvent::ShiftKey : 0;

  views::RootView* root_view = GetRootView();
  if (!root_view) {
    error_ = kNoValidRecipientError;
    return false;
  }

  views::KeyEvent event(type, code, flags, 0, 0);
  if (!root_view->ProcessKeyEvent(event)) {
    error_ = kKeyEventUnprocessedError;
    return false;
  }

  return true;
}
