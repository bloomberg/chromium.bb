// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_EXTERNAL_PROTOCOL_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_EXTERNAL_PROTOCOL_DIALOG_GTK_H_

#include "base/time.h"
#include "googleurl/src/gurl.h"

class TabContents;

typedef struct _GtkWidget GtkWidget;

class ExternalProtocolDialogGtk {
 public:
  explicit ExternalProtocolDialogGtk(const GURL& url);

 private:
  static void OnDialogResponseThunk(GtkWidget* widget,
                                    int response,
                                    ExternalProtocolDialogGtk* dialog) {
    dialog->OnDialogResponse(response);
  }
  void OnDialogResponse(int response);

  GtkWidget* dialog_;
  GtkWidget* checkbox_;
  GURL url_;
  base::Time creation_time_;
};

#endif  // CHROME_BROWSER_GTK_EXTERNAL_PROTOCOL_DIALOG_GTK_H_
