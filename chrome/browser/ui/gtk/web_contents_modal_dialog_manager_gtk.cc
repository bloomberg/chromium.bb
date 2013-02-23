// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/native_web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"

namespace {

class NativeWebContentsModalDialogManagerGtk
    : public NativeWebContentsModalDialogManager {
 public:
  NativeWebContentsModalDialogManagerGtk() {
  }

  virtual ~NativeWebContentsModalDialogManagerGtk() {
  }

  // NativeWebContentsModalDialogManager overrides
  virtual void ManageDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }

  virtual void ShowDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GetConstrainedWindowGtk(dialog)->ShowWebContentsModalDialog();
  }

  virtual void CloseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GetConstrainedWindowGtk(dialog)->CloseWebContentsModalDialog();
  }

  virtual void FocusDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GetConstrainedWindowGtk(dialog)->FocusWebContentsModalDialog();
  }

  virtual void PulseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GetConstrainedWindowGtk(dialog)->PulseWebContentsModalDialog();
  }

 private:
  ConstrainedWindowGtk* GetConstrainedWindowGtk(
      NativeWebContentsModalDialog dialog) {
    gpointer constrained_window_gtk =
        g_object_get_data(G_OBJECT(dialog), "ConstrainedWindowGtk");
    DCHECK(constrained_window_gtk);
    return static_cast<ConstrainedWindowGtk*>(constrained_window_gtk);
  }

  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManagerGtk);
};

}  // namespace

NativeWebContentsModalDialogManager*
    WebContentsModalDialogManager::CreateNativeManager(
        NativeWebContentsModalDialogManagerDelegate* native_delegate) {
  return new NativeWebContentsModalDialogManagerGtk;
}
