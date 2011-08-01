// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_PROFILE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_PROFILE_HANDLER_H_
#pragma once

#include "content/browser/webui/web_ui.h"

class RefCountedMemory;

namespace base {
class ListValue;
}

// The handler for Javascript messages related to the "new profile" page.
class NewProfileHandler : public WebUIMessageHandler {
 public:
  NewProfileHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "create" message - finishes creating a profile.
  void HandleCreate(const base::ListValue* args);

  // Callback for the "cancel" message - cancels creating a profile.
  void HandleCancel(const base::ListValue* args);

  // Callback for the "requestProfileInfo" message - sends profile info.
  void HandleRequestProfileInfo(const base::ListValue* args);

 private:
  // Send the default avatar images to the page.
  void SendDefaultAvatarImages();

  // Send information about the profile to the page.
  void SendProfileInfo();

  DISALLOW_COPY_AND_ASSIGN(NewProfileHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_PROFILE_HANDLER_H_
