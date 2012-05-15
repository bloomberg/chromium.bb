// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DIALOG_VIEW_H_
#pragma once

#include <set>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/web_dialog_web_contents_delegate.h"
#include "chrome/browser/ui/webui/web_dialog_ui.h"
#include "chrome/browser/ui/views/web_dialog_view.h"

namespace ui {
class Accelerator;
class AcceleratorTarget;
}

// A customized dialog view for the keyboard overlay.
class KeyboardOverlayDialogView : public WebDialogView {
 public:
  KeyboardOverlayDialogView(Profile* profile,
                            WebDialogDelegate* delegate,
                            AcceleratorTarget* target);
  virtual ~KeyboardOverlayDialogView();

  // Overridden from views::View:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // Shows the keyboard overlay.
  static void ShowDialog(ui::AcceleratorTarget* target);

 private:
  void RegisterDialogAccelerators();

  // Returns true if |accelerator| is an accelerator for closing the dialog.
  bool IsCloseAccelerator(const ui::Accelerator& accelerator);

  // Points to the view from which this dialog is created.
  AcceleratorTarget* target_;

  // Contains accelerators for closing this dialog.
  std::set<ui::Accelerator> close_accelerators_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DIALOG_VIEW_H_
