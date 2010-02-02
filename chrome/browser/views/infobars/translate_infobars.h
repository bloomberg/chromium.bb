// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFOBARS_TRANSLATE_INFOBARS_H_
#define CHROME_BROWSER_VIEWS_INFOBARS_TRANSLATE_INFOBARS_H_

#include "app/menus/simple_menu_model.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "chrome/browser/views/infobars/infobars.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/controls/menu/menu_2.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
class MenuButton;
}
class LanguagesMenuModel;
class OptionsMenuModel;
class TranslateTextButton;

// This file contains implementations for infobars for the Translate feature.

class TranslateInfoBar : public InfoBar,
                         public views::ViewMenuDelegate,
                         public menus::SimpleMenuModel::Delegate {
 public:
  TranslateInfoBar(TranslateInfoBarDelegate* delegate, bool before_translate,
      int label_id);
  virtual ~TranslateInfoBar();

  // Overridden from views::View:
  virtual void Layout();

  // Overridden from views::MenuDelegate:
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
      menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

 protected:
  views::MenuButton* CreateMenuButton(int menu_id, const std::wstring& label);
  gfx::Point DetermineMenuPositionAndAlignment(views::MenuButton* menu_button,
      views::Menu2::Alignment* alignment);
  void OnLanguageModified(views::MenuButton* menu_button,
      int new_language_index);
  TranslateInfoBarDelegate* GetDelegate() const;

  // Overridden from InfoBar:
  virtual int GetAvailableWidth() const;

  // Returns the leftmost position that subclasses can position their controls.
  virtual int GetAvailableX() const;

  virtual views::MenuButton* target_language_menu_button() const {
    return NULL;
  }

  virtual int GetSpacingAfterFirstLanguageButton() const {
    return InfoBar::kButtonInLabelSpacing;
  }

 private:
  bool before_translate_;
  views::ImageView* icon_;
  views::Label* label_1_;
  views::Label* label_2_;
  views::Label* label_3_;
  views::MenuButton* original_language_menu_button_;
  views::MenuButton* options_menu_button_;

  scoped_ptr<LanguagesMenuModel> original_language_menu_model_;
  scoped_ptr<OptionsMenuModel> options_menu_model_;

  scoped_ptr<views::Menu2> original_language_menu_menu_;
  scoped_ptr<views::Menu2> options_menu_menu_;

  // This is true if language placeholders in label have been swapped.
  bool swapped_language_placeholders_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBar);
};

class BeforeTranslateInfoBar : public TranslateInfoBar {
 public:
  explicit BeforeTranslateInfoBar(BeforeTranslateInfoBarDelegate* delegate);
  virtual ~BeforeTranslateInfoBar();

  // Overridden from views::View:
  virtual void Layout();

 protected:
  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overriden from TranslateInfoBar:
  virtual int GetSpacingAfterFirstLanguageButton() const {
    return 10;
  }

  // Overridden from InfoBar:
  virtual int GetAvailableWidth() const;

 private:
  TranslateTextButton* accept_button_;
  TranslateTextButton* deny_button_;

  DISALLOW_COPY_AND_ASSIGN(BeforeTranslateInfoBar);
};

class AfterTranslateInfoBar : public TranslateInfoBar {
 public:
  explicit AfterTranslateInfoBar(AfterTranslateInfoBarDelegate* delegate);
  virtual ~AfterTranslateInfoBar();

  // Overridden from views::MenuDelegate:
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Overridden from menus::SimpleMenuModel::Delegate:
  virtual void ExecuteCommand(int command_id);

 protected:
  // Overridden from InfoBar:
  virtual int GetAvailableWidth() const;

  virtual views::MenuButton* target_language_menu_button() const {
    return target_language_menu_button_;
  }

 private:
  views::MenuButton* target_language_menu_button_;

  scoped_ptr<LanguagesMenuModel> target_language_menu_model_;

  scoped_ptr<views::Menu2> target_language_menu_menu_;

  DISALLOW_COPY_AND_ASSIGN(AfterTranslateInfoBar);
};

#endif  // CHROME_BROWSER_VIEWS_INFOBARS_TRANSLATE_INFOBARS_H_
