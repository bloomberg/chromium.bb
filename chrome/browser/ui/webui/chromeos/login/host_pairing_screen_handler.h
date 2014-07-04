// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HOST_PAIRING_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HOST_PAIRING_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/host_pairing_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class HostPairingScreenHandler : public HostPairingScreenActor,
                                 public BaseScreenHandler {
 public:
  HostPairingScreenHandler();
  virtual ~HostPairingScreenHandler();

 private:
  // Overridden from BaseScreenHandler:
  virtual void Initialize() OVERRIDE;
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;

  // Overridden from content::WebUIMessageHandler:
  virtual void RegisterMessages() OVERRIDE;

  // Overridden from HostPairingScreenActor:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetDelegate(Delegate* delegate) OVERRIDE;
  virtual void OnContextChanged(const base::DictionaryValue& diff) OVERRIDE;

  HostPairingScreenActor::Delegate* delegate_;
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(HostPairingScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HOST_PAIRING_SCREEN_HANDLER_H_
