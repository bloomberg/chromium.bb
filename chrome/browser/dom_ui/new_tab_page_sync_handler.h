// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(BROWSER_SYNC)

#ifndef CHROME_BROWSER_DOM_UI_NEW_TAB_PAGE_SYNC_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_NEW_TAB_PAGE_SYNC_HANDLER_H_

#include <string>

#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_status_ui_helper.h"

class Value;

// Sends sync-state changes to the New Tab Page for UI updating and forwards
// link clicks on the page to the sync service.
class NewTabPageSyncHandler : public DOMMessageHandler,
                              public ProfileSyncServiceObserver {
 public:
  NewTabPageSyncHandler();
  virtual ~NewTabPageSyncHandler();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // Callback for "GetSyncMessage".
  void HandleGetSyncMessage(const Value* value);
  // Callback for "SyncLinkClicked".
  void HandleSyncLinkClicked(const Value* value);

  // ProfileSyncServiceObserver
  virtual void OnStateChanged();

 private:
  enum MessageType {
    HIDE,
    PROMOTION,
    SYNC_ERROR,
  };
  // Helper to invoke the |syncMessageChanged| JS function on the new tab page.
  void SendSyncMessageToPage(MessageType type,
                             std::string msg, std::string linktext);

  // Helper to query the sync service and figure out what to send to
  // the page, and send it via SendSyncMessageToPage.
  // NOTE: precondition: sync must be enabled.
  void BuildAndSendSyncStatus();

  // Helper to send a message to the NNTP which hides the sync section.
  void HideSyncStatusSection();

  // Helper to convert from a sync status message type to an NTP specific one.
  static MessageType FromSyncStatusMessageType(
      SyncStatusUIHelper::MessageType type);

  // Cached pointer to ProfileSyncService.
  ProfileSyncService* sync_service_;

  // Used to make sure we don't register ourselves twice if the user refreshes
  // the new tab page.
  bool waiting_for_initial_page_load_;

  DISALLOW_COPY_AND_ASSIGN(NewTabPageSyncHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_NEW_TAB_PAGE_SYNC_HANDLER_H_
#endif  // defined(BROWSER_SYNC)
