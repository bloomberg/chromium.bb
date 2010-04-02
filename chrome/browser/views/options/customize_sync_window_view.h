// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_CUSTOMIZE_SYNC_WINDOW_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_CUSTOMIZE_SYNC_WINDOW_VIEW_H_

#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace views {
class Checkbox;
class Label;
}

class Profile;

class CustomizeSyncWindowView : public views::View,
                                public views::DialogDelegate {
 public:
  virtual ~CustomizeSyncWindowView() {}

  // Show the CustomizeSyncWindowView for the given profile. |parent_window| is
  // optional.
  // TODO(dantasse) when we make this window modal, |parent_window| will not be
  // optional.
  static void Show(gfx::NativeWindow parent_window, Profile* profile);

  // Simulate clicking the "OK" and "Cancel" buttons on the singleton dialog,
  // if it exists.
  static void ClickOk();
  static void ClickCancel();

  // views::View methods:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

  // views::DialogDelegate methods:
  virtual bool Accept();
  virtual int GetDialogButtons() const;
  virtual bool CanResize() const { return false; }
  virtual bool CanMaximize() const { return false; }
  virtual bool IsAlwaysOnTop() const { return false; }
  virtual bool HasAlwaysOnTopMenu() const { return false; }
  virtual std::wstring GetWindowTitle() const;
  // TODO(dantasse) make this gaia-dialog-modal by overriding IsModal() when
  // we replace the HTML sync setup wizard with more native dialogs.
  virtual void WindowClosing();
  virtual views::View* GetContentsView();

 private:
  explicit CustomizeSyncWindowView(Profile* profile);

  // Initialize the controls on the dialog.
  void Init();

  // Adds a new check-box as a child to the view.
  views::Checkbox* AddCheckbox(const std::wstring& text, bool checked);

  views::Label* description_label_;
  views::Checkbox* bookmarks_check_box_;
  views::Checkbox* preferences_check_box_;
  views::Checkbox* autofill_check_box_;
  views::Checkbox* themes_check_box_;

  Profile* profile_;

  // Singleton instance of this class.
  static CustomizeSyncWindowView* instance_;

  DISALLOW_COPY_AND_ASSIGN(CustomizeSyncWindowView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_CUSTOMIZE_SYNC_WINDOW_VIEW_H_
