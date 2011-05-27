// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/eula_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "googleurl/src/gurl.h"

namespace chromeos {

// Representation independent class that controls OOBE screen showing EULA
// to users.
class EulaScreen : public WizardScreen,
                   public EulaScreenActor::Delegate {
 public:
  explicit EulaScreen(WizardScreenDelegate* delegate);
  virtual ~EulaScreen();

  // WizardScreen implementation:
  virtual void Show();
  virtual void Hide();
  virtual gfx::Size GetScreenSize() const;

  // EulaScreenActor::Delegate implementation:
  virtual bool IsTpmEnabled() const;
  virtual GURL GetGoogleEulaUrl() const;
  virtual GURL GetOemEulaUrl() const;
  virtual void OnExit(bool accepted);
  virtual std::string* GetTpmPasswordStorage();
  virtual bool IsUsageStatsEnabled() const;

 private:
  // URL of the OEM EULA page (on disk).
  GURL oem_eula_page_;

  // TPM password local storage. By convention, we clear the password
  // from TPM as soon as we read it. We store it here locally until
  // EULA screen is closed.
  // TODO(glotov): Sanitize memory used to store password when
  // it's destroyed.
  std::string tpm_password_;

  scoped_ptr<EulaScreenActor> actor_;

  DISALLOW_COPY_AND_ASSIGN(EulaScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_H_
