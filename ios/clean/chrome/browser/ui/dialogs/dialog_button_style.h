// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_BUTTON_STYLE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_BUTTON_STYLE_H_

// Enum type to describe the style of buttons to use.
enum class DialogButtonStyle : char {
  DEFAULT,  // Uses default button styling.

  CANCEL,  // Uses styling to indicates that the button cancels the
           // current action, and leaves things unchanged.

  DESTRUCTIVE  // Uses styling that indicates that the button might change or
               // delete data.
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_DIALOGS_DIALOG_BUTTON_STYLE_H_
