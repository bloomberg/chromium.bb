// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_NEW_WINDOW_DELEGATE_H_
#define ASH_NEW_WINDOW_DELEGATE_H_

namespace ash {

// A delegate class to create or open windows that are not a part of
// ash.
class NewWindowDelegate {
 public:
  virtual ~NewWindowDelegate() {}

  // Invoked when the user uses Ctrl+T to open a new tab.
  virtual void NewTab() = 0;

  // Invoked when the user uses Ctrl-N or Ctrl-Shift-N to open a new window.
  virtual void NewWindow(bool incognito) = 0;

  // Invoked when an accelerator is used to open the file manager.
  virtual void OpenFileManager() = 0;

  // Invoked when the user opens Crosh.
  virtual void OpenCrosh() = 0;

  // Invoked when the user uses Shift+Ctrl+T to restore the closed tab.
  virtual void RestoreTab() = 0;

  // Shows the keyboard shortcut overlay.
  virtual void ShowKeyboardOverlay() = 0;

  // Shows the task manager window.
  virtual void ShowTaskManager() = 0;

  // Opens the feedback page for "Report Issue".
  virtual void OpenFeedbackPage() = 0;
};

}  // namespace ash

#endif  // ASH_NEW_WINDOW_DELEGATE_H_
