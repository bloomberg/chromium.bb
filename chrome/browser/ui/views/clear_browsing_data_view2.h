// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CLEAR_BROWSING_DATA_VIEW2_H_
#define CHROME_BROWSER_UI_VIEWS_CLEAR_BROWSING_DATA_VIEW2_H_
#pragma once

#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/ui/views/clear_data_view.h"
#include "ui/base/models/combobox_model.h"
#include "views/controls/button/button.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Label;
class Throbber;
class Window;
}

class ClearDataView;
class Profile;
class MessageLoop;

////////////////////////////////////////////////////////////////////////////////
//
// The ClearBrowsingDataView2 class is responsible for drawing the UI controls
// of the dialog that allows the user to select what to delete (history,
// downloads, etc).
//
// TODO(raz) Remove the 2 suffix when the mac/linux/chromeos versions are there
//
////////////////////////////////////////////////////////////////////////////////
class ClearBrowsingDataView2 : public views::View,
                               public views::ButtonListener,
                               public ui::ComboboxModel,
                               public views::Combobox::Listener,
                               public BrowsingDataRemover::Observer,
                               public views::LinkController {
 public:
  ClearBrowsingDataView2(Profile* profile, ClearDataView* clear_data_view);

  virtual ~ClearBrowsingDataView2(void);

  // Initialize the controls on the dialog.
  void Init();

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount();
  virtual string16 GetItemAt(int index);

  // Overridden from views::Combobox::Listener:
  virtual void ItemChanged(views::Combobox* sender, int prev_index,
                           int new_index);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overriden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Enable/disable clearing from this tab
  void SetAllowClear(bool allow);

 private:
  // Adds a new check-box as a child to the view.
  views::Checkbox* AddCheckbox(const std::wstring& text, bool checked);

  // Sets the controls on the UI to be enabled/disabled depending on whether we
  // have a delete operation in progress or not.
  void UpdateControlEnabledState();

  // Hand off control layout to layout manger
  void InitControlLayout();

  // Starts the process of deleting the browsing data depending on what the
  // user selected.
  void OnDelete();

  // Callback from BrowsingDataRemover. Closes the dialog.
  virtual void OnBrowsingDataRemoverDone();

  // Parent window, used for disabling close
  ClearDataView* clear_data_parent_window_;

  // Allows for disabling the clear button from outside this view
  bool allow_clear_;

  // UI elements
  views::View* throbber_view_;
  views::Throbber* throbber_;
  views::Label* status_label_;
  views::Label* delete_all_label_;
  views::Checkbox* del_history_checkbox_;
  views::Checkbox* del_downloads_checkbox_;
  views::Checkbox* del_cache_checkbox_;
  views::Checkbox* del_cookies_checkbox_;
  views::Checkbox* del_passwords_checkbox_;
  views::Checkbox* del_form_data_checkbox_;
  views::Label* time_period_label_;
  views::Combobox* time_period_combobox_;
  views::NativeButton* clear_browsing_data_button_;

  // Used to signal enabled/disabled state for controls in the UI.
  bool delete_in_progress_;

  Profile* profile_;

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  BrowsingDataRemover* remover_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowsingDataView2);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CLEAR_BROWSING_DATA_VIEW2_H_
