// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_HANDLER_H_

#include "chrome/browser/chromeos/login/user_image_screen_actor.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class ListValue;
}  // namespace base

namespace chromeos {

// WebUI implementation of UserImageScreenActor. It is used to interact
// with JS page part allowing user to select avatar.
class UserImageScreenHandler : public UserImageScreenActor,
                               public BaseScreenHandler,
                               public TakePhotoDialog::Delegate {
 public:
  UserImageScreenHandler();
  virtual ~UserImageScreenHandler();

  // BaseScreenHandler implementation:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // UserImageScreenActor implementation:
  virtual void SetDelegate(
      UserImageScreenActor::Delegate* screen) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void PrepareToShow() OVERRIDE;
  virtual void SelectImage(int index) OVERRIDE;
  virtual void UpdateVideoFrame(const SkBitmap& frame) OVERRIDE;
  virtual void ShowCameraError() OVERRIDE;
  virtual void ShowCameraInitializing() OVERRIDE;
  virtual bool IsCapturing() const OVERRIDE;
  virtual void AddProfileImage(const SkBitmap& image) OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // TakePhotoDialog::Delegate implementation.
  virtual void OnPhotoAccepted(const SkBitmap& photo) OVERRIDE;

 private:
  // Opens the camera capture dialog.
  void HandleTakePhoto(const base::ListValue* args);

  // Handles clicking on default user image.
  void HandleSelectImage(const base::ListValue* args);

  // Called when user accept the image closing the screen.
  void HandleImageAccepted(const base::ListValue* args);

  UserImageScreenActor::Delegate* screen_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  // Index of the selected default user image. -1 if the photo is taken.
  int selected_image_;

  // Last user photo, if taken.
  SkBitmap user_photo_;

  // Downloaded user profile picture if present.
  SkBitmap profile_picture_;

  // Its data URL.
  std::string profile_picture_data_url_;

  DISALLOW_COPY_AND_ASSIGN(UserImageScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_HANDLER_H_
