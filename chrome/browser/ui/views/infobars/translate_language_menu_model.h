// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_LANGUAGE_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_LANGUAGE_MENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"

class TranslateInfoBarBase;
class TranslateInfoBarDelegate;
namespace views {
class MenuButton;
}

// A menu model that builds the contents of the language menus in the translate
// infobar. This menu has only one level (no submenus).
class TranslateLanguageMenuModel : public ui::SimpleMenuModel,
                                   public ui::SimpleMenuModel::Delegate {
 public:
  enum LanguageType {
    ORIGINAL,
    TARGET
  };

  TranslateLanguageMenuModel(LanguageType language_type,
                             TranslateInfoBarDelegate* infobar_delegate,
                             TranslateInfoBarBase* infobar,
                             views::MenuButton* button,
                             bool translate_on_change);
  virtual ~TranslateLanguageMenuModel();

  // ui::SimpleMenuModel::Delegate implementation:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  size_t GetLanguageIndex() const;

  LanguageType language_type_;
  TranslateInfoBarDelegate* infobar_delegate_;
  TranslateInfoBarBase* infobar_;
  views::MenuButton* button_;
  const bool translate_on_change_;

  DISALLOW_COPY_AND_ASSIGN(TranslateLanguageMenuModel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_TRANSLATE_LANGUAGE_MENU_MODEL_H_
