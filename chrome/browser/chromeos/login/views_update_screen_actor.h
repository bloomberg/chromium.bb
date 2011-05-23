// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_UPDATE_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_UPDATE_SCREEN_ACTOR_H_
#pragma once

#include "chrome/browser/chromeos/login/update_screen_actor.h"
#include "chrome/browser/chromeos/login/update_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"

namespace chromeos {

class ViewsUpdateScreenActor : public DefaultViewScreen<chromeos::UpdateView>,
                               public UpdateScreenActor {
 public:
  ViewsUpdateScreenActor(WizardScreenDelegate* delegate,
                         int width,
                         int height);

  virtual ~ViewsUpdateScreenActor() {}

  virtual void Show();
  virtual void Hide();
  virtual void ShowManualRebootInfo();
  virtual void SetProgress(int progress);
  virtual void ShowCurtain(bool enable);

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsUpdateScreenActor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_UPDATE_SCREEN_ACTOR_H_
