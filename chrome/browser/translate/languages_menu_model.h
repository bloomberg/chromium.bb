// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_LANGUAGES_MENU_MODEL_H_
#define CHROME_BROWSER_TRANSLATE_LANGUAGES_MENU_MODEL_H_
#pragma once

#include "ui/base/models/simple_menu_model.h"

class TranslateInfoBarDelegate;

// A menu model that builds the contents of the language menus in the translate
// infobar. This menu has only one level (no submenus).
class LanguagesMenuModel : public ui::SimpleMenuModel,
                           public ui::SimpleMenuModel::Delegate {
 public:
  enum LanguageType {
    ORIGINAL,
    TARGET
  };
  LanguagesMenuModel(TranslateInfoBarDelegate* translate_delegate,
                     LanguageType language_type);
  virtual ~LanguagesMenuModel();

  // ui::SimpleMenuModel::Delegate implementation:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  TranslateInfoBarDelegate* translate_infobar_delegate_;
  LanguageType language_type_;

  DISALLOW_COPY_AND_ASSIGN(LanguagesMenuModel);
};

#endif  // CHROME_BROWSER_TRANSLATE_LANGUAGES_MENU_MODEL_H_
