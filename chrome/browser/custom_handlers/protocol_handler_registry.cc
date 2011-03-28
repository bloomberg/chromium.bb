// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/pref_names.h"
#include "content/browser/child_process_security_policy.h"
#include "net/base/network_delegate.h"
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

net::URLRequestJob* ProtocolHandlerRegistry::Factory(net::URLRequest* request,
    const std::string& scheme) {
  return request->context()->network_delegate()->MaybeCreateURLRequestJob(
      request);
}


net::URLRequestJob* ProtocolHandlerRegistry::MaybeCreateJob(
    net::URLRequest* request) const {
  ProtocolHandler* handler = GetHandlerFor(request->url().scheme());

  if (!handler) {
    return NULL;
  }

  GURL translated_url(handler->TranslateUrl(request->url()));

  if (!translated_url.is_valid()) {
    return NULL;
  }

  return new net::URLRequestRedirectJob(request, translated_url);
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
