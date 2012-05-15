// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/eula_screen_actor.h"
#include "chrome/browser/chromeos/login/tpm_password_fetcher.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "googleurl/src/gurl.h"

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
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;

  // EulaScreenActor::Delegate implementation:
  virtual bool IsTpmEnabled() const OVERRIDE;
  virtual GURL GetOemEulaUrl() const OVERRIDE;
  virtual void OnExit(bool accepted, bool is_usage_stats_checked) OVERRIDE;
  virtual void InitiatePasswordFetch() OVERRIDE;
  virtual bool IsUsageStatsEnabled() const OVERRIDE;
  virtual void OnActorDestroyed(EulaScreenActor* actor) OVERRIDE;

  // TpmPasswordFetcherDelegate implementation:
  virtual void OnPasswordFetched(const std::string& tpm_password) OVERRIDE;

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

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_H_
