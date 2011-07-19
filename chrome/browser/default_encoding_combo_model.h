// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_ENCODING_COMBO_MODEL_H_
#define CHROME_BROWSER_DEFAULT_ENCODING_COMBO_MODEL_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/character_encoding.h"
#include "ui/base/models/combobox_model.h"

class Profile;

class DefaultEncodingComboboxModel : public ui::ComboboxModel {
 public:
  DefaultEncodingComboboxModel();
  virtual ~DefaultEncodingComboboxModel();

  // Overridden from ui::ComboboxModel.
  virtual int GetItemCount();

  virtual string16 GetItemAt(int index);

  std::string GetEncodingCharsetByIndex(int index);

  int GetSelectedEncodingIndex(Profile* profile);

 private:
  std::vector<CharacterEncoding::EncodingInfo> sorted_encoding_list_;

  DISALLOW_COPY_AND_ASSIGN(DefaultEncodingComboboxModel);
};

#endif  // CHROME_BROWSER_DEFAULT_ENCODING_COMBO_MODEL_H_
