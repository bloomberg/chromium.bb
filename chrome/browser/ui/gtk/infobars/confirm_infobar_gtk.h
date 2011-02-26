// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_CONFIRM_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_CONFIRM_INFOBAR_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;

// An infobar that shows a message, up to two optional buttons, and an optional,
// right-aligned link. This is commonly used to do things like:
// "Would you like to do X? [Yes] [No]                         _Learn More_ [x]"
class ConfirmInfoBarGtk : public InfoBar {
 public:
  explicit ConfirmInfoBarGtk(ConfirmInfoBarDelegate* delegate);

 private:
  virtual ~ConfirmInfoBarGtk();

  // Adds a button to the info bar by type. It will do nothing if the delegate
  // doesn't specify a button of the given type.
  void AddButton(ConfirmInfoBarDelegate::InfoBarButton type);

  CHROMEGTK_CALLBACK_0(ConfirmInfoBarGtk, void, OnOkButton);
  CHROMEGTK_CALLBACK_0(ConfirmInfoBarGtk, void, OnCancelButton);
  CHROMEGTK_CALLBACK_0(ConfirmInfoBarGtk, void, OnLinkClicked);

  GtkWidget* confirm_hbox_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_CONFIRM_INFOBAR_GTK_H_
