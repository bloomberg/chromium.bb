// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_ONE_CLICK_SIGNIN_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_ONE_CLICK_SIGNIN_DIALOG_GTK_H_
#pragma once

#include "chrome/browser/ui/sync/one_click_signin_dialog.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

// Displays the one-click signin confirmation dialog (as a
// window-modal dialog) and starts one-click signin if the user
// accepts.
class OneClickSigninDialogGtk {
 public:
  // Deletes self on close.  The given callback will be called if the
  // user confirms the dialog.
  OneClickSigninDialogGtk(GtkWindow* parent_window,
                          const OneClickAcceptCallback& accept_callback);

  void SendResponseForTest(int response_id);
  void SetUseDefaultSettingsForTest(bool use_default_settings);

 private:
  ~OneClickSigninDialogGtk();

  CHROMEGTK_CALLBACK_1(OneClickSigninDialogGtk, void, OnResponse, int);

  GtkWidget* dialog_;
  GtkWidget* use_default_settings_checkbox_;
  const OneClickAcceptCallback accept_callback_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_ONE_CLICK_SIGNIN_DIALOG_GTK_H_
