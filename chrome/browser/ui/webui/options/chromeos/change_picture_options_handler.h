// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {
namespace options {

// ChromeOS user image options page UI handler.
class ChangePictureOptionsHandler : public ::options::OptionsPageUIHandler,
                                    public ui::SelectFileDialog::Listener,
                                    public ImageDecoder::Delegate {
 public:
  ChangePictureOptionsHandler();
  virtual ~ChangePictureOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Sends list of available default images to the page.
  void SendDefaultImages();

  // Sends current selection to the page.
  void SendSelectedImage();

  // Sends the profile image to the page. If |should_select| is true then
  // the profile image element is selected.
  void SendProfileImage(const gfx::ImageSkia& image, bool should_select);

  // Starts profile image update and shows the last downloaded profile image,
  // if any, on the page. Shouldn't be called before |SendProfileImage|.
  void UpdateProfileImage();

  // Sends previous user image to the page.
  void SendOldImage(const std::string& image_url);

  // Starts camera presence check.
  void CheckCameraPresence();

  // Updates UI with camera presence state.
  void SetCameraPresent(bool present);

  // Opens a file selection dialog to choose user image from file.
  void HandleChooseFile(const base::ListValue* args);

  // Handles photo taken with WebRTC UI.
  void HandlePhotoTaken(const base::ListValue* args);

  // Handles camera presence check request.
  void HandleCheckCameraPresence(const base::ListValue* args);

  // Gets the list of available user images and sends it to the page.
  void HandleGetAvailableImages(const base::ListValue* args);

  // Handles page initialized event.
  void HandlePageInitialized(const base::ListValue* args);

  // Handles page shown event.
  void HandlePageShown(const base::ListValue* args);

  // Selects one of the available images as user's.
  void HandleSelectImage(const base::ListValue* args);

  // SelectFileDialog::Delegate implementation.
  virtual void FileSelected(
      const FilePath& path,
      int index, void* params) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called when the camera presence check has been completed.
  void OnCameraPresenceCheckDone();

  // Sets user image to photo taken from camera.
  void SetImageFromCamera(const gfx::ImageSkia& photo);

  // Returns handle to browser window or NULL if it can't be found.
  gfx::NativeWindow GetBrowserWindow() const;

  // Overriden from ImageDecoder::Delegate:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // Previous user image from camera/file and its data URL.
  gfx::ImageSkia previous_image_;
  std::string previous_image_url_;

  // Index of the previous user image.
  int previous_image_index_;

  // Last user photo, if taken.
  gfx::ImageSkia user_photo_;

  // Data URL for |user_photo_|.
  std::string user_photo_data_url_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<ChangePictureOptionsHandler> weak_factory_;

  // Last ImageDecoder instance used to decode an image blob received by
  // HandlePhotoTaken.
  scoped_refptr<ImageDecoder> image_decoder_;

  DISALLOW_COPY_AND_ASSIGN(ChangePictureOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
