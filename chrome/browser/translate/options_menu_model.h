// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_OPTIONS_MENU_MODEL_H_
#define CHROME_BROWSER_TRANSLATE_OPTIONS_MENU_MODEL_H_

#include "app/menus/simple_menu_model.h"

class TranslateInfoBarDelegate;
class TranslateInfoBarDelegate2;
class String16;

// A menu model that builds the contents of the options menu in the translate
// infobar. This menu has only one level (no submenus).
class OptionsMenuModel : public menus::SimpleMenuModel {
 public:
  // TODO(jcivelli): remove this constructor once we have migrated to the new
  //                 translate infobars.
  OptionsMenuModel(menus::SimpleMenuModel::Delegate* menu_delegate,
                   TranslateInfoBarDelegate* translate_delegate);
  OptionsMenuModel(menus::SimpleMenuModel::Delegate* menu_delegate,
                   TranslateInfoBarDelegate2* translate_delegate);
  virtual ~OptionsMenuModel();

 private:
  void Init(const string16& original_language, const string16& target_language);

  DISALLOW_COPY_AND_ASSIGN(OptionsMenuModel);
};

#endif  // CHROME_BROWSER_TRANSLATE_OPTIONS_MENU_MODEL_H_
