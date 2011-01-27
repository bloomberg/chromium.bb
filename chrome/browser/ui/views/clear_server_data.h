// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CLEAR_SERVER_DATA_H_
#define CHROME_BROWSER_UI_VIEWS_CLEAR_SERVER_DATA_H_
#pragma once

#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/views/clear_data_view.h"
#include "chrome/browser/ui/views/confirm_message_box_dialog.h"
#include "views/controls/button/button.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/layout/grid_layout.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class ColumnSet;
class GridLayout;
class Label;
class Throbber;
class Window;
}

class ClearDataView;
class Profile;
class MessageLoop;

////////////////////////////////////////////////////////////////////////////////
//
// The ClearServerData class is responsible for drawing the UI controls of the
// dialog that allows the user to delete non-local data (e.g. Chrome Sync data)
//
////////////////////////////////////////////////////////////////////////////////
class ClearServerDataView : public views::View,
                            public views::ButtonListener,
                            public views::LinkController,
                            public ProfileSyncServiceObserver,
                            public ConfirmMessageBoxObserver {
 public:
  ClearServerDataView(Profile* profile, ClearDataView* clear_data_view);

  virtual ~ClearServerDataView();

  // Initialize the controls on the dialog.
  void Init();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overriden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // Disable clearing from this tab
  void SetAllowClear(bool allow);

 private:
  void InitControlLayout();
  void InitControlVisibility();

  void AddSpacing(views::GridLayout* layout,
                  bool related_follows);

  void AddWrappingLabelRow(views::GridLayout* layout,
                           views::Label* label,
                           int id,
                           bool related_follows);

  // Adds a new check-box as a child to the view.
  views::Checkbox* AddCheckbox(const std::wstring& text, bool checked);

  // Sets the controls on the UI to be enabled/disabled depending on whether we
  // have a delete operation in progress or not.
  void UpdateControlEnabledState();

  // Enables/disables the clear button as appropriate
  void UpdateClearButtonEnabledState(bool delete_in_progress);

  // Starts the process of deleting the browsing data depending on what the
  // user selected.
  void OnDelete();

  // ProfileSyncServiceObserver method.
  virtual void OnStateChanged();

  // ProfileSyncServiceObserver
  virtual void OnConfirmMessageAccept();
  virtual void OnConfirmMessageCancel();

  ClearDataView* clear_data_parent_window_;
  Profile* profile_;
  ProfileSyncService* sync_service_;
  bool allow_clear_;

  views::Label* flash_title_label_;
  views::Label* flash_description_label_;
  views::Label* chrome_sync_title_label_;
  views::Label* chrome_sync_description_label_;
  views::Label* dashboard_label_;
  views::Label* status_label_;
  views::Link* flash_link_;
  views::Link* dashboard_link_;
  views::NativeButton* clear_server_data_button_;
  views::Throbber* throbber_;

  DISALLOW_COPY_AND_ASSIGN(ClearServerDataView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CLEAR_SERVER_DATA_H_

