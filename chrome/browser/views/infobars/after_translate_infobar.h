// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFOBARS_AFTER_TRANSLATE_INFOBAR_H_
#define CHROME_BROWSER_VIEWS_INFOBARS_AFTER_TRANSLATE_INFOBAR_H_
#pragma once

#include "chrome/browser/translate/languages_menu_model.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/translate/translate_infobar_view.h"
#include "chrome/browser/views/infobars/translate_infobar_base.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/controls/menu/view_menu_delegate.h"

class InfoBarTextButton;
class TranslateInfoBarDelegate;

namespace views {
class Menu2;
class MenuButton;
}

class AfterTranslateInfoBar : public TranslateInfoBarBase,
                              public views::ViewMenuDelegate {
 public:
  explicit AfterTranslateInfoBar(TranslateInfoBarDelegate* delegate);
  virtual ~AfterTranslateInfoBar();

  // Overridden from views::View:
  virtual void Layout();

  // Overridden from TranslateInfoBarView:
  virtual void OriginalLanguageChanged();
  virtual void TargetLanguageChanged();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 protected:
  // Overridden from views::ViewMenuDelegate:
  virtual void RunMenu(View* source, const gfx::Point& pt);

 private:
  // Sets the text of the original or target language menu buttons to reflect
  // the current value from the delegate.
  void UpdateLanguageButtonText(LanguagesMenuModel::LanguageType language);

  // The text displayed in the infobar is something like:
  // "Translated from <lang1> to <lang2>"
  // Where <lang1> and <lang2> are displayed in a combobox.
  // So the text is split in 3 chunks, each one displayed in one of the label
  // below.
  views::Label* label_1_;
  views::Label* label_2_;
  views::Label* label_3_;

  views::MenuButton* original_language_menu_button_;
  views::MenuButton* target_language_menu_button_;
  views::MenuButton* options_menu_button_;
  InfoBarTextButton* revert_button_;

  scoped_ptr<views::Menu2> original_language_menu_;
  LanguagesMenuModel original_language_menu_model_;

  scoped_ptr<views::Menu2> target_language_menu_;
  LanguagesMenuModel target_language_menu_model_;

  scoped_ptr<views::Menu2> options_menu_;
  OptionsMenuModel options_menu_model_;

  // True if the target language comes before the original one.
  bool swapped_language_buttons_;

  DISALLOW_COPY_AND_ASSIGN(AfterTranslateInfoBar);
};

#endif  // CHROME_BROWSER_VIEWS_INFOBARS_AFTER_TRANSLATE_INFOBAR_H_
