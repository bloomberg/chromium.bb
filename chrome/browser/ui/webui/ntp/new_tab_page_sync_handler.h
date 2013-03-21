// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_PAGE_SYNC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_PAGE_SYNC_HANDLER_H_

#include <string>

#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

// Sends sync-state changes to the New Tab Page for UI updating and forwards
// link clicks on the page to the sync service.
class NewTabPageSyncHandler : public content::WebUIMessageHandler,
                              public ProfileSyncServiceObserver {
 public:
  NewTabPageSyncHandler();
  virtual ~NewTabPageSyncHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for "GetSyncMessage".
  void HandleGetSyncMessage(const base::ListValue* args);
  // Callback for "SyncLinkClicked".
  void HandleSyncLinkClicked(const base::ListValue* args);

  // ProfileSyncServiceObserver
  virtual void OnStateChanged() OVERRIDE;

  void OnSigninAllowedPrefChange();

 private:
  friend class MockNewTabPageSyncHandler;
  FRIEND_TEST_ALL_PREFIXES(NewTabPageSyncHandlerBrowserTest,
                           ChangeSigninAllowedToFalse);

  enum MessageType {
    HIDE,
    SYNC_ERROR,
    SYNC_PROMO
  };
  // Helper to invoke the |syncMessageChanged| JS function on the new tab page.
  virtual void SendSyncMessageToPage(MessageType type,
                                     std::string msg, std::string linktext);

  // Helper to query the sync service and figure out what to send to
  // the page, and send it via SendSyncMessageToPage.
  // NOTE: precondition: sync must be enabled.
  void BuildAndSendSyncStatus();

  // Helper to send a message to the NNTP which hides the sync section.
  void HideSyncStatusSection();

  // Helper to convert from a sync status message type to an NTP specific one.
  static MessageType FromSyncStatusMessageType(
      sync_ui_util::MessageType type);

  // Cached pointer to ProfileSyncService.
  ProfileSyncService* sync_service_;

  PrefChangeRegistrar profile_pref_registrar_;

  // Used to make sure we don't register ourselves twice if the user refreshes
  // the new tab page.
  bool waiting_for_initial_page_load_;

  DISALLOW_COPY_AND_ASSIGN(NewTabPageSyncHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_PAGE_SYNC_HANDLER_H_
