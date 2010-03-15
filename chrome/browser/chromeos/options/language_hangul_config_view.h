// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_HANGUL_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_HANGUL_CONFIG_VIEW_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/window/dialog_delegate.h"

namespace chromeos {

class HangulKeyboardComboboxModel;
// A dialog box for showing a password textfield.
class LanguageHangulConfigView : public views::Combobox::Listener,
                                 public views::DialogDelegate,
                                 public views::View {
 public:
  LanguageHangulConfigView();
  virtual ~LanguageHangulConfigView();

  // views::Combobox::Listener overrides.
  virtual void ItemChanged(views::Combobox* sender,
                           int prev_index,
                           int new_index);

  // views::DialogDelegate overrides.
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }
  virtual std::wstring GetWindowTitle() const;

  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  // Initializes UI.
  void Init();
  // Updates the hangul keyboard combobox.
  void UpdateHangulKeyboardCombobox();

  // GConf config path names for the Korean IME.
  static const char kHangulSection[];
  static const char kHangulKeyboardConfigName[];

  views::View* contents_;

  // A combobox for Hangul keyboard layouts and its model.
  views::Combobox* hangul_keyboard_combobox_;
  scoped_ptr<HangulKeyboardComboboxModel> hangul_keyboard_combobox_model_;

  DISALLOW_COPY_AND_ASSIGN(LanguageHangulConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_HANGUL_CONFIG_VIEW_H_
