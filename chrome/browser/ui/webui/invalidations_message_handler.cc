// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/invalidations_message_handler.h"

#include "base/bind.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/invalidation/invalidation_handler.h"
#include "components/invalidation/invalidation_logger.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "content/public/browser/web_ui.h"

namespace invalidation {
class InvalidationLogger;
}  // namespace invalidation

namespace syncer {
class ObjectIdInvalidationMap;
}  // namespace syncer

InvalidationsMessageHandler::InvalidationsMessageHandler()
    : logger_(NULL), weak_ptr_factory_(this) {}

InvalidationsMessageHandler::~InvalidationsMessageHandler() {
  if (logger_)
    logger_->UnregisterObserver(this);
}

void InvalidationsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "doneLoading",
      base::Bind(&InvalidationsMessageHandler::UIReady,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestDetailedStatus",
      base::Bind(&InvalidationsMessageHandler::HandleRequestDetailedStatus,
                 base::Unretained(this)));
}

void InvalidationsMessageHandler::UIReady(const base::ListValue* args) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (invalidation_provider) {
    logger_ = invalidation_provider->GetInvalidationService()->
        GetInvalidationLogger();
  }
  if (logger_ && !logger_->IsObserverRegistered(this))
    logger_->RegisterObserver(this);
  UpdateContent(args);
}

void InvalidationsMessageHandler::HandleRequestDetailedStatus(
    const base::ListValue* args) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (invalidation_provider) {
    invalidation_provider->GetInvalidationService()->RequestDetailedStatus(
        base::Bind(&InvalidationsMessageHandler::OnDetailedStatus,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void InvalidationsMessageHandler::UpdateContent(const base::ListValue* args) {
  if (logger_)
    logger_->EmitContent();
}

void InvalidationsMessageHandler::OnRegistrationChange(
    const std::multiset<std::string>& registered_handlers) {
  base::ListValue list_of_handlers;
  for (std::multiset<std::string>::const_iterator it =
       registered_handlers.begin();
       it != registered_handlers.end(); ++it) {
    list_of_handlers.Append(new base::StringValue(*it));
  }
  web_ui()->CallJavascriptFunction("chrome.invalidations.updateHandlers",
                                   list_of_handlers);
}

void InvalidationsMessageHandler::OnStateChange(
    const syncer::InvalidatorState& new_state,
    const base::Time& last_changed_timestamp) {
  std::string state(syncer::InvalidatorStateToString(new_state));
  web_ui()->CallJavascriptFunction(
      "chrome.invalidations.updateInvalidatorState", base::StringValue(state),
      base::FundamentalValue(last_changed_timestamp.ToJsTime()));
}

void InvalidationsMessageHandler::OnUpdateIds(
    const std::string& handler_name,
    const syncer::ObjectIdCountMap& ids) {
  base::ListValue list_of_objects;
  for (syncer::ObjectIdCountMap::const_iterator it = ids.begin();
       it != ids.end();
       ++it) {
    scoped_ptr<base::DictionaryValue> dic(new base::DictionaryValue());
    dic->SetString("name", (it->first).name());
    dic->SetInteger("source", (it->first).source());
    dic->SetInteger("totalCount", it->second);
    list_of_objects.Append(dic.release());
  }
  web_ui()->CallJavascriptFunction("chrome.invalidations.updateIds",
                                   base::StringValue(handler_name),
                                   list_of_objects);
}
void InvalidationsMessageHandler::OnDebugMessage(
    const base::DictionaryValue& details) {}

void InvalidationsMessageHandler::OnInvalidation(
    const syncer::ObjectIdInvalidationMap& new_invalidations) {
  scoped_ptr<base::ListValue> invalidations_list = new_invalidations.ToValue();
  web_ui()->CallJavascriptFunction("chrome.invalidations.logInvalidations",
                                   *invalidations_list);
}

void InvalidationsMessageHandler::OnDetailedStatus(
    const base::DictionaryValue& network_details) {
  web_ui()->CallJavascriptFunction("chrome.invalidations.updateDetailedStatus",
                                   network_details);
}
