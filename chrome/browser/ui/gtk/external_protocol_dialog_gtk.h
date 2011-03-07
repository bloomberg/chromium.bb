// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTERNAL_PROTOCOL_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTERNAL_PROTOCOL_DIALOG_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;

class ExternalProtocolDialogGtk {
 public:
  explicit ExternalProtocolDialogGtk(const GURL& url);
  ~ExternalProtocolDialogGtk();

 private:
  CHROMEGTK_CALLBACK_1(ExternalProtocolDialogGtk, void, OnResponse, int);

  GtkWidget* dialog_;
  GtkWidget* checkbox_;
  GURL url_;
  base::TimeTicks creation_time_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTERNAL_PROTOCOL_DIALOG_GTK_H_
