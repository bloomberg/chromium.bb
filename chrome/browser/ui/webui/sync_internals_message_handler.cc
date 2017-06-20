// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_message_handler.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/about_sync_util.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/cycle/commit_counters.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/cycle/update_counters.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/js/js_event_details.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/user_events/user_event_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using browser_sync::ProfileSyncService;
using syncer::JsEventDetails;
using syncer::ModelTypeSet;
using syncer::SyncService;
using syncer::WeakHandle;

namespace {

// Converts the string at |index| in |list| to an int, defaulting to 0 on error.
int64_t StringAtIndexToInt64(const base::ListValue* list, int index) {
  std::string str;
  if (list->GetString(index, &str)) {
    int64_t integer = 0;
    if (base::StringToInt64(str, &integer))
      return integer;
  }
  return 0;
}

}  //  namespace

SyncInternalsMessageHandler::SyncInternalsMessageHandler()
    : SyncInternalsMessageHandler(
          base::BindRepeating(
              &syncer::sync_ui_util::ConstructAboutInformation)) {}

SyncInternalsMessageHandler::SyncInternalsMessageHandler(
    AboutSyncDataDelegate about_sync_data_delegate)
    : about_sync_data_delegate_(std::move(about_sync_data_delegate)),
      weak_ptr_factory_(this) {}

SyncInternalsMessageHandler::~SyncInternalsMessageHandler() {
  UnregisterModelNotifications();
}

void SyncInternalsMessageHandler::OnJavascriptDisallowed() {
  // Invaliding weak ptrs works well here because the only weak ptr we vend is
  // to the sync side to give us information that should be used to populate the
  // javascript side. If javascript is disallowed, we don't care about updating
  // the UI with data, so dropping those callbacks is fine.
  weak_ptr_factory_.InvalidateWeakPtrs();
  UnregisterModelNotifications();
}

void SyncInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRegisterForEvents,
      base::Bind(&SyncInternalsMessageHandler::HandleRegisterForEvents,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRegisterForPerTypeCounters,
      base::Bind(&SyncInternalsMessageHandler::HandleRegisterForPerTypeCounters,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestUpdatedAboutInfo,
      base::Bind(&SyncInternalsMessageHandler::HandleRequestUpdatedAboutInfo,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestListOfTypes,
      base::Bind(&SyncInternalsMessageHandler::HandleRequestListOfTypes,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kRequestUserEventsVisibility,
      base::Bind(
          &SyncInternalsMessageHandler::HandleRequestUserEventsVisibility,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kSetIncludeSpecifics,
      base::Bind(&SyncInternalsMessageHandler::HandleSetIncludeSpecifics,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kWriteUserEvent,
      base::Bind(&SyncInternalsMessageHandler::HandleWriteUserEvent,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      syncer::sync_ui_util::kGetAllNodes,
      base::Bind(&SyncInternalsMessageHandler::HandleGetAllNodes,
                 base::Unretained(this)));
}

void SyncInternalsMessageHandler::HandleRegisterForEvents(
    const ListValue* args) {
  DCHECK(args->empty());
  AllowJavascript();

  // is_registered_ flag protects us from double-registering.  This could
  // happen on a page refresh, where the JavaScript gets re-run but the
  // message handler remains unchanged.
  SyncService* service = GetSyncService();
  if (service && !is_registered_) {
    service->AddObserver(this);
    service->AddProtocolEventObserver(this);
    js_controller_ = service->GetJsController();
    js_controller_->AddJsEventHandler(this);
    is_registered_ = true;
  }
}

void SyncInternalsMessageHandler::HandleRegisterForPerTypeCounters(
    const ListValue* args) {
  DCHECK(args->empty());
  AllowJavascript();

  SyncService* service = GetSyncService();
  if (!service)
    return;

  if (!is_registered_for_counters_) {
    service->AddTypeDebugInfoObserver(this);
    is_registered_for_counters_ = true;
  } else {
    // Re-register to ensure counters get re-emitted.
    service->RemoveTypeDebugInfoObserver(this);
    service->AddTypeDebugInfoObserver(this);
  }
}

void SyncInternalsMessageHandler::HandleRequestUpdatedAboutInfo(
    const ListValue* args) {
  DCHECK(args->empty());
  AllowJavascript();
  SendAboutInfo();
}

void SyncInternalsMessageHandler::HandleRequestListOfTypes(
    const ListValue* args) {
  DCHECK(args->empty());
  AllowJavascript();

  DictionaryValue event_details;
  auto type_list = base::MakeUnique<ListValue>();
  ModelTypeSet protocol_types = syncer::ProtocolTypes();
  for (ModelTypeSet::Iterator it = protocol_types.First(); it.Good();
       it.Inc()) {
    type_list->AppendString(ModelTypeToString(it.Get()));
  }
  event_details.Set(syncer::sync_ui_util::kTypes, std::move(type_list));
  DispatchEvent(syncer::sync_ui_util::kOnReceivedListOfTypes, event_details);
}

void SyncInternalsMessageHandler::HandleGetAllNodes(const ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  AllowJavascript();

  int request_id = 0;
  bool success = ExtractIntegerValue(args, &request_id);
  DCHECK(success);

  SyncService* service = GetSyncService();
  if (service) {
    // This opens up the possibility of non-javascript code calling us
    // asynchronously, and potentially at times we're not allowed to call into
    // the javascript side. We guard against this by invalidating this weak ptr
    // should javascript become disallowed.
    service->GetAllNodes(
        base::Bind(&SyncInternalsMessageHandler::OnReceivedAllNodes,
                   weak_ptr_factory_.GetWeakPtr(), request_id));
  }
}

void SyncInternalsMessageHandler::HandleRequestUserEventsVisibility(
    const base::ListValue* args) {
  DCHECK(args->empty());
  AllowJavascript();
  CallJavascriptFunction(
      syncer::sync_ui_util::kUserEventsVisibilityCallback,
      Value(base::FeatureList::IsEnabled(switches::kSyncUserEvents)));
}

void SyncInternalsMessageHandler::HandleSetIncludeSpecifics(
    const ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  AllowJavascript();
  include_specifics_ = args->GetList()[0].GetBool();
}

void SyncInternalsMessageHandler::HandleWriteUserEvent(
    const base::ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  AllowJavascript();

  Profile* profile = Profile::FromWebUI(web_ui());
  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile);

  sync_pb::UserEventSpecifics event_specifics;
  event_specifics.set_event_time_usec(StringAtIndexToInt64(args, 0));
  event_specifics.set_navigation_id(StringAtIndexToInt64(args, 1));
  user_event_service->RecordUserEvent(event_specifics);
}

void SyncInternalsMessageHandler::OnReceivedAllNodes(
    int request_id,
    std::unique_ptr<ListValue> nodes) {
  CallJavascriptFunction(syncer::sync_ui_util::kGetAllNodesCallback,
                         Value(request_id), *nodes);
}

void SyncInternalsMessageHandler::OnStateChanged(SyncService* sync) {
  SendAboutInfo();
}

void SyncInternalsMessageHandler::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  std::unique_ptr<DictionaryValue> value(
      syncer::ProtocolEvent::ToValue(event, include_specifics_));
  DispatchEvent(syncer::sync_ui_util::kOnProtocolEvent, *value);
}

void SyncInternalsMessageHandler::OnCommitCountersUpdated(
    syncer::ModelType type,
    const syncer::CommitCounters& counters) {
  EmitCounterUpdate(type, syncer::sync_ui_util::kCommit, counters.ToValue());
}

void SyncInternalsMessageHandler::OnUpdateCountersUpdated(
    syncer::ModelType type,
    const syncer::UpdateCounters& counters) {
  EmitCounterUpdate(type, syncer::sync_ui_util::kUpdate, counters.ToValue());
}

void SyncInternalsMessageHandler::OnStatusCountersUpdated(
    syncer::ModelType type,
    const syncer::StatusCounters& counters) {
  EmitCounterUpdate(type, syncer::sync_ui_util::kStatus, counters.ToValue());
}

void SyncInternalsMessageHandler::EmitCounterUpdate(
    syncer::ModelType type,
    const std::string& counter_type,
    std::unique_ptr<DictionaryValue> value) {
  auto details = base::MakeUnique<DictionaryValue>();
  details->SetString(syncer::sync_ui_util::kModelType, ModelTypeToString(type));
  details->SetString(syncer::sync_ui_util::kCounterType, counter_type);
  details->Set(syncer::sync_ui_util::kCounters, std::move(value));
  DispatchEvent(syncer::sync_ui_util::kOnCountersUpdated, *details);
}

void SyncInternalsMessageHandler::HandleJsEvent(
    const std::string& name,
    const JsEventDetails& details) {
  DVLOG(1) << "Handling event: " << name
           << " with details " << details.ToString();
  DispatchEvent(name, details.Get());
}

void SyncInternalsMessageHandler::SendAboutInfo() {
  std::unique_ptr<DictionaryValue> value =
      about_sync_data_delegate_.Run(GetSyncService(), chrome::GetChannel());
  DispatchEvent(syncer::sync_ui_util::kOnAboutInfoUpdated, *value);
}

SyncService* SyncInternalsMessageHandler::GetSyncService() {
  return ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(
      Profile::FromWebUI(web_ui())->GetOriginalProfile());
}

void SyncInternalsMessageHandler::DispatchEvent(const std::string& name,
                                                const Value& details_value) {
  CallJavascriptFunction(syncer::sync_ui_util::kDispatchEvent, Value(name),
                         details_value);
}

void SyncInternalsMessageHandler::UnregisterModelNotifications() {
  SyncService* service = GetSyncService();
  if (!service)
    return;

  // Cannot use ScopedObserver to do all the tracking because most don't follow
  // AddObserver/RemoveObserver method naming style.
  if (is_registered_) {
    DCHECK(js_controller_);
    service->RemoveObserver(this);
    service->RemoveProtocolEventObserver(this);
    js_controller_->RemoveJsEventHandler(this);
    js_controller_ = nullptr;
    is_registered_ = false;
  }

  if (is_registered_for_counters_) {
    service->RemoveTypeDebugInfoObserver(this);
    is_registered_for_counters_ = false;
  }
}
