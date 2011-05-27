// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_ACTOR_H_
#pragma once

#include "googleurl/src/gurl.h"
#include "ui/gfx/size.h"

namespace chromeos {

// Interface between eula screen and its representation, either WebUI or
// Views one.
class EulaScreenActor {
 public:
  // Allows us to get info from eula screen that we need.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns true if TPM is enabled.
    virtual bool IsTpmEnabled() const = 0;

    // Returns URL of the Google EULA page.
    virtual GURL GetGoogleEulaUrl() const = 0;

    // Returns URL of the OEM EULA page that should be displayed using current
    // locale and manifest. Returns empty URL otherwise.
    virtual GURL GetOemEulaUrl() const = 0;

    // Called when screen is exited. |accepted| indicates if EULA was
    // accepted or not.
    virtual void OnExit(bool accepted) = 0;

    // Returns the string where TPM password will be stored.
    virtual std::string* GetTpmPasswordStorage() = 0;

    // Returns true if usage statistics reporting is enabled.
    virtual bool IsUsageStatsEnabled() const = 0;
  };

  virtual ~EulaScreenActor() {}

  virtual void PrepareToShow() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;

  // Returns true if user decided to report usage statistics.
  virtual bool IsUsageStatsChecked() const = 0;
  virtual void SetDelegate(Delegate* delegate) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EULA_SCREEN_ACTOR_H_
