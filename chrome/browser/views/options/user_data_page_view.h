// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_USER_DATA_PAGE_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_USER_DATA_PAGE_VIEW_H_

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/common/pref_member.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/view.h"

namespace views {
class GroupboxView;
class Label;
class NativeButton;
}

// TODO(idana): once the p13n module becomes public, we should get rid of the
// sync specific options dialog tab and just add a bookmark sync section to the
// existing (and newly added) "Personal Stuff" tab.

class OptionsGroupView;

///////////////////////////////////////////////////////////////////////////////
// UserDataPageView

class UserDataPageView : public OptionsPageView,
                         public views::ButtonListener,
                         public views::LinkController,
                         public ProfileSyncServiceObserver {
 public:
  explicit UserDataPageView(Profile* profile);
  virtual ~UserDataPageView();

 protected:
  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender);

  // views::LinkController method.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name);
  virtual void HighlightGroup(OptionsGroup highlight_group);

  // views::View overrides:
  virtual void Layout();

  // ProfileSyncServiceObserver methods.
  virtual void OnStateChanged();

 private:
  // Updates various controls based on the current sync state.
  void UpdateControls();
  // Returns whether initialization of controls is done or not.
  bool IsInitialized() const {
    // If initialization is already done, all the UI controls data members
    // should be non-NULL. So check for one of them to determine if controls
    // are already initialized or not.
    return sync_group_ != NULL;
  }
  // Helper to get status label for synced state.
  std::wstring GetSyncedStateStatusLabel() const;

  void InitSyncGroup();

  // Controls for the Sync group.
  OptionsGroupView* sync_group_;
  views::Label* sync_status_label_;
  views::Link* sync_action_link_;
  views::NativeButton* sync_start_stop_button_;

  // Cached pointer to ProfileSyncService, if it exists. Kept up to date
  // and NULL-ed out on destruction.
  ProfileSyncService* sync_service_;

  DISALLOW_COPY_AND_ASSIGN(UserDataPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_USER_DATA_PAGE_VIEW_H_

#endif  // CHROME_PERSONALIZATION
