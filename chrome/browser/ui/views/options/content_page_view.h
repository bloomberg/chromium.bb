// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H_
#pragma once

#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/views/confirm_message_box_dialog.h"
#include "chrome/browser/ui/views/options/options_page_view.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/view.h"

namespace views {
class Checkbox;
class Label;
class NativeButton;
class RadioButton;
}
class FileDisplayArea;
class OptionsGroupView;
class PrefService;

////////////////////////////////////////////////////////////////////////////////
// ContentPageView

class ContentPageView : public OptionsPageView,
                        public views::LinkController,
                        public ProfileSyncServiceObserver,
                        public views::ButtonListener,
                        public ConfirmMessageBoxObserver {
 public:
  explicit ContentPageView(Profile* profile);
  virtual ~ContentPageView();

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::LinkController method.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // ConfirmMessageBoxObserver implementation.
  virtual void OnConfirmMessageAccept();

  // ProfileSyncServiceObserver method.
  virtual void OnStateChanged();

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::string* pref_name);

  // views::View overrides:
  virtual void Layout();

 private:
  // Updates various sync controls based on the current sync state.
  void UpdateSyncControls();

  // Returns whether initialization of controls is done or not.
  bool is_initialized() const {
    // If initialization is already done, all the UI controls data members
    // should be non-NULL. So check for one of them to determine if controls
    // are already initialized or not.
    return sync_group_ != NULL;
  }

  // Init all the dialog controls.
  void InitPasswordSavingGroup();
  void InitFormAutofillGroup();
  void InitBrowsingDataGroup();
  void InitThemesGroup();
  void InitSyncGroup();

  // Controls for the Password Saving group
  views::NativeButton* show_passwords_button_;
  OptionsGroupView* passwords_group_;
  views::RadioButton* passwords_asktosave_radio_;
  views::RadioButton* passwords_neversave_radio_;

  // Controls for the Form Autofill group
  views::NativeButton* change_autofill_settings_button_;
  OptionsGroupView* form_autofill_group_;

  // Controls for the Themes group
  OptionsGroupView* themes_group_;
  views::NativeButton* themes_reset_button_;
  views::Link* themes_gallery_link_;

  // Controls for the browsing data group.
  OptionsGroupView* browsing_data_group_;
  views::NativeButton* import_button_;

  // Controls for the Sync group.
  OptionsGroupView* sync_group_;
  views::Label* sync_status_label_;
  views::Link* sync_action_link_;
  views::NativeButton* sync_start_stop_button_;
  views::NativeButton* sync_customize_button_;
  views::Link* privacy_dashboard_link_;

  BooleanPrefMember ask_to_save_passwords_;
  BooleanPrefMember form_autofill_enabled_;
  StringPrefMember is_using_default_theme_;

  // Cached pointer to ProfileSyncService, if it exists. Kept up to date
  // and NULL-ed out on destruction.
  ProfileSyncService* sync_service_;

  DISALLOW_COPY_AND_ASSIGN(ContentPageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_PAGE_VIEW_H_
