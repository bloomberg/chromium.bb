// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_USER_IMAGE_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_USER_IMAGE_SCREEN_ACTOR_H_
#pragma once

#include "chrome/browser/chromeos/login/user_image_screen_actor.h"
#include "chrome/browser/chromeos/login/user_image_view.h"
#include "chrome/browser/chromeos/login/view_screen.h"

namespace chromeos {

// Views implementation of UserImageScreenActor.
class ViewsUserImageScreenActor : public ViewScreen<UserImageView>,
                                  public UserImageScreenActor,
                                  public UserImageView::Delegate {
 public:
  explicit ViewsUserImageScreenActor(ViewScreenDelegate* delegate);
  virtual ~ViewsUserImageScreenActor();

  UserImageScreenActor::Delegate* screen() { return screen_; }

  // ViewScreen<UserImageView> implementation.
  virtual UserImageView* AllocateView() OVERRIDE;

  // UserImageScreenActor implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetDelegate(UserImageScreenActor::Delegate* screen) OVERRIDE;
  virtual void SelectImage(int index) OVERRIDE;
  virtual void UpdateVideoFrame(const SkBitmap& frame) OVERRIDE;
  virtual void ShowCameraError() OVERRIDE;
  virtual void ShowCameraInitializing() OVERRIDE;
  virtual void CheckCameraPresence() OVERRIDE;
  virtual bool IsCapturing() const OVERRIDE;

  // UserImageView::Delegate implementation:
  virtual void StartCamera() OVERRIDE;
  virtual void StopCamera() OVERRIDE;
  virtual void OnPhotoTaken(const SkBitmap& image) OVERRIDE;
  virtual void OnDefaultImageSelected(int index) OVERRIDE;

 private:
  UserImageScreenActor::Delegate* screen_;

  DISALLOW_COPY_AND_ASSIGN(ViewsUserImageScreenActor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_USER_IMAGE_SCREEN_ACTOR_H_
