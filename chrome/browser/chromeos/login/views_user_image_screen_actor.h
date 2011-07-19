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
  virtual UserImageView* AllocateView();

  // UserImageScreenActor implementation:
  virtual void PrepareToShow();
  virtual void Show();
  virtual void Hide();
  virtual void SetDelegate(UserImageScreenActor::Delegate* screen);
  virtual void SelectImage(int index);
  virtual void UpdateVideoFrame(const SkBitmap& frame);
  virtual void ShowCameraError();
  virtual void ShowCameraInitializing();
  virtual bool IsCapturing() const;

  // UserImageView::Delegate implementation:
  virtual void StartCamera();
  virtual void StopCamera();
  virtual void OnPhotoTaken(const SkBitmap& image);
  virtual void OnDefaultImageSelected(int index);

 private:
  UserImageScreenActor::Delegate* screen_;

  DISALLOW_COPY_AND_ASSIGN(ViewsUserImageScreenActor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_USER_IMAGE_SCREEN_ACTOR_H_

