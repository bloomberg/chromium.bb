// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_EASY_UNLOCK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_EASY_UNLOCK_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/signin/easy_unlock_service_observer.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

class EasyUnlockHandler : public OptionsPageUIHandler,
                          public EasyUnlockServiceObserver {
 public:
  EasyUnlockHandler();
  virtual ~EasyUnlockHandler();

  // OptionsPageUIHandler
  virtual void InitializeHandler() OVERRIDE;
  virtual void GetLocalizedValues(base::DictionaryValue* values) OVERRIDE;

  // WebUIMessageHandler
  virtual void RegisterMessages() OVERRIDE;

  // EasyUnlockServiceObserver
  virtual void OnTurnOffOperationStatusChanged() OVERRIDE;

 private:
  void SendTurnOffOperationStatus();

  // JS callbacks.
  void HandleGetTurnOffFlowStatus(const base::ListValue* args);
  void HandleRequestTurnOff(const base::ListValue* args);
  void HandlePageDismissed(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_EASY_UNLOCK_HANDLER_H_
