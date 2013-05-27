// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_AFTER_TRANSLATE_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_AFTER_TRANSLATE_INFOBAR_H_

#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/ui/views/infobars/translate_infobar_base.h"
#include "chrome/browser/ui/views/infobars/translate_language_menu_model.h"
#include "ui/views/controls/button/menu_button_listener.h"

class TranslateInfoBarDelegate;
namespace views {
class MenuButton;
}

class AfterTranslateInfoBar : public TranslateInfoBarBase,
                              public views::MenuButtonListener {
 public:
  AfterTranslateInfoBar(InfoBarService* owner,
                        TranslateInfoBarDelegate* delegate);

 private:
  virtual ~AfterTranslateInfoBar();

  // TranslateInfoBarBase:
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;
  virtual int ContentMinimumWidth() const OVERRIDE;

  // views::MenuButtonListener:
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  // The text displayed in the infobar is something like:
  // "Translated from <lang1> to <lang2> [more text in some languages]"
  // ...where <lang1> and <lang2> are comboboxes.  So the text is split in 3
  // chunks, each displayed in one of the labels below.
  views::Label* label_1_;
  views::Label* label_2_;
  views::Label* label_3_;

  views::MenuButton* original_language_menu_button_;
  views::MenuButton* target_language_menu_button_;
  views::LabelButton* revert_button_;
  views::MenuButton* options_menu_button_;

  scoped_ptr<TranslateLanguageMenuModel> original_language_menu_model_;
  scoped_ptr<TranslateLanguageMenuModel> target_language_menu_model_;
  OptionsMenuModel options_menu_model_;

  // True if the target language comes before the original one.
  bool swapped_language_buttons_;

  // True if the source language is expected to be determined by a server.
  bool autodetermined_source_language_;

  DISALLOW_COPY_AND_ASSIGN(AfterTranslateInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_AFTER_TRANSLATE_INFOBAR_H_
