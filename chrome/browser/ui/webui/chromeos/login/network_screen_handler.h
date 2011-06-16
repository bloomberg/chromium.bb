// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#pragma once

#include "chrome/browser/chromeos/login/network_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/browser/webui/web_ui.h"

class ListValue;

namespace chromeos {

// WebUI implementation of NetworkScreenActor. It is used to interact with
// the welcome screen (part of the page) of the OOBE.
class NetworkScreenHandler : public NetworkScreenActor,
                             public OobeMessageHandler {
 public:
  NetworkScreenHandler();
  virtual ~NetworkScreenHandler();

  // NetworkScreenActor implementation:
  virtual void SetDelegate(NetworkScreenActor::Delegate* screen);
  virtual void PrepareToShow();
  virtual void Show();
  virtual void Hide();
  virtual void ShowError(const string16& message);
  virtual void ClearErrors();
  virtual void ShowConnectingStatus(bool connecting,
                                    const string16& network_id);
  virtual void EnableContinue(bool enabled);

  // OobeMessageHandler implementation:
  virtual void GetLocalizedStrings(DictionaryValue* localized_strings);
  virtual void Initialize();

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages();

 private:
  // Handles moving off the screen.
  void OnExit(const ListValue* args);

  NetworkScreenActor::Delegate* screen_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
