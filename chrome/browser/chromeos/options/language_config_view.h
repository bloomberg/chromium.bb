// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_

#include <string>

#include "chrome/browser/chromeos/cros/language_library.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view2.h"
#include "views/controls/table/table_view_observer.h"
#include "views/window/dialog_delegate.h"

namespace chromeos {

class LanguageCheckbox;
class LanguageHangulConfigView;
class PreferredLanguageTableModel;
// A dialog box for showing a password textfield.
class LanguageConfigView : public views::ButtonListener,
                           public views::DialogDelegate,
                           public views::TableViewObserver,
                           public views::View {
 public:
  LanguageConfigView();
  virtual ~LanguageConfigView();

  // views::ButtonListener overrides.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

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

  // views::TableViewObserver overrides:
  virtual void OnSelectionChanged();

 private:
  // Initializes UI.
  void Init();

  views::View* contents_;
  views::NativeButton* hangul_configure_button_;
  LanguageCheckbox* language_checkbox_;

  // A table for preferred languages and its model.
  views::TableView2* preferred_language_table_;
  scoped_ptr<PreferredLanguageTableModel> preferred_language_table_model_;
  DISALLOW_COPY_AND_ASSIGN(LanguageConfigView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_LANGUAGE_CONFIG_VIEW_H_
