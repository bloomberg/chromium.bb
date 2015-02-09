// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/screens/user_image_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

// WebUI implementation of UserImageScreenActor. It is used to interact
// with JS page part allowing user to select avatar.
class UserImageScreenHandler : public UserImageView, public BaseScreenHandler {
 public:
  UserImageScreenHandler();
  ~UserImageScreenHandler() override;

  // BaseScreenHandler implementation:
  void Initialize() override;
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // UserImageView implementation:
  void Bind(UserImageModel& model) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;
  void PrepareToShow() override;
  void HideCurtain() override;

 private:

  // Sends image data to the page.
  void HandleGetImages();

  // Screen ready to be shown.
  void HandleScreenReady();

  // Handles photo taken with WebRTC UI.
  void HandlePhotoTaken(const std::string& image_url);

  // Handles 'take-photo' button click.
  void HandleTakePhoto();

  // Handles 'discard-photo' button click.
  void HandleDiscardPhoto();

  // Handles clicking on default user image.
  void HandleSelectImage(const std::string& image_url,
                         const std::string& image_type,
                         bool is_user_selection);

  // Called when user accept the image closing the screen.
  void HandleImageAccepted();

  // Called when the user image screen has been loaded and shown.
  void HandleScreenShown();

  UserImageModel* model_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  // Keeps whether screen has loaded all default images and redy to be shown.
  bool is_ready_;

  base::Time screen_show_time_;

  DISALLOW_COPY_AND_ASSIGN(UserImageScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_HANDLER_H_
