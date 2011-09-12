// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_

#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
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
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Opens a file selection dialog to choose user image from file.
  void HandleChooseFile(const base::ListValue* args);

  // Opens the camera capture dialog.
  void HandleTakePhoto(const base::ListValue* args);

  // Gets the list of available user images and sends it to the page.
  void HandleGetAvailableImages(const base::ListValue* args);

  // Gets URL of the currently selected image.
  void HandleGetSelectedImage(const base::ListValue* args);

  // Selects one of the available images as user's.
  void HandleSelectImage(const base::ListValue* args);

  // SelectFileDialog::Delegate implementation.
  virtual void FileSelected(
      const FilePath& path,
      int index, void* params) OVERRIDE;

  // TakePhotoDialog::Delegate implementation.
  virtual void OnPhotoAccepted(const SkBitmap& photo) OVERRIDE;

  // Returns handle to browser window or NULL if it can't be found.
  gfx::NativeWindow GetBrowserWindow() const;

  scoped_refptr<SelectFileDialog> select_file_dialog_;
  SkBitmap previous_image_;

  DISALLOW_COPY_AND_ASSIGN(ChangePictureOptionsHandler);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
