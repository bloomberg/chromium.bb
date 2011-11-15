// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

// ChromeOS user image options page UI handler.
class ChangePictureOptionsHandler : public OptionsPageUIHandler,
                                    public SelectFileDialog::Listener,
                                    public TakePhotoDialog::Delegate {
 public:
  ChangePictureOptionsHandler();
  virtual ~ChangePictureOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void Initialize() OVERRIDE;
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Sends list of available default images to the page.
  void SendAvailableImages();

  // Sends current selection to the page.
  void SendSelectedImage();

  // Sends the profile image to the page. If |should_select| is true then
  // the profile image element is selected.
  void SendProfileImage(const SkBitmap& image, bool should_select);

  // Starts profile image update and shows the last downloaded profile image,
  // if any, on the page. Shouldn't be called before |SendProfileImage|.
  void UpdateProfileImage();

  // Starts camera presence check.
  void CheckCameraPresence();

  // Updates UI with camera presence state.
  void SetCameraPresent(bool present);

  // Opens a file selection dialog to choose user image from file.
  void HandleChooseFile(const base::ListValue* args);

  // Opens the camera capture dialog.
  void HandleTakePhoto(const base::ListValue* args);

  // Gets the list of available user images and sends it to the page.
  void HandleGetAvailableImages(const base::ListValue* args);

  // Handles page shown event.
  void HandleOnPageShown(const base::ListValue* args);

  // Selects one of the available images as user's.
  void HandleSelectImage(const base::ListValue* args);

  // SelectFileDialog::Delegate implementation.
  virtual void FileSelected(
      const FilePath& path,
      int index, void* params) OVERRIDE;

  // TakePhotoDialog::Delegate implementation.
  virtual void OnPhotoAccepted(const SkBitmap& photo) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called when the camera presence check has been completed.
  void OnCameraPresenceCheckDone();

  // Returns handle to browser window or NULL if it can't be found.
  gfx::NativeWindow GetBrowserWindow() const;

  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Previous user image from camera/file and its data URL.
  SkBitmap previous_image_;
  std::string previous_image_data_url_;

  // Index of the previous user image.
  int previous_image_index_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<ChangePictureOptionsHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChangePictureOptionsHandler);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
