// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_UPDATE_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_UPDATE_SCREEN_ACTOR_H_
#pragma once

#include "chrome/browser/chromeos/login/update_screen_actor.h"
#include "chrome/browser/chromeos/login/update_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"

namespace chromeos {

class ViewsUpdateScreenActor : public DefaultViewScreen<UpdateView>,
                               public UpdateScreenActor {
 public:
  explicit ViewsUpdateScreenActor(ViewScreenDelegate* delegate);
  virtual ~ViewsUpdateScreenActor();

  virtual void SetDelegate(UpdateScreenActor::Delegate* screen) OVERRIDE;
  virtual void PrepareToShow();
  virtual void Show();
  virtual void Hide();
  virtual void ShowManualRebootInfo();
  virtual void SetProgress(int progress);
  virtual void ShowCurtain(bool enable);
  virtual void ShowPreparingUpdatesInfo(bool visible);

 private:
  UpdateScreenActor::Delegate* screen_;

  DISALLOW_COPY_AND_ASSIGN(ViewsUpdateScreenActor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_UPDATE_SCREEN_ACTOR_H_
