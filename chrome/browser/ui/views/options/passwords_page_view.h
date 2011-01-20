// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIONS_PASSWORDS_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIONS_PASSWORDS_PAGE_VIEW_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/views/confirm_message_box_dialog.h"
#include "chrome/browser/ui/views/options/options_page_view.h"
#include "ui/base/models/table_model.h"
#include "ui/base/text/text_elider.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view.h"
#include "views/controls/table/table_view_observer.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"
#include "webkit/glue/password_form.h"

///////////////////////////////////////////////////////////////////////////////
// PasswordTableModelObserver
// An observer interface to notify change of row count in a table model. This
// allow the container view of TableView(i.e. PasswordsPageView and
// ExceptionsPageView), to be notified of row count changes directly
// from the TableModel. We have two different observers in
// PasswordsTableModel, namely TableModelObserver and
// PasswordsTableModelObserver, rather than adding this event to
// TableModelObserver because only container view of
// PasswordsTableModel cares about this event.
class PasswordsTableModelObserver {
 public:
  virtual void OnRowCountChanged(size_t rows) = 0;
};

///////////////////////////////////////////////////////////////////////////////
// MultiLabelButtons
// A button that can have 2 different labels set on it and for which the
// preferred size is the size of the widest string.
class MultiLabelButtons : public views::NativeButton {
 public:
  MultiLabelButtons(views::ButtonListener* listener,
                    const std::wstring& label,
                    const std::wstring& alt_label);

  virtual gfx::Size GetPreferredSize();

 private:
  std::wstring label_;
  std::wstring alt_label_;
  gfx::Size pref_size_;

  DISALLOW_COPY_AND_ASSIGN(MultiLabelButtons);
};

///////////////////////////////////////////////////////////////////////////////
// PasswordsTableModel
class PasswordsTableModel : public TableModel,
                            public PasswordStoreConsumer {
 public:
  explicit PasswordsTableModel(Profile* profile);
  virtual ~PasswordsTableModel();

  // TableModel methods.
  virtual int RowCount() OVERRIDE;
  virtual string16 GetText(int row, int column) OVERRIDE;
  virtual int CompareValues(int row1, int row2, int column_id) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;

  // Delete the PasswordForm at specified row from the database (and remove
  // from view).
  void ForgetAndRemoveSignon(int row);

  // Delete all saved signons for the active profile (via web data service),
  // and clear the view.
  void ForgetAndRemoveAllSignons();

  // PasswordStoreConsumer implementation.
  virtual void OnPasswordStoreRequestDone(
      int handle, const std::vector<webkit_glue::PasswordForm*>& result);

  // Request saved logins data.
  void GetAllSavedLoginsForProfile();

  // Return the PasswordForm at the specified index.
  webkit_glue::PasswordForm* GetPasswordFormAt(int row);

  // Set the observer who concerns about how many rows are in the table.
  void set_row_count_observer(PasswordsTableModelObserver* observer) {
    row_count_observer_ = observer;
  }

 protected:
  // Wraps the PasswordForm from the database and caches the display URL for
  // quick sorting.
  struct PasswordRow {
    PasswordRow(const ui::SortedDisplayURL& url,
                webkit_glue::PasswordForm* password_form)
        : display_url(url), form(password_form) {
    }

    // Contains the URL that is displayed along with the
    ui::SortedDisplayURL display_url;

    // The underlying PasswordForm. We own this.
    scoped_ptr<webkit_glue::PasswordForm> form;
  };

  // The password store associated with the currently active profile.
  PasswordStore* password_store() {
    return profile_->GetPasswordStore(Profile::EXPLICIT_ACCESS);
  }

  // The TableView observing this model.
  ui::TableModelObserver* observer_;

  // Dispatching row count events specific to this password manager table model
  // to this observer.
  PasswordsTableModelObserver* row_count_observer_;

  // Handle to any pending PasswordStore login lookup query.
  int pending_login_query_;

  // The set of passwords we're showing.
  typedef std::vector<PasswordRow*> PasswordRows;
  PasswordRows saved_signons_;
  STLElementDeleter<PasswordRows> saved_signons_cleanup_;

  Profile* profile_;

 private:
  // Cancel any pending login query involving a callback.
  void CancelLoginsQuery();

  DISALLOW_COPY_AND_ASSIGN(PasswordsTableModel);
};

///////////////////////////////////////////////////////////////////////////////
// PasswordsPageView
class PasswordsPageView : public OptionsPageView,
                          public views::TableViewObserver,
                          public views::ButtonListener,
                          public PasswordsTableModelObserver,
                          public ConfirmMessageBoxObserver {
 public:
  explicit PasswordsPageView(Profile* profile);
  virtual ~PasswordsPageView();

  // views::TableViewObserverImplementation.
  virtual void OnSelectionChanged();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // PasswordsTableModelObserver implementation.
  virtual void OnRowCountChanged(size_t rows);

  // ConfirmMessageBoxObserver implementation.
  virtual void OnConfirmMessageAccept();

 protected:
  virtual void InitControlLayout();

 private:
  // Helper to configure our buttons and labels.
  void SetupButtonsAndLabels();

  // Helper to configure our table view.
  void SetupTable();

  // Helper that hides the password.
  void HidePassword();

  // Handles changes to the observed preferences and updates the UI.
  void NotifyPrefChanged(const std::string* pref_name);

  PasswordsTableModel table_model_;
  views::TableView* table_view_;

  // The buttons and labels.
  MultiLabelButtons show_button_;
  views::NativeButton remove_button_;
  views::NativeButton remove_all_button_;
  views::Label password_label_;
  webkit_glue::PasswordForm* current_selected_password_;

  // Tracks the preference that controls whether showing passwords is allowed.
  BooleanPrefMember allow_show_passwords_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIONS_PASSWORDS_PAGE_VIEW_H_
