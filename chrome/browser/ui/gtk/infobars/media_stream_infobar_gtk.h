// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_MEDIA_STREAM_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_MEDIA_STREAM_INFOBAR_GTK_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

class MediaStreamDevicesMenuModel;
class MediaStreamInfoBarDelegate;

class MediaStreamInfoBarGtk : public InfoBarGtk {
 public:
  MediaStreamInfoBarGtk(InfoBarTabHelper* owner,
                        MediaStreamInfoBarDelegate* delegate);
  virtual ~MediaStreamInfoBarGtk();

  // Initializes the infobar widgets. Should be called after the object has been
  // created.
  virtual void Init();

 private:
  CHROMEGTK_CALLBACK_0(MediaStreamInfoBarGtk, void, OnAllowButton);
  CHROMEGTK_CALLBACK_0(MediaStreamInfoBarGtk, void, OnDenyButton);
  CHROMEGTK_CALLBACK_0(MediaStreamInfoBarGtk, void, OnDevicesClicked);

  MediaStreamInfoBarDelegate* GetDelegate();

  DISALLOW_COPY_AND_ASSIGN(MediaStreamInfoBarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_MEDIA_STREAM_INFOBAR_GTK_H_
