// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_VIRTUAL_KEYBOARD_VK_MESSAGE_HANDLER_H_
#define ATHENA_VIRTUAL_KEYBOARD_VK_MESSAGE_HANDLER_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace athena {

// Processes WebUI messages for chrome://keyboard.
class VKMessageHandler : public content::WebUIMessageHandler {
 public:
  VKMessageHandler();
  virtual ~VKMessageHandler();

 private:
  // WebUI message callbacks.
  void SendKeyEvent(const base::ListValue* params);
  void HideKeyboard(const base::ListValue* params);

  // content::WebUIMessageHandler:
  virtual void RegisterMessages() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(VKMessageHandler);
};

}  // namespace athena

#endif  // ATHENA_VIRTUAL_KEYBOARD_VK_MESSAGE_HANDLER_H_
