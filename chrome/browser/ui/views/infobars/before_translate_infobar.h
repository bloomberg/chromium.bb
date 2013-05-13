// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_BEFORE_TRANSLATE_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_BEFORE_TRANSLATE_INFOBAR_H_

#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/ui/views/infobars/translate_infobar_base.h"
#include "chrome/browser/ui/views/infobars/translate_language_menu_model.h"
#include "ui/views/controls/button/menu_button_listener.h"

class TranslateInfoBarDelegate;
namespace views {
class MenuButton;
}

class BeforeTranslateInfoBar : public TranslateInfoBarBase,
                               public views::MenuButtonListener {
 public:
  BeforeTranslateInfoBar(InfoBarService* owner,
                         TranslateInfoBarDelegate* delegate);

 private:
  virtual ~BeforeTranslateInfoBar();

  // TranslateInfoBarBase:
  virtual void Layout() OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual int ContentMinimumWidth() const OVERRIDE;

  // views::MenuButtonListener:
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  // The text displayed in the infobar is something like:
  // "The page is in <lang>. Would you like to translate it?"
  // ...where <lang> is a combobox.  So the text is split in 2 chunks, each
  // displayed in one of the labels below.
  views::Label* label_1_;
  views::Label* label_2_;

  views::MenuButton* language_menu_button_;
  views::LabelButton* accept_button_;
  views::LabelButton* deny_button_;
  views::LabelButton* never_translate_button_;
  views::LabelButton* always_translate_button_;
  views::MenuButton* options_menu_button_;

  scoped_ptr<TranslateLanguageMenuModel> language_menu_model_;
  OptionsMenuModel options_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(BeforeTranslateInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_BEFORE_TRANSLATE_INFOBAR_H_
