// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MISSING_SYSTEM_FILE_DIALOG_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_MISSING_SYSTEM_FILE_DIALOG_WIN_H_

#include "base/basictypes.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

// A modal dialog to notify the user about missing system files.  This dialog
// has a "learn more" link that automatically closes the dialog and navigates
// to an appropriate page in the Help Center.
class MissingSystemFileDialog : public views::DialogDelegateView,
                                public views::LinkListener {
 public:
  // Shows dialog as a child window of |parent|.
  static void ShowDialog(gfx::NativeWindow parent, Profile* profile);

 private:
  explicit MissingSystemFileDialog(Profile* profile);
  virtual ~MissingSystemFileDialog();

  // views::DialogDelegateView:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Whether we have shown this error dialog.  We want to show the dialog per
  // browser process.
  static bool dialog_shown_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(MissingSystemFileDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_MISSING_SYSTEM_FILE_DIALOG_WIN_H_
