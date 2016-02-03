// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/sync_internals/sync_internals_message_handler.h"

#include <vector>

#include "base/logging.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/about_sync_util.h"
#include "components/sync_driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/public/provider/web/web_ui_ios.h"
#include "ios/web/public/web_thread.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/internal_api/public/sessions/commit_counters.h"
#include "sync/internal_api/public/sessions/status_counters.h"
#include "sync/internal_api/public/sessions/update_counters.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/js/js_event_details.h"

using syncer::JsEventDetails;
using syncer::ModelTypeSet;
using syncer::WeakHandle;

SyncInternalsMessageHandler::SyncInternalsMessageHandler()
    : is_registered_(false),
      is_registered_for_counters_(false),
      weak_ptr_factory_(this) {}

SyncInternalsMessageHandler::~SyncInternalsMessageHandler() {
  if (js_controller_)
    js_controller_->RemoveJsEventHandler(this);

  sync_driver::SyncService* service = GetSyncService();
  if (service && service->HasObserver(this)) {
    service->RemoveObserver(this);
    service->RemoveProtocolEventObserver(this);
  }

  if (service && is_registered_for_counters_) {
    service->RemoveTypeDebugInfoObserver(this);
  }
}

void SyncInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);

  web_ui()->RegisterMessageCallback(
      sync_driver::sync_ui_util::kRegisterForEvents,
      base::Bind(&SyncInternalsMessageHandler::HandleRegisterForEvents,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      sync_driver::sync_ui_util::kRegisterForPerTypeCounters,
      base::Bind(&SyncInternalsMessageHandler::HandleRegisterForPerTypeCounters,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      sync_driver::sync_ui_util::kRequestUpdatedAboutInfo,
      base::Bind(&SyncInternalsMessageHandler::HandleRequestUpdatedAboutInfo,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      sync_driver::sync_ui_util::kRequestListOfTypes,
      base::Bind(&SyncInternalsMessageHandler::HandleRequestListOfTypes,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      sync_driver::sync_ui_util::kGetAllNodes,
      base::Bind(&SyncInternalsMessageHandler::HandleGetAllNodes,
                 base::Unretained(this)));
}

void SyncInternalsMessageHandler::HandleRegisterForEvents(
    const base::ListValue* args) {
  DCHECK(args->empty());

  // is_registered_ flag protects us from double-registering.  This could
  // happen on a page refresh, where the JavaScript gets re-run but the
  // message handler remains unchanged.
  sync_driver::SyncService* service = GetSyncService();
  if (service && !is_registered_) {
    service->AddObserver(this);
    service->AddProtocolEventObserver(this);
    js_controller_ = service->GetJsController();
    js_controller_->AddJsEventHandler(this);
    is_registered_ = true;
  }
}

void SyncInternalsMessageHandler::HandleRegisterForPerTypeCounters(
    const base::ListValue* args) {
  DCHECK(args->empty());

  if (sync_driver::SyncService* service = GetSyncService()) {
    if (!is_registered_for_counters_) {
      service->AddTypeDebugInfoObserver(this);
      is_registered_for_counters_ = true;
    } else {
      // Re-register to ensure counters get re-emitted.
      service->RemoveTypeDebugInfoObserver(this);
      service->AddTypeDebugInfoObserver(this);
    }
  }
}

void SyncInternalsMessageHandler::HandleRequestUpdatedAboutInfo(
    const base::ListValue* args) {
  DCHECK(args->empty());
  SendAboutInfo();
}

void SyncInternalsMessageHandler::HandleRequestListOfTypes(
    const base::ListValue* args) {
  DCHECK(args->empty());
  base::DictionaryValue event_details;
  scoped_ptr<base::ListValue> type_list(new base::ListValue());
  ModelTypeSet protocol_types = syncer::ProtocolTypes();
  for (ModelTypeSet::Iterator it = protocol_types.First(); it.Good();
       it.Inc()) {
    type_list->Append(new base::StringValue(ModelTypeToString(it.Get())));
  }
  event_details.Set(sync_driver::sync_ui_util::kTypes, type_list.release());
  web_ui()->CallJavascriptFunction(
      sync_driver::sync_ui_util::kDispatchEvent,
      base::StringValue(sync_driver::sync_ui_util::kOnReceivedListOfTypes),
      event_details);
}

void SyncInternalsMessageHandler::HandleGetAllNodes(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  int request_id = 0;
  bool success = args->GetInteger(0, &request_id);
  DCHECK(success);

  sync_driver::SyncService* service = GetSyncService();
  if (service) {
    service->GetAllNodes(
        base::Bind(&SyncInternalsMessageHandler::OnReceivedAllNodes,
                   weak_ptr_factory_.GetWeakPtr(), request_id));
  }
}

void SyncInternalsMessageHandler::OnReceivedAllNodes(
    int request_id,
    scoped_ptr<base::ListValue> nodes) {
  base::FundamentalValue id(request_id);
  web_ui()->CallJavascriptFunction(
      sync_driver::sync_ui_util::kGetAllNodesCallback, id, *nodes);
}

void SyncInternalsMessageHandler::OnStateChanged() {
  SendAboutInfo();
}

void SyncInternalsMessageHandler::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  scoped_ptr<base::DictionaryValue> value(
      syncer::ProtocolEvent::ToValue(event));
  web_ui()->CallJavascriptFunction(
      sync_driver::sync_ui_util::kDispatchEvent,
      base::StringValue(sync_driver::sync_ui_util::kOnProtocolEvent), *value);
}

void SyncInternalsMessageHandler::OnCommitCountersUpdated(
    syncer::ModelType type,
    const syncer::CommitCounters& counters) {
  EmitCounterUpdate(type, sync_driver::sync_ui_util::kCommit,
                    counters.ToValue());
}

void SyncInternalsMessageHandler::OnUpdateCountersUpdated(
    syncer::ModelType type,
    const syncer::UpdateCounters& counters) {
  EmitCounterUpdate(type, sync_driver::sync_ui_util::kUpdate,
                    counters.ToValue());
}

void SyncInternalsMessageHandler::OnStatusCountersUpdated(
    syncer::ModelType type,
    const syncer::StatusCounters& counters) {
  EmitCounterUpdate(type, sync_driver::sync_ui_util::kStatus,
                    counters.ToValue());
}

void SyncInternalsMessageHandler::EmitCounterUpdate(
    syncer::ModelType type,
    const std::string& counter_type,
    scoped_ptr<base::DictionaryValue> value) {
  scoped_ptr<base::DictionaryValue> details(new base::DictionaryValue());
  details->SetString(sync_driver::sync_ui_util::kModelType,
                     ModelTypeToString(type));
  details->SetString(sync_driver::sync_ui_util::kCounterType, counter_type);
  details->Set(sync_driver::sync_ui_util::kCounters, value.release());
  web_ui()->CallJavascriptFunction(
      sync_driver::sync_ui_util::kDispatchEvent,
      base::StringValue(sync_driver::sync_ui_util::kOnCountersUpdated),
      *details);
}

void SyncInternalsMessageHandler::HandleJsEvent(const std::string& name,
                                                const JsEventDetails& details) {
  DVLOG(1) << "Handling event: " << name << " with details "
           << details.ToString();
  web_ui()->CallJavascriptFunction(sync_driver::sync_ui_util::kDispatchEvent,
                                   base::StringValue(name), details.Get());
}

void SyncInternalsMessageHandler::SendAboutInfo() {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui());
  SigninManager* signin_manager =
      ios::SigninManagerFactory::GetForBrowserState(browser_state);
  sync_driver::SyncService* sync_service = GetSyncService();
  scoped_ptr<base::DictionaryValue> value =
      sync_driver::sync_ui_util::ConstructAboutInformation(
          sync_service, signin_manager, GetChannel());
  web_ui()->CallJavascriptFunction(
      sync_driver::sync_ui_util::kDispatchEvent,
      base::StringValue(sync_driver::sync_ui_util::kOnAboutInfoUpdated),
      *value);
}

// Gets the SyncService of the underlying original profile. May return null.
sync_driver::SyncService* SyncInternalsMessageHandler::GetSyncService() {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui());
  return IOSChromeProfileSyncServiceFactory::GetForBrowserState(
      browser_state->GetOriginalChromeBrowserState());
}
