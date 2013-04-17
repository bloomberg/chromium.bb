// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_ALTERNATE_NAV_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_ALTERNATE_NAV_INFOBAR_GTK_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

class AlternateNavInfoBarDelegate;

// An infobar that shows a string with an embedded link.
class AlternateNavInfoBarGtk : public InfoBarGtk {
 public:
  AlternateNavInfoBarGtk(InfoBarService* owner,
                         AlternateNavInfoBarDelegate* delegate);

 private:
  virtual ~AlternateNavInfoBarGtk();

  CHROMEGTK_CALLBACK_0(AlternateNavInfoBarGtk, void, OnLinkClicked);

  AlternateNavInfoBarDelegate* GetDelegate();

  DISALLOW_COPY_AND_ASSIGN(AlternateNavInfoBarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_ALTERNATE_NAV_INFOBAR_GTK_H_
