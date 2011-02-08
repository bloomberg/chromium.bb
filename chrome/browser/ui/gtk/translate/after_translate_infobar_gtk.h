// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TRANSLATE_AFTER_TRANSLATE_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_TRANSLATE_AFTER_TRANSLATE_INFOBAR_GTK_H_
#pragma once

#include "base/task.h"
#include "chrome/browser/ui/gtk/translate/translate_infobar_base_gtk.h"

class TranslateInfoBarDelegate;

class AfterTranslateInfoBar : public TranslateInfoBarBase {
 public:
  explicit AfterTranslateInfoBar(TranslateInfoBarDelegate* delegate);
  virtual ~AfterTranslateInfoBar();

  // Overridden from TranslateInfoBarBase:
  virtual void Init();

 protected:
  virtual bool ShowOptionsMenuButton() const;

 private:
  CHROMEGTK_CALLBACK_0(AfterTranslateInfoBar, void, OnOriginalLanguageModified);
  CHROMEGTK_CALLBACK_0(AfterTranslateInfoBar, void, OnTargetLanguageModified);
  CHROMEGTK_CALLBACK_0(AfterTranslateInfoBar, void, OnRevertPressed);

  // These methods set the original/target language on the
  // TranslateInfobarDelegate.
  void SetOriginalLanguage(size_t language_index);
  void SetTargetLanguage(size_t language_index);

  ScopedRunnableMethodFactory<AfterTranslateInfoBar> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(AfterTranslateInfoBar);
};

#endif  // CHROME_BROWSER_UI_GTK_TRANSLATE_AFTER_TRANSLATE_INFOBAR_GTK_H_
