// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include "base/scoped_ptr.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/pref_names.h"
#include "net/url_request/url_request_redirect_job.h"


// ProtocolHandlerRegistry -----------------------------------------------------

ProtocolHandlerRegistry::ProtocolHandlerRegistry(Profile* profile)
  :profile_(profile) {
}

void ProtocolHandlerRegistry::RegisterProtocolHandler(
    ProtocolHandler* handler) {
  if (protocolHandlers_.find(handler->protocol()) == protocolHandlers_.end()) {
    ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
    if (!policy->IsWebSafeScheme(handler->protocol())) {
      policy->RegisterWebSafeScheme(handler->protocol());
    }
    net::URLRequest::RegisterProtocolFactory(handler->protocol(),
        &ProtocolHandlerRegistry::Factory);
  }
  protocolHandlers_[handler->protocol()] = handler;
}

void ProtocolHandlerRegistry::Load() {
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->HasPrefPath(prefs::kRegisteredProtocolHandlers)) {
    return;
  }
  const ListValue* protocolHandlers =
    prefs->GetList(prefs::kRegisteredProtocolHandlers);

  for (size_t i = 0; i < protocolHandlers->GetSize(); i++) {
    DictionaryValue* dict;
    protocolHandlers->GetDictionary(i, &dict);
    RegisterHandlerFromValue(dict);
  }
}

void ProtocolHandlerRegistry::RegisterHandlerFromValue(
    const DictionaryValue* value) {
  ProtocolHandler* handler = ProtocolHandler::CreateProtocolHandler(value);
  if (handler) {
    RegisterProtocolHandler(handler);
  }
}

void ProtocolHandlerRegistry::Save() {
  scoped_ptr<Value> value(Encode());
  profile_->GetPrefs()->Set(prefs::kRegisteredProtocolHandlers, *value);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
}

ProtocolHandler* ProtocolHandlerRegistry::GetHandlerFor(
    const std::string& scheme) const {
  ProtocolHandlerMap::const_iterator i = protocolHandlers_.find(scheme);
  return i == protocolHandlers_.end() ? NULL : i->second;
}

bool ProtocolHandlerRegistry::CanSchemeBeOverridden(
    const std::string& scheme) const {
  return GetHandlerFor(scheme) != NULL ||
    !net::URLRequest::IsHandledProtocol(scheme);
}

bool ProtocolHandlerRegistry::IsAlreadyRegistered(
    const ProtocolHandler* handler) const {
  ProtocolHandler* currentHandler = GetHandlerFor(handler->protocol());
  return currentHandler && *currentHandler == *handler;
}

net::URLRequestJob* ProtocolHandlerRegistry::CreateJob(
    net::URLRequest* request,
    const std::string& scheme) const {
  ProtocolHandler* handler = GetHandlerFor(scheme);

  if (!handler) {
    return NULL;
  }

  GURL translatedUrl(handler->TranslateUrl(request->url()));

  if (!translatedUrl.is_valid()) {
    return NULL;
  }

  return new net::URLRequestRedirectJob(request, translatedUrl);
}

net::URLRequestJob* ProtocolHandlerRegistry::Factory(net::URLRequest* request,
                                                const std::string& scheme) {
  ChromeURLRequestContext* context =
    static_cast<ChromeURLRequestContext*>(request->context());
  return context->protocol_handler_registry()->CreateJob(request, scheme);
}

ProtocolHandlerRegistry::~ProtocolHandlerRegistry() {}

Value* ProtocolHandlerRegistry::Encode() {
  ListValue* protocolHandlers = new ListValue();

  for (ProtocolHandlerMap::iterator i = protocolHandlers_.begin();
      i != protocolHandlers_.end(); ++i) {
    protocolHandlers->Append(i->second->Encode());
  }
  return protocolHandlers;
}

void ProtocolHandlerRegistry::OnAcceptRegisterProtocolHandler(
    ProtocolHandler* handler) {
  RegisterProtocolHandler(handler);
  Save();
}

void ProtocolHandlerRegistry::OnDenyRegisterProtocolHandler(
    ProtocolHandler* handler) {
}

void ProtocolHandlerRegistry::RegisterPrefs(PrefService* prefService) {
  prefService->RegisterListPref(prefs::kRegisteredProtocolHandlers);
}
