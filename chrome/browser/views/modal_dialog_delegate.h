// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_MODAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_VIEWS_MODAL_DIALOG_DELEGATE_H_

#include <string>

#include "views/window/dialog_delegate.h"

namespace views {
class Window;
}

class ModalDialogDelegate : public views::DialogDelegate {
 public:
  virtual ~ModalDialogDelegate() {}
  // Methods called from AppModalDialog.
  virtual gfx::NativeWindow GetDialogRootWindow() = 0;
  virtual void ShowModalDialog();
  virtual void ActivateModalDialog();
  virtual void CloseModalDialog();
 protected:
  ModalDialogDelegate();

  // The dialog if it is currently visible.
  views::Window* dialog_;
};

#endif  // CHROME_BROWSER_VIEWS_MODAL_DIALOG_DELEGATE_H_

