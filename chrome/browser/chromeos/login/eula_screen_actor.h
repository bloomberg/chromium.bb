// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_ACTOR_H_
#pragma once

#include <string>

#include "googleurl/src/gurl.h"
#include "ui/gfx/size.h"

namespace chromeos {

// Interface between eula screen and its representation, either WebUI or
// Views one. Note, do not forget to call OnActorDestroyed in the dtor.
class EulaScreenActor {
 public:
  // Allows us to get info from eula screen that we need.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns true if TPM is enabled.
    virtual bool IsTpmEnabled() const = 0;

    // Returns URL of the OEM EULA page that should be displayed using current
    // locale and manifest. Returns empty URL otherwise.
    virtual GURL GetOemEulaUrl() const = 0;

    // Called when screen is exited. |accepted| indicates if EULA was
    // accepted or not.
    virtual void OnExit(bool accepted, bool is_usage_stats_checked) = 0;

    // Initiate TPM password fetch. Will call actor's OnPasswordFetched() when
    // done.
    virtual void InitiatePasswordFetch() = 0;

    // Returns true if usage statistics reporting is enabled.
    virtual bool IsUsageStatsEnabled() const = 0;

    // This method is called, when actor is being destroyed. Note, if Delegate
    // is destroyed earlier then it has to call SetDelegate(NULL).
    virtual void OnActorDestroyed(EulaScreenActor* actor) = 0;
  };

  virtual ~EulaScreenActor() {}

  virtual void PrepareToShow() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void SetDelegate(Delegate* delegate) = 0;
  virtual void OnPasswordFetched(const std::string& tpm_password) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_ACTOR_H_
