// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/models/simple_menu_model.h"

// A SimpleMenuModel subclass that allows the system menu for a window to be
// wrapped.
class SystemMenuModel : public ui::SimpleMenuModel {
 public:
  explicit SystemMenuModel(ui::SimpleMenuModel::Delegate* delegate);
  virtual ~SystemMenuModel();

  // Overridden from ui::MenuModel:
  virtual int GetFirstItemIndex(gfx::NativeMenu native_menu) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemMenuModel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SYSTEM_MENU_MODEL_H_
