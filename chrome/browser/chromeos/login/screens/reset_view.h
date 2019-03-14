// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_VIEW_H_

#include "chrome/browser/chromeos/login/oobe_screen.h"

namespace chromeos {

namespace tpm_firmware_update {
enum class Mode;
}

class ResetScreen;

// Interface for dependency injection between ResetScreen and its actual
// representation, either views based or WebUI.
class ResetView {
 public:
  constexpr static OobeScreen kScreenId = OobeScreen::SCREEN_OOBE_RESET;

  virtual ~ResetView() {}

  virtual void Bind(ResetScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;

  enum class State {
    kRestartRequired = 0,
    kRevertPromise,
    kPowerwashProposal,
    kError,
  };

  virtual void SetIsRollbackAvailable(bool value) = 0;
  virtual void SetIsRollbackChecked(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateAvailable(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateChecked(bool value) = 0;
  virtual void SetIsTpmFirmwareUpdateEditable(bool value) = 0;
  virtual void SetTpmFirmwareUpdateMode(tpm_firmware_update::Mode value) = 0;
  virtual void SetIsConfirmational(bool value) = 0;
  virtual void SetIsOfficialBuild(bool value) = 0;
  virtual void SetScreenState(State value) = 0;

  virtual State GetScreenState() = 0;
  virtual tpm_firmware_update::Mode GetTpmFirmwareUpdateMode() = 0;
  virtual bool GetIsRollbackAvailable() = 0;
  virtual bool GetIsRollbackChecked() = 0;
  virtual bool GetIsTpmFirmwareUpdateChecked() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_VIEW_H_
