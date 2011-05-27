// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_EULA_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_EULA_SCREEN_ACTOR_H_
#pragma once

#include "chrome/browser/chromeos/login/eula_screen_actor.h"
#include "chrome/browser/chromeos/login/eula_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"

namespace chromeos {

// Views implementation of EulaScreenActor.
class ViewsEulaScreenActor : public ViewScreen<EulaView>,
                             public EulaScreenActor {
 public:
  explicit ViewsEulaScreenActor(ViewScreenDelegate* delegate);
  virtual ~ViewsEulaScreenActor();

  Delegate* screen() { return screen_; }

  // ViewScreen<EulaView> implementation.
  virtual EulaView* AllocateView();

  // EulaScreenActor implementation:
  virtual void PrepareToShow();
  virtual void Show();
  virtual void Hide();
  virtual bool IsUsageStatsChecked() const;
  virtual void SetDelegate(Delegate* delegate);

 private:
  Delegate* screen_;

  DISALLOW_COPY_AND_ASSIGN(ViewsEulaScreenActor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_EULA_SCREEN_ACTOR_H_
