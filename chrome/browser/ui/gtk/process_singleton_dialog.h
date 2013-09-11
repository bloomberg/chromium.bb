// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_PROCESS_SINGLETON_DIALOG_H_
#define CHROME_BROWSER_UI_GTK_PROCESS_SINGLETON_DIALOG_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;

// Displays an error to the user when the ProcessSingleton cannot acquire the
// lock.  This runs the message loop itself as the browser message loop has not
// started by that point in the startup process.
class ProcessSingletonDialog {
 public:
  // Shows the dialog, and returns once the dialog has been closed.
  static bool ShowAndRun(const std::string& message,
                         const std::string& relaunch_text);

  int GetResponseId() const { return response_id_; }

 private:
  ProcessSingletonDialog(const std::string& message,
                         const std::string& relaunch_text);

  CHROMEGTK_CALLBACK_1(ProcessSingletonDialog, void, OnResponse, int);

  GtkWidget* dialog_;
  int response_id_;

  DISALLOW_COPY_AND_ASSIGN(ProcessSingletonDialog);
};

#endif  // CHROME_BROWSER_UI_GTK_PROCESS_SINGLETON_DIALOG_H_
