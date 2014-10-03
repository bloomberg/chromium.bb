// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_SCREEN_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/eula_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "chromeos/tpm_password_fetcher.h"
#include "url/gurl.h"

namespace chromeos {

// Representation independent class that controls OOBE screen showing EULA
// to users.
class EulaScreen : public WizardScreen,
                   public EulaScreenActor::Delegate,
                   public TpmPasswordFetcherDelegate {
 public:
  EulaScreen(ScreenObserver* observer, EulaScreenActor* actor);
  virtual ~EulaScreen();

  // WizardScreen implementation:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual std::string GetName() const override;

  // EulaScreenActor::Delegate implementation:
  virtual GURL GetOemEulaUrl() const override;
  virtual void OnExit(bool accepted, bool usage_stats_enabled) override;
  virtual void InitiatePasswordFetch() override;
  virtual bool IsUsageStatsEnabled() const override;
  virtual void OnActorDestroyed(EulaScreenActor* actor) override;

  // TpmPasswordFetcherDelegate implementation:
  virtual void OnPasswordFetched(const std::string& tpm_password) override;

 private:
  // URL of the OEM EULA page (on disk).
  GURL oem_eula_page_;

  // TPM password local storage. By convention, we clear the password
  // from TPM as soon as we read it. We store it here locally until
  // EULA screen is closed.
  // TODO(glotov): Sanitize memory used to store password when
  // it's destroyed.
  std::string tpm_password_;

  EulaScreenActor* actor_;

  TpmPasswordFetcher password_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(EulaScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_SCREEN_H_
