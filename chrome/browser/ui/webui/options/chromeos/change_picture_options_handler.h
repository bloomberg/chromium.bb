// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_

#include "chrome/browser/ui/shell_dialogs.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

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
  void ChooseFile(const ListValue* args);
  void TakePhoto(const ListValue* args);

  // SelectFileDialog::Delegate implementation.
  virtual void FileSelected(const FilePath& path, int index, void* params);

  scoped_refptr<SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ChangePictureOptionsHandler);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CHANGE_PICTURE_OPTIONS_HANDLER_H_
