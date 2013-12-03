// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_MESSAGE_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_MESSAGE_INFOBAR_H_

#include "chrome/browser/ui/views/infobars/translate_infobar_base.h"

class TranslateMessageInfoBar : public TranslateInfoBarBase {
 public:
  TranslateMessageInfoBar(InfoBarService* owner,
                          TranslateInfoBarDelegate* delegate);

 private:
  virtual ~TranslateMessageInfoBar();

  // TranslateInfoBarBase:
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;
  virtual int ContentMinimumWidth() const OVERRIDE;

  views::Label* label_;
  views::LabelButton* button_;

  DISALLOW_COPY_AND_ASSIGN(TranslateMessageInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_MESSAGE_INFOBAR_H_
