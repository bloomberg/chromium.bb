// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_MESSAGE_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/values.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/protocol_event_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "sync/js/js_controller.h"
#include "sync/js/js_event_handler.h"

class ProfileSyncService;

// The implementation for the chrome://sync-internals page.
class SyncInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public syncer::JsEventHandler,
      public ProfileSyncServiceObserver,
      public browser_sync::ProtocolEventObserver {
 public:
  SyncInternalsMessageHandler();
  virtual ~SyncInternalsMessageHandler();

  virtual void RegisterMessages() OVERRIDE;

  // Sets up observers to receive events and forward them to the UI.
  void HandleRegisterForEvents(const base::ListValue* args);

  // Fires an event to send updated info back to the page.
  void HandleRequestUpdatedAboutInfo(const base::ListValue* args);

  // Fires and event to send the list of types back to the page.
  void HandleRequestListOfTypes(const base::ListValue* args);

  // Handler for getAllNodes message.  Needs a |request_id| argument.
  void HandleGetAllNodes(const base::ListValue* args);

  // syncer::JsEventHandler implementation.
  virtual void HandleJsEvent(
      const std::string& name,
      const syncer::JsEventDetails& details) OVERRIDE;

  // Callback used in GetAllNodes.
  void OnReceivedAllNodes(int request_id, scoped_ptr<base::ListValue> nodes);

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // ProtocolEventObserver implementation.
  virtual void OnProtocolEvent(const syncer::ProtocolEvent& e) OVERRIDE;

 private:
  // Fetches updated aboutInfo and sends it to the page in the form of an
  // onAboutInfoUpdated event.
  void SendAboutInfo();

  ProfileSyncService* GetProfileSyncService();

  base::WeakPtr<syncer::JsController> js_controller_;
  base::WeakPtrFactory<SyncInternalsMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_MESSAGE_HANDLER_H_
