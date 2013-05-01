// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_HANDLER_H_

#include <string>

#include "content/public/browser/web_ui_message_handler.h"
#include "webkit/plugins/webplugininfo.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

// The handler class for TranslateInternals page operations.
class TranslateInternalsHandler : public content::WebUIMessageHandler {
 public:
  TranslateInternalsHandler() {}
  virtual ~TranslateInternalsHandler() {}

  // content::WebUIMessageHandler methods:
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Handles the Javascript message 'removePrefItem'. This message is sent
  // when UI requests to remove an item in the preference.
  void OnRemovePrefItem(const base::ListValue* args);

  // Handles the Javascript message 'requestInfo'. This message is sent
  // when UI needs to show information concerned with the translation.
  // For now, this returns only prefs to Javascript.
  // |args| is not used.
  void OnRequestInfo(const base::ListValue* args);

  // Sends a messsage to Javascript.
  void SendMessageToJs(const std::string& message, const base::Value& value);

  // Sends the current preference to Javascript.
  void SendPrefsToJs();

  DISALLOW_COPY_AND_ASSIGN(TranslateInternalsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_HANDLER_H_
