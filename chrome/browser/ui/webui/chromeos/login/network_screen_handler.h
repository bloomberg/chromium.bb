// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#pragma once

#include "chrome/browser/chromeos/login/network_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/browser/webui/web_ui.h"
#include "ui/gfx/point.h"

class ListValue;

namespace views {
class Widget;
}

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
  // Handles change of the network control position.
  void HandleNetworkControlPosition(const ListValue* args);

  // Handles moving off the screen.
  void HandleOnExit(const ListValue* args);

  // Handles change of the language.
  void HandleOnLanguageChanged(const ListValue* args);

  // Handles change of the input method.
  void HandleOnInputMethodChanged(const ListValue* args);

  // Returns available languages. Caller gets the ownership. Note, it does
  // depend on the current locale.
  static ListValue* GetLanguageList();

  // Returns available input methods. Caller gets the ownership. Note, it does
  // depend on the current locale.
  static ListValue* GetInputMethods();

  // Creates network window or updates it's bounds.
  void CreateOrUpdateNetworkWindow();

  // Closes network window.
  void CloseNetworkWindow();

  // Window that contains network dropdown button.
  // TODO(nkostylev): Temporary solution till we have
  // RenderWidgetHostViewViews working.
  views::Widget* network_window_;

  NetworkScreenActor::Delegate* screen_;

  bool is_continue_enabled_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  // Position of the network control.
  gfx::Point network_control_pos_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
