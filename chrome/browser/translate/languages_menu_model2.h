// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_LANGUAGES_MENU_MODEL2_H_
#define CHROME_BROWSER_TRANSLATE_LANGUAGES_MENU_MODEL2_H_

#include "app/menus/simple_menu_model.h"

class TranslateInfoBarDelegate2;
class String16;

// A menu model that builds the contents of the language menus in the translate
// infobar. This menu has only one level (no submenus).
class LanguagesMenuModel2 : public menus::SimpleMenuModel,
                            public menus::SimpleMenuModel::Delegate {
 public:
  enum LanguageType {
    ORIGINAL,
    TARGET
  };
  LanguagesMenuModel2(TranslateInfoBarDelegate2* translate_delegate,
                      LanguageType language_type);
  virtual ~LanguagesMenuModel2();

  // menus::SimpleMenuModel::Delegate implementation:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

 private:
  TranslateInfoBarDelegate2* translate_infobar_delegate_;
  LanguageType language_type_;

  DISALLOW_COPY_AND_ASSIGN(LanguagesMenuModel2);
};

#endif  // CHROME_BROWSER_TRANSLATE_LANGUAGES_MENU_MODEL2_H_
