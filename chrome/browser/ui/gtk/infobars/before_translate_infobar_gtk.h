// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_BEFORE_TRANSLATE_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_BEFORE_TRANSLATE_INFOBAR_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/gtk/infobars/translate_infobar_base_gtk.h"

class TranslateInfoBarDelegate;

class BeforeTranslateInfoBar : public TranslateInfoBarBase {
 public:
  BeforeTranslateInfoBar(InfoBarTabHelper* owner,
                         TranslateInfoBarDelegate* delegate);
  virtual ~BeforeTranslateInfoBar();

  // Overridden from TranslateInfoBarBase:
  virtual void Init() OVERRIDE;

 protected:
  virtual bool ShowOptionsMenuButton() const OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_0(BeforeTranslateInfoBar, void, OnLanguageModified);
  CHROMEGTK_CALLBACK_0(BeforeTranslateInfoBar, void, OnAcceptPressed);
  CHROMEGTK_CALLBACK_0(BeforeTranslateInfoBar, void, OnDenyPressed);
  CHROMEGTK_CALLBACK_0(BeforeTranslateInfoBar, void, OnNeverTranslatePressed);
  CHROMEGTK_CALLBACK_0(BeforeTranslateInfoBar, void, OnAlwaysTranslatePressed);

  DISALLOW_COPY_AND_ASSIGN(BeforeTranslateInfoBar);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_BEFORE_TRANSLATE_INFOBAR_GTK_H_
