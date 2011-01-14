// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTING_COMBO_MODEL_H_
#define CHROME_BROWSER_CONTENT_SETTING_COMBO_MODEL_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/common/content_settings.h"
#include "ui/base/models/combobox_model.h"

class ContentSettingComboModel : public ui::ComboboxModel {
 public:
  explicit ContentSettingComboModel(ContentSettingsType content_type);
  virtual ~ContentSettingComboModel();

  virtual int GetItemCount();
  virtual string16 GetItemAt(int index);

  ContentSetting SettingForIndex(int index);

  int IndexForSetting(ContentSetting);

 private:
  const ContentSettingsType content_type_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingComboModel);
};

#endif  // CHROME_BROWSER_CONTENT_SETTING_COMBO_MODEL_H_
