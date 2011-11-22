// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_TRANSLATE_MESSAGE_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_TRANSLATE_MESSAGE_INFOBAR_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/gtk/infobars/translate_infobar_base_gtk.h"

class TranslateInfoBarDelegate;

class TranslateMessageInfoBar : public TranslateInfoBarBase {
 public:
  TranslateMessageInfoBar(InfoBarTabHelper* owner,
                          TranslateInfoBarDelegate* delegate);
  virtual ~TranslateMessageInfoBar();

  // Overridden from TranslateInfoBarBase:
  virtual void Init() OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_0(TranslateMessageInfoBar, void, OnButtonPressed);

  DISALLOW_COPY_AND_ASSIGN(TranslateMessageInfoBar);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_TRANSLATE_MESSAGE_INFOBAR_GTK_H_
