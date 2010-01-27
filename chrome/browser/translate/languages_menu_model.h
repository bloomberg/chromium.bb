// Copyright (c) 2010 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_LANGUAGES_MENU_MODEL_H_
#define CHROME_BROWSER_TRANSLATE_LANGUAGES_MENU_MODEL_H_

#include "app/menus/simple_menu_model.h"

class TranslateInfoBarDelegate;

// A menu model that builds the contents of the language menus in the translate
// infobar, i.e. the original and target languages.
class LanguagesMenuModel : public menus::SimpleMenuModel {
 public:
  explicit LanguagesMenuModel(menus::SimpleMenuModel::Delegate* menu_delegate,
      TranslateInfoBarDelegate* translate_delegate, bool original_language);
  virtual ~LanguagesMenuModel();

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguagesMenuModel);
};

#endif  // CHROME_BROWSER_TRANSLATE_LANGUAGES_MENU_MODEL_H_
