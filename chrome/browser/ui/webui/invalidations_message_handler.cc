// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/invalidations_message_handler.h"

#include "base/bind.h"
#include "chrome/browser/invalidation/invalidation_logger.h"
#include "chrome/browser/invalidation/invalidation_service.h"
#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

namespace invalidation {
class InvalidationLogger;
}  // namespace invalidation

namespace syncer {
class ObjectIdInvalidationMap;
}  // namespace syncer

InvalidationsMessageHandler::InvalidationsMessageHandler()
    : logger_(NULL), isRegistered(false) {}

InvalidationsMessageHandler::~InvalidationsMessageHandler() {
  if (logger_)
    logger_->UnregisterForDebug(this);
}

void InvalidationsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "doneLoading",
      base::Bind(&InvalidationsMessageHandler::UIReady,
                 base::Unretained(this)));
}

void InvalidationsMessageHandler::UIReady(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!isRegistered && profile) {
    invalidation::InvalidationService* invalidation_service =
        invalidation::InvalidationServiceFactory::GetForProfile(profile);
    if (invalidation_service)
      logger_ = invalidation_service->GetInvalidationLogger();
    if (logger_) {
      logger_->RegisterForDebug(this);
      isRegistered = true;
    }
  }
  UpdateContent(args);
}

void InvalidationsMessageHandler::UpdateContent(const base::ListValue* args) {
  if (logger_)
    logger_->EmitContent();
}

void InvalidationsMessageHandler::OnUnregistration(
    const base::DictionaryValue& newRegistrationState) {}

void InvalidationsMessageHandler::OnRegistration(
    const base::DictionaryValue& newRegistrationState) {}

void InvalidationsMessageHandler::OnStateChange(
    const syncer::InvalidatorState& newState) {
  std::string state(syncer::InvalidatorStateToString(newState));
  web_ui()->CallJavascriptFunction("chrome.invalidations.updateState",
                                   base::StringValue(state));
}

void InvalidationsMessageHandler::OnUpdateIds(
    const base::DictionaryValue& newIds) {}

void InvalidationsMessageHandler::OnDebugMessage(
    const base::DictionaryValue& details) {}

void InvalidationsMessageHandler::OnInvalidation(
    const syncer::ObjectIdInvalidationMap& newInvalidations) {
  scoped_ptr<base::ListValue> invalidationsList = newInvalidations.ToValue();
  web_ui()->CallJavascriptFunction("chrome.invalidations.logInvalidations",
                                   *invalidationsList);
}
