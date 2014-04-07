// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_message_handler.h"

#include <vector>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "sync/internal_api/public/events/protocol_event.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/js/js_event_details.h"

using syncer::JsEventDetails;
using syncer::ModelTypeSet;
using syncer::WeakHandle;

SyncInternalsMessageHandler::SyncInternalsMessageHandler()
    : weak_ptr_factory_(this) {}

SyncInternalsMessageHandler::~SyncInternalsMessageHandler() {
  if (js_controller_)
    js_controller_->RemoveJsEventHandler(this);

  ProfileSyncService* service = GetProfileSyncService();
  if (service && service->HasObserver(this)) {
    service->RemoveObserver(this);
    service->RemoveProtocolEventObserver(this);
  }
}

void SyncInternalsMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "registerForEvents",
      base::Bind(&SyncInternalsMessageHandler::HandleRegisterForEvents,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestUpdatedAboutInfo",
      base::Bind(&SyncInternalsMessageHandler::HandleRequestUpdatedAboutInfo,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestListOfTypes",
      base::Bind(&SyncInternalsMessageHandler::HandleRequestListOfTypes,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getAllNodes",
      base::Bind(&SyncInternalsMessageHandler::HandleGetAllNodes,
                 base::Unretained(this)));
}

void SyncInternalsMessageHandler::HandleRegisterForEvents(
    const base::ListValue* args) {
  DCHECK(args->empty());

  ProfileSyncService* service = GetProfileSyncService();
  if (service) {
    service->AddObserver(this);
    service->AddProtocolEventObserver(this);
    js_controller_ = service->GetJsController();
    js_controller_->AddJsEventHandler(this);
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
  for (ModelTypeSet::Iterator it = protocol_types.First();
       it.Good(); it.Inc()) {
    type_list->Append(new base::StringValue(ModelTypeToString(it.Get())));
  }
  event_details.Set("types", type_list.release());
  web_ui()->CallJavascriptFunction(
      "chrome.sync.dispatchEvent",
      base::StringValue("onReceivedListOfTypes"),
      event_details);
}

void SyncInternalsMessageHandler::HandleGetAllNodes(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());
  int request_id = 0;
  bool success = args->GetInteger(0, &request_id);
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
    scoped_ptr<base::ListValue> nodes) {
  base::ListValue response_args;
  response_args.Append(new base::FundamentalValue(request_id));
  response_args.Append(nodes.release());

  web_ui()->CallJavascriptFunction("chrome.sync.getAllNodesCallback",
                                   response_args);
}

void SyncInternalsMessageHandler::OnStateChanged() {
  SendAboutInfo();
}

void SyncInternalsMessageHandler::OnProtocolEvent(
    const syncer::ProtocolEvent& event) {
  scoped_ptr<base::DictionaryValue> value(
      syncer::ProtocolEvent::ToValue(event));
  web_ui()->CallJavascriptFunction(
      "chrome.sync.dispatchEvent",
      base::StringValue("onProtocolEvent"),
      *value);
}

void SyncInternalsMessageHandler::HandleJsEvent(
    const std::string& name,
    const JsEventDetails& details) {
  DVLOG(1) << "Handling event: " << name
           << " with details " << details.ToString();
  web_ui()->CallJavascriptFunction("chrome.sync.dispatchEvent",
                                   base::StringValue(name),
                                   details.Get());
}

void SyncInternalsMessageHandler::SendAboutInfo() {
  scoped_ptr<base::DictionaryValue> value =
      sync_ui_util::ConstructAboutInformation(GetProfileSyncService());
  web_ui()->CallJavascriptFunction(
      "chrome.sync.dispatchEvent",
      base::StringValue("onAboutInfoUpdated"),
      *value);
}

// Gets the ProfileSyncService of the underlying original profile.
// May return NULL (e.g., if sync is disabled on the command line).
ProfileSyncService* SyncInternalsMessageHandler::GetProfileSyncService() {
  Profile* profile = Profile::FromWebUI(web_ui());
  ProfileSyncServiceFactory* factory = ProfileSyncServiceFactory::GetInstance();
  return factory->GetForProfile(profile->GetOriginalProfile());
}

