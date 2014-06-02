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
#include "sync/internal_api/public/sessions/type_debug_info_observer.h"
#include "sync/js/js_controller.h"
#include "sync/js/js_event_handler.h"

class ProfileSyncService;

// The implementation for the chrome://sync-internals page.
class SyncInternalsMessageHandler : public content::WebUIMessageHandler,
                                    public syncer::JsEventHandler,
                                    public ProfileSyncServiceObserver,
                                    public browser_sync::ProtocolEventObserver,
                                    public syncer::TypeDebugInfoObserver {
 public:
  SyncInternalsMessageHandler();
  virtual ~SyncInternalsMessageHandler();

  virtual void RegisterMessages() OVERRIDE;

  // Sets up observers to receive events and forward them to the UI.
  void HandleRegisterForEvents(const base::ListValue* args);

  // Sets up observers to receive per-type counters and forward them to the UI.
  void HandleRegisterForPerTypeCounters(const base::ListValue* args);

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

  // TypeDebugInfoObserver implementation.
  virtual void OnCommitCountersUpdated(
      syncer::ModelType type,
      const syncer::CommitCounters& counters) OVERRIDE;
  virtual void OnUpdateCountersUpdated(
      syncer::ModelType type,
      const syncer::UpdateCounters& counters) OVERRIDE;
  virtual void OnStatusCountersUpdated(
      syncer::ModelType type,
      const syncer::StatusCounters& counters) OVERRIDE;

  // Helper to emit counter updates.
  //
  // Used in implementation of On*CounterUpdated methods.  Emits the given
  // dictionary value with additional data to specify the model type and
  // counter type.
  void EmitCounterUpdate(syncer::ModelType type,
                         const std::string& counter_type,
                         scoped_ptr<base::DictionaryValue> value);

 private:
  // Fetches updated aboutInfo and sends it to the page in the form of an
  // onAboutInfoUpdated event.
  void SendAboutInfo();

  ProfileSyncService* GetProfileSyncService();

  base::WeakPtr<syncer::JsController> js_controller_;

  // A flag used to prevent double-registration with ProfileSyncService.
  bool is_registered_;

  // A flag used to prevent double-registration as TypeDebugInfoObserver with
  // ProfileSyncService.
  bool is_registered_for_counters_;

  base::WeakPtrFactory<SyncInternalsMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_MESSAGE_HANDLER_H_
