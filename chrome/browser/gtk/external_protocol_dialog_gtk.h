// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_EXTERNAL_PROTOCOL_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_EXTERNAL_PROTOCOL_DIALOG_GTK_H_

#include "app/gtk_signal.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

class TabContents;

typedef struct _GtkWidget GtkWidget;

class ExternalProtocolDialogGtk {
 public:
  explicit ExternalProtocolDialogGtk(const GURL& url);

 private:
  CHROMEGTK_CALLBACK_1(ExternalProtocolDialogGtk, void, OnDialogResponse, int);

  GtkWidget* dialog_;
  GtkWidget* checkbox_;
  GURL url_;
  base::TimeTicks creation_time_;
};

#endif  // CHROME_BROWSER_GTK_EXTERNAL_PROTOCOL_DIALOG_GTK_H_
