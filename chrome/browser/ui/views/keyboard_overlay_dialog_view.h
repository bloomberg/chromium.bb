// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DIALOG_VIEW_H_
#pragma once

#include <set>

#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "views/window/window_delegate.h"

class BrowserView;

// A customized dialog view for the keyboard overlay.
class KeyboardOverlayDialogView : public HtmlDialogView {
 public:
  KeyboardOverlayDialogView(Profile* profile,
                            HtmlDialogUIDelegate* delegate,
                            BrowserView* parent_view);
  virtual ~KeyboardOverlayDialogView();

  // Initializes the contents of the dialog (the DOMView and the callbacks).
  void InitDialog();

  // Overridden from views::View:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // Shows the keyboard overlay.
  static void ShowDialog(gfx::NativeWindow owning_window, BrowserView* parent);

 private:
  // Returns true if |accelerator| is an accelerator for closing the dialog.
  bool IsCloseAccelerator(const views::Accelerator& accelerator);

  // Points to the view from which this dialog is created.
  BrowserView* parent_view_;

  // Contains accelerators for closing this dialog.
  std::set<views::Accelerator> close_accelerators_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_KEYBOARD_OVERLAY_DIALOG_VIEW_H_
