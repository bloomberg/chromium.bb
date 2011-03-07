// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CLEAR_DATA_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CLEAR_DATA_VIEW_H_
#pragma once

#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/ui/views/clear_browsing_data_view2.h"
#include "chrome/browser/ui/views/clear_server_data.h"
#include "views/controls/button/button.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class Label;
class Throbber;
class Window;
}

class ClearBrowsingDataView2;
class ClearServerDataView;
class Profile;
class MessageLoop;

////////////////////////////////////////////////////////////////////////////////
//
// The ClearDataView class is responsible for drawing the window that allows
// the user to select what to delete (history, downloads, etc).  It has tabs
// separating "local" data from "other" (e.g. server) data
//
////////////////////////////////////////////////////////////////////////////////
class ClearDataView : public views::View,
                      public views::DialogDelegate {
 public:
  explicit ClearDataView(Profile* profile);
  virtual ~ClearDataView(void) {}

  // Disallow the window closing while clearing either server or browsing
  // data.  After clear completes, close the window.
  void StartClearingBrowsingData();
  void StopClearingBrowsingData();

  void StartClearingServerData();
  void SucceededClearingServerData();
  void FailedClearingServerData();

  // Initialize the controls on the dialog.
  void Init();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from views::DialogDelegate:
  virtual int GetDefaultDialogButton() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual int GetDialogButtons() const;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual views::View* GetContentsView();
  virtual bool GetSizeExtraViewHeightToButtons() { return true; }
  virtual views::View* GetInitiallyFocusedView();

 private:
  // Sets the controls on the UI to be enabled/disabled depending on whether we
  // have a delete operation in progress or not.
  void UpdateControlEnabledState();

  // Currently clearing
  bool clearing_data_;

  views::TabbedPane* tabs_;
  ClearServerDataView* clear_server_data_tab_;
  ClearBrowsingDataView2* clear_browsing_data_tab_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ClearDataView);
};

static const int kDialogPadding = 7;

#endif  // CHROME_BROWSER_UI_VIEWS_CLEAR_DATA_VIEW_H_

