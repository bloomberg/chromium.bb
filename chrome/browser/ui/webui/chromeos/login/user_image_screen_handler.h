// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/chromeos/login/user_image_screen_actor.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class ListValue;
}  // namespace base

namespace chromeos {

// WebUI implementation of UserImageScreenActor. It is used to interact
// with JS page part allowing user to select avatar.
class UserImageScreenHandler : public UserImageScreenActor,
                               public BaseScreenHandler,
                               public ImageDecoder::Delegate {
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
  virtual void CheckCameraPresence() OVERRIDE;
  virtual void AddProfileImage(const gfx::ImageSkia& image) OVERRIDE;
  virtual void OnProfileImageAbsent() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Sends profile image as a data URL to the page.
  void SendProfileImage(const std::string& data_url);

  // Sends image data to the page.
  void HandleGetImages(const base::ListValue* args);

  // Handles photo taken with WebRTC UI.
  void HandlePhotoTaken(const base::ListValue* args);

  // Handles camera presence check request.
  void HandleCheckCameraPresence(const base::ListValue* args);

  // Handles clicking on default user image.
  void HandleSelectImage(const base::ListValue* args);

  // Called when user accept the image closing the screen.
  void HandleImageAccepted(const base::ListValue* args);

  // Called when the user image screen has been loaded and shown.
  void HandleScreenShown(const base::ListValue* args);

  // Called when the camera presence check has been completed.
  void OnCameraPresenceCheckDone();

  // Overriden from ImageDecoder::Delegate:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;


  UserImageScreenActor::Delegate* screen_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  // Index of the selected user image.
  int selected_image_;

  // Last user photo, if taken.
  gfx::ImageSkia user_photo_;

  // Last ImageDecoder instance used to decode an image blob received by
  // HandlePhotoTaken.
  scoped_refptr<ImageDecoder> image_decoder_;

  // If |true|, decoded photo should be immediately accepeted (i.e., both
  // HandleTakePhoto and HandleImageAccepted have already been called but we're
  // still waiting for  photo image decoding to finish.
  bool accept_photo_after_decoding_;

  // Data URL for |user_photo_|.
  std::string user_photo_data_url_;

  // Data URL of the profile picture;
  std::string profile_picture_data_url_;

  // True if user has no custom profile picture.
  bool profile_picture_absent_;

  base::WeakPtrFactory<UserImageScreenHandler> weak_factory_;

  base::Time screen_show_time_;

  DISALLOW_COPY_AND_ASSIGN(UserImageScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_USER_IMAGE_SCREEN_HANDLER_H_
