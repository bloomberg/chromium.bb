// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_BEFORE_TRANSLATE_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_BEFORE_TRANSLATE_INFOBAR_H_
#pragma once

#include "chrome/browser/translate/languages_menu_model.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/ui/views/infobars/translate_infobar_base.h"
#include "views/controls/menu/view_menu_delegate.h"

class InfoBarTextButton;
class TranslateInfoBarDelegate;
namespace views {
class Menu2;
class MenuButton;
}

class BeforeTranslateInfoBar : public TranslateInfoBarBase,
                               public views::ViewMenuDelegate {
 public:
  explicit BeforeTranslateInfoBar(TranslateInfoBarDelegate* delegate);

 private:
  virtual ~BeforeTranslateInfoBar();

  // TranslateInfoBarBase:
  virtual void Layout();
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);
  virtual void OriginalLanguageChanged();

  // Overridden from views::ViewMenuDelegate:
  virtual void RunMenu(View* source, const gfx::Point& pt);

  // Sets the text of the original language menu button to reflect the current
  // value from the delegate.
  void UpdateOriginalButtonText();

  // The text displayed in the infobar is something like:
  // "The page is in <lang>. Would you like to translate it?"
  // ...where <lang> is a combobox.  So the text is split in 2 chunks, each
  // displayed in one of the labels below.
  views::Label* label_1_;
  views::Label* label_2_;

  views::MenuButton* language_menu_button_;
  InfoBarTextButton* accept_button_;
  InfoBarTextButton* deny_button_;
  InfoBarTextButton* never_translate_button_;
  InfoBarTextButton* always_translate_button_;
  views::MenuButton* options_menu_button_;

  scoped_ptr<views::Menu2> languages_menu_;
  LanguagesMenuModel languages_menu_model_;

  scoped_ptr<views::Menu2> options_menu_;
  OptionsMenuModel options_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(BeforeTranslateInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_BEFORE_TRANSLATE_INFOBAR_H_
