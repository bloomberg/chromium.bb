// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_message_handler.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/driver/about_sync_util.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/cycle/commit_counters.h"
#include "components/sync/engine/cycle/status_counters.h"
#include "components/sync/engine/cycle/update_counters.h"
#include "components/sync/engine/events/protocol_event.h"
#include "components/sync/js/js_event_details.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

using browser_sync::ProfileSyncService;
using syncer::JsEventDetails;
using syncer::ModelTypeSet;
using syncer::WeakHandle;

namespace {
class UtilAboutSyncDataExtractor : public AboutSyncDataExtractor {
 public:
  std::unique_ptr<base::DictionaryValue> ConstructAboutInformation(
      syncer::SyncService* service,
      SigninManagerBase* signin) override {
    return syncer::sync_ui_util::ConstructAboutInformation(
        service, signin, chrome::GetChannel());
  }
};
}  //  namespace

SyncInternalsMessageHandler::SyncInternalsMessageHandler()
    : SyncInternalsMessageHandler(
          base::MakeUnique<UtilAboutSyncDataExtractor>()) {}

SyncInternalsMessageHandler::SyncInternalsMessageHandler(
    std::unique_ptr<AboutSyncDataExtractor> about_sync_data_extractor)
    : about_sync_data_extractor_(std::move(about_sync_data_extractor)),
      weak_ptr_factory_(this) {}

SyncInternalsMessageHandler::~SyncInternalsMessageHandler() {
  if (js_controller_)
    js_controller_->RemoveJsEventHandler(this);

  ProfileSyncService* service = GetProfileSyncService();
  if (service && service->HasObserver(this)) {
    service->RemoveObserver(this);
    service->RemoveProtocolEventObserver(this);
  }

  if (service && is_registered_for_counters_) {
    service->RemoveTypeDebugInfoObserver(this);
  }
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
      syncer::sync_ui_util::kGetAllNodes,
      base::Bind(&SyncInternalsMessageHandler::HandleGetAllNodes,
                 base::Unretained(this)));
}

void SyncInternalsMessageHandler::HandleRegisterForEvents(
    const base::ListValue* args) {
  DCHECK(args->empty());

  // is_registered_ flag protects us from double-registering.  This could
  // happen on a page refresh, where the JavaScript gets re-run but the
  // message handler remains unchanged.
  ProfileSyncService* service = GetProfileSyncService();
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

  if (ProfileSyncService* service = GetProfileSyncService()) {
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
  std::unique_ptr<base::ListValue> type_list(new base::ListValue());
  ModelTypeSet protocol_types = syncer::ProtocolTypes();
  for (ModelTypeSet::Iterator it = protocol_types.First();
       it.Good(); it.Inc()) {
    type_list->AppendString(ModelTypeToString(it.Get()));
  }
  event_details.Set(syncer::sync_ui_util::kTypes, type_list.release());
  web_ui()->CallJavascriptFunctionUnsafe(
      syncer::sync_ui_util::kDispatchEvent,
      base::StringValue(syncer::sync_ui_util::kOnReceivedListOfTypes),
      event_details);
}

void SyncInternalsMessageHandler::HandleGetAllNodes(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  int request_id = 0;
  bool success = ExtractIntegerValue(args, &request_id);
  DCHECK(success);

  ProfileSyncService* service = GetProfileSyncService();
  if (service) {
    service->GetAllNodes(
        base::Bind(&SyncInternalsMessageHandler::OnReceivedAllNodes,
                   weak_ptr_factory_.GetWeakPtr(), request_id));
  }
}

void SyncInternalsMessageHandler::OnReceivedAllNodes(
    int request_id,
    std::unique_ptr<base::ListValue> nodes) {
  base::Value id(request_id);
  web_ui()->CallJavascriptFunctionUnsafe(
      syncer::sync_ui_util::kGetAllNodesCallback, id, *nodes);
}

void SyncInternalsMessageHandler::OnStateChanged(syncer::SyncService* sync) {
  SendAboutInfo();
}

void SyncInternalsMessageHandler::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  std::unique_ptr<base::DictionaryValue> value(
      syncer::ProtocolEvent::ToValue(event));
  web_ui()->CallJavascriptFunctionUnsafe(
      syncer::sync_ui_util::kDispatchEvent,
      base::StringValue(syncer::sync_ui_util::kOnProtocolEvent), *value);
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
    std::unique_ptr<base::DictionaryValue> value) {
  std::unique_ptr<base::DictionaryValue> details(new base::DictionaryValue());
  details->SetString(syncer::sync_ui_util::kModelType, ModelTypeToString(type));
  details->SetString(syncer::sync_ui_util::kCounterType, counter_type);
  details->Set(syncer::sync_ui_util::kCounters, value.release());
  web_ui()->CallJavascriptFunctionUnsafe(
      syncer::sync_ui_util::kDispatchEvent,
      base::StringValue(syncer::sync_ui_util::kOnCountersUpdated), *details);
}

void SyncInternalsMessageHandler::HandleJsEvent(
    const std::string& name,
    const JsEventDetails& details) {
  DVLOG(1) << "Handling event: " << name
           << " with details " << details.ToString();
  web_ui()->CallJavascriptFunctionUnsafe(syncer::sync_ui_util::kDispatchEvent,
                                         base::StringValue(name),
                                         details.Get());
}

void SyncInternalsMessageHandler::SendAboutInfo() {
  ProfileSyncService* sync_service = GetProfileSyncService();
  SigninManagerBase* signin = sync_service ? sync_service->signin() : nullptr;
  std::unique_ptr<base::DictionaryValue> value =
      about_sync_data_extractor_->ConstructAboutInformation(sync_service,
                                                            signin);
  web_ui()->CallJavascriptFunctionUnsafe(
      syncer::sync_ui_util::kDispatchEvent,
      base::StringValue(syncer::sync_ui_util::kOnAboutInfoUpdated), *value);
}

// Gets the ProfileSyncService of the underlying original profile.
// May return NULL (e.g., if sync is disabled on the command line).
ProfileSyncService* SyncInternalsMessageHandler::GetProfileSyncService() {
  Profile* profile = Profile::FromWebUI(web_ui());
  return ProfileSyncServiceFactory::GetForProfile(
      profile->GetOriginalProfile());
}
