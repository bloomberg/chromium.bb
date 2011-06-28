// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_PROFILE_DOM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_PROFILE_DOM_HANDLER_H_
#pragma once

#include "content/browser/webui/web_ui.h"

class RefCountedMemory;

// The handler for Javascript messages related to the "new profile" view.
class NewProfileDOMHandler : public WebUIMessageHandler {
 public:
  NewProfileDOMHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback for the "create" message - finishes creating a profile.
  void HandleCreate(const ListValue* args);

  // Callback for the "cancel" message - cancels creating a profile.
  void HandleCancel(const ListValue* args);

  // Callback for the "requestProfileInfo" message - sends profile info.
  void HandleRequestProfileInfo(const ListValue* args);

 private:
  void SendDefaultAvatarImages();
  void SendProfileInfo();

  DISALLOW_COPY_AND_ASSIGN(NewProfileDOMHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_PROFILE_DOM_HANDLER_H_
