// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_LINK_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_LINK_INFOBAR_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

class LinkInfoBarDelegate;

// An infobar that shows a string with an embedded link.
class LinkInfoBarGtk : public InfoBar {
 public:
  explicit LinkInfoBarGtk(LinkInfoBarDelegate* delegate);

 private:
  virtual ~LinkInfoBarGtk();

  CHROMEGTK_CALLBACK_0(LinkInfoBarGtk, void, OnLinkClicked);

  LinkInfoBarDelegate* GetDelegate();

  DISALLOW_COPY_AND_ASSIGN(LinkInfoBarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_LINK_INFOBAR_GTK_H_
