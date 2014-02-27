// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_LANGUAGE_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_TRANSLATE_LANGUAGE_COMBOBOX_MODEL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ui/base/models/combobox_model.h"

class TranslateBubbleModel;

// The model for the combobox to select a language. This is used for Translate
// user interface to select language.
class LanguageComboboxModel : public ui::ComboboxModel {
 public:
  LanguageComboboxModel(int default_index,
                        TranslateBubbleModel* model);
  virtual ~LanguageComboboxModel();

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;
  virtual int GetDefaultIndex() const OVERRIDE;

 private:
  const int default_index_;
  TranslateBubbleModel* model_;

  DISALLOW_COPY_AND_ASSIGN(LanguageComboboxModel);
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_LANGUAGE_COMBOBOX_MODEL_H_
