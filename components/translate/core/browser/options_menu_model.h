// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_OPTIONS_MENU_MODEL_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_OPTIONS_MENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"

namespace translate {

class TranslateInfoBarDelegate;

// A menu model that builds the contents of the options menu in the translate
// infobar. This menu has only one level (no submenus).
class OptionsMenuModel : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  // Command IDs of the items in this menu; exposed for testing.
  enum CommandID {
    ABOUT_TRANSLATE = 0,
    ALWAYS_TRANSLATE,
    NEVER_TRANSLATE_LANGUAGE,
    NEVER_TRANSLATE_SITE,
    REPORT_BAD_DETECTION
  };

  explicit OptionsMenuModel(TranslateInfoBarDelegate* translate_delegate);
  virtual ~OptionsMenuModel();

  // ui::SimpleMenuModel::Delegate implementation:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  TranslateInfoBarDelegate* translate_infobar_delegate_;

  DISALLOW_COPY_AND_ASSIGN(OptionsMenuModel);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_OPTIONS_MENU_MODEL_H_
