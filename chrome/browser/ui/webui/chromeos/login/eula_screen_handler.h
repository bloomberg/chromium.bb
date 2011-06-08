// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/eula_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/browser/webui/web_ui.h"

class ListValue;

namespace chromeos {

// WebUI implementation of EulaScreenActor. It is used to interact
// with the eula part of the JS page.
class EulaScreenHandler : public EulaScreenActor,
                          public OobeMessageHandler {
 public:
  EulaScreenHandler();
  virtual ~EulaScreenHandler();

  // EulaScreenActor implementation:
  virtual void PrepareToShow();
  virtual void Show();
  virtual void Hide();
  virtual void SetDelegate(Delegate* delegate);

  // OobeMessageHandler implementation:
  virtual void GetLocalizedSettings(DictionaryValue* localized_strings);
  virtual void Initialize();

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages();

 private:
  // JS messages handlers.
  void OnPageReady(const ListValue* args);
  void OnExit(const ListValue* args);

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(EulaScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_EULA_SCREEN_HANDLER_H_
