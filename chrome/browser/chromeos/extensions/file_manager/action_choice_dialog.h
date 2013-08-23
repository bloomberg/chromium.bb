// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides the action choice dialog. The dialog is shown when an
// removable drive, such as an SD card, is inserted, to ask the user what to
// do with the drive (ex. open the file manager, or open some other app).

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_ACTION_CHOICE_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_ACTION_CHOICE_DIALOG_H_

namespace base {
class FilePath;
}

namespace file_manager {
namespace util {

// Opens an action choice dialog for a removable drive.
// One of the actions is opening the File Manager. Passes |advanced_mode|
// flag to the dialog. If it is enabled, then auto-choice gets disabled.
void OpenActionChoiceDialog(const base::FilePath& file_path,
                            bool advanced_mode);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_ACTION_CHOICE_DIALOG_H_
