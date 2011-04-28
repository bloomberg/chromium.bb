// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_

#include "chrome/browser/ui/shell_dialogs.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "ui/gfx/native_widget_types.h"

class DictionaryValue;
class ListValue;

namespace chromeos {

// ChromeOS user image options page UI handler.
class ChangePictureOptionsHandler : public OptionsPageUIHandler,
                                    public SelectFileDialog::Listener {
 public:
  ChangePictureOptionsHandler();
  virtual ~ChangePictureOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  // Opens a file selection dialog to choose user image from file.
  void ChooseFile(const ListValue* args);

  // Opens the camera capture dialog.
  void TakePhoto(const ListValue* args);

  // Gets the list of available user images and sends it to the page.
  void GetAvailableImages(const ListValue* args);

  // Selects one of the available images as user's.
  void SelectImage(const ListValue* args);

  // SelectFileDialog::Delegate implementation.
  virtual void FileSelected(const FilePath& path, int index, void* params);

  // Returns handle to browser window or NULL if it can't be found.
  gfx::NativeWindow GetBrowserWindow() const;

  scoped_refptr<SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ChangePictureOptionsHandler);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
