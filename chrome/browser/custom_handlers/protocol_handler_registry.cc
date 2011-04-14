// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/custom_handlers/protocol_handler.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/browser/child_process_security_policy.h"
#include "net/base/network_delegate.h"
#include "net/url_request/url_request_redirect_job.h"


// ProtocolHandlerRegistry -----------------------------------------------------

ProtocolHandlerRegistry::ProtocolHandlerRegistry(Profile* profile,
    Delegate* delegate)
  : profile_(profile),
    delegate_(delegate),
    enabled_(true) {
}

ProtocolHandlerRegistry::~ProtocolHandlerRegistry() {
  STLDeleteContainerPairSecondPointers(protocol_handlers_.begin(),
      protocol_handlers_.end());
  STLDeleteContainerPointers(ignored_protocol_handlers_.begin(),
      ignored_protocol_handlers_.end());
}

static void RemoveAndDelete(ProtocolHandlerMap& handlers,
    const std::string& scheme) {
  ProtocolHandlerMap::iterator p = handlers.find(scheme);
  if (p != handlers.end()) {
    ProtocolHandler* removed_handler = p->second;
    handlers.erase(p);
    delete removed_handler;
  }
}

void ProtocolHandlerRegistry::RegisterProtocolHandler(
    ProtocolHandler* handler) {
  DCHECK(CanSchemeBeOverridden(handler->protocol()));
  if (enabled_) {
    delegate_->RegisterExternalHandler(handler->protocol());
  }
  RemoveAndDelete(protocol_handlers_, handler->protocol());
  protocol_handlers_[handler->protocol()] = handler;
}

void ProtocolHandlerRegistry::IgnoreProtocolHandler(ProtocolHandler* handler) {
  ignored_protocol_handlers_.push_back(handler);
}

void ProtocolHandlerRegistry::Enable() {
  if (enabled_) {
    return;
  }
  enabled_ = true;
  for (ProtocolHandlerMap::const_iterator p = protocol_handlers_.begin();
      p != protocol_handlers_.end(); p++) {
    delegate_->RegisterExternalHandler(p->first);
  }
}

void ProtocolHandlerRegistry::Disable() {
  if (!enabled_) {
    return;
  }
  enabled_ = false;
  for (ProtocolHandlerMap::const_iterator p = protocol_handlers_.begin();
      p != protocol_handlers_.end(); p++) {
    delegate_->DeregisterExternalHandler(p->first);
  }
}

ProtocolHandlerList ProtocolHandlerRegistry::GetHandlersFromPref(
    const char* pref_name) {
  ProtocolHandlerList result;
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->HasPrefPath(pref_name)) {
    return result;
  }

  const ListValue* handlers = prefs->GetList(pref_name);
  if (handlers) {
    for (size_t i = 0; i < handlers->GetSize(); i++) {
      DictionaryValue* dict;
      handlers->GetDictionary(i, &dict);
      ProtocolHandler* handler = ProtocolHandler::CreateProtocolHandler(dict);
      if (handler) {
        result.push_back(handler);
      }
    }
  }
  return result;
}

void ProtocolHandlerRegistry::Load() {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs->HasPrefPath(prefs::kCustomHandlersEnabled)) {
    enabled_ = prefs->GetBoolean(prefs::kCustomHandlersEnabled);
  }
  ProtocolHandlerList registered_handlers =
    GetHandlersFromPref(prefs::kRegisteredProtocolHandlers);
  for (ProtocolHandlerList::iterator p = registered_handlers.begin();
      p != registered_handlers.end(); p++) {
    RegisterProtocolHandler(*p);
  }
  ProtocolHandlerList ignored_handlers =
    GetHandlersFromPref(prefs::kIgnoredProtocolHandlers);
  for (ProtocolHandlerList::iterator p = ignored_handlers.begin();
      p != ignored_handlers.end(); p++) {
    IgnoreProtocolHandler(*p);
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
  scoped_ptr<Value> registered_protocol_handlers(EncodeRegisteredHandlers());
  scoped_ptr<Value> ignored_protocol_handlers(EncodeIgnoredHandlers());
  scoped_ptr<Value> enabled(Value::CreateBooleanValue(enabled_));
  profile_->GetPrefs()->Set(prefs::kRegisteredProtocolHandlers,
      *registered_protocol_handlers);
  profile_->GetPrefs()->Set(prefs::kIgnoredProtocolHandlers,
      *ignored_protocol_handlers);
  profile_->GetPrefs()->Set(prefs::kCustomHandlersEnabled, *enabled);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
}

ProtocolHandler* ProtocolHandlerRegistry::GetHandlerFor(
    const std::string& scheme) const {
  ProtocolHandlerMap::const_iterator i = protocol_handlers_.find(scheme);
  return i == protocol_handlers_.end() ? NULL : i->second;
}

bool ProtocolHandlerRegistry::CanSchemeBeOverridden(
    const std::string& scheme) const {
  return GetHandlerFor(scheme) != NULL ||
    !delegate_->IsExternalHandlerRegistered(scheme);
}

void ProtocolHandlerRegistry::GetHandledProtocols(
    std::vector<std::string>* output) {
  ProtocolHandlerMap::iterator p;
  for (p = protocol_handlers_.begin(); p != protocol_handlers_.end(); p++) {
    output->push_back(p->first);
  }
}

void ProtocolHandlerRegistry::RemoveHandlerFor(const std::string& scheme) {
  RemoveAndDelete(protocol_handlers_, scheme);
  net::URLRequest::RegisterProtocolFactory(scheme, NULL);
  Save();
}

void ProtocolHandlerRegistry::RemoveIgnoredHandler(ProtocolHandler* handler) {
  for (ProtocolHandlerList::iterator p = ignored_protocol_handlers_.begin();
      p != ignored_protocol_handlers_.end(); p++) {
    if (*handler == **p) {
      ProtocolHandler* removed_handler = *p;
      ignored_protocol_handlers_.erase(p);
      delete removed_handler;
      break;
    }
  }
}

bool ProtocolHandlerRegistry::IsRegistered(
    const ProtocolHandler* handler) const {
  ProtocolHandler* currentHandler = GetHandlerFor(handler->protocol());
  return currentHandler && *currentHandler == *handler;
}

bool ProtocolHandlerRegistry::IsIgnored(const ProtocolHandler* handler) const {
  ProtocolHandlerList::const_iterator i;
  for (i = ignored_protocol_handlers_.begin();
      i != ignored_protocol_handlers_.end(); i++) {
    if (**i == *handler) {
      return true;
    }
  }
  return false;
}


bool ProtocolHandlerRegistry::IsHandledProtocol(
    const std::string& scheme) const {
  return GetHandlerFor(scheme);
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

Value* ProtocolHandlerRegistry::EncodeRegisteredHandlers() {
  ListValue* protocol_handlers = new ListValue();

  for (ProtocolHandlerMap::iterator i = protocol_handlers_.begin();
      i != protocol_handlers_.end(); ++i) {
    protocol_handlers->Append(i->second->Encode());
  }
  return protocol_handlers;
}

Value* ProtocolHandlerRegistry::EncodeIgnoredHandlers() {
  ListValue* handlers = new ListValue();

  for (ProtocolHandlerList::iterator i = ignored_protocol_handlers_.begin();
      i != ignored_protocol_handlers_.end(); ++i) {
    handlers->Append((*i)->Encode());
  }
  return handlers;
}

void ProtocolHandlerRegistry::OnAcceptRegisterProtocolHandler(
    ProtocolHandler* handler) {
  RegisterProtocolHandler(handler);
  Save();
}

void ProtocolHandlerRegistry::OnDenyRegisterProtocolHandler(
    ProtocolHandler* handler) {
}

void ProtocolHandlerRegistry::OnIgnoreRegisterProtocolHandler(
    ProtocolHandler* handler) {
  IgnoreProtocolHandler(handler);
  Save();
}

void ProtocolHandlerRegistry::RegisterPrefs(PrefService* prefService) {
  prefService->RegisterListPref(prefs::kRegisteredProtocolHandlers);
  prefService->RegisterListPref(prefs::kIgnoredProtocolHandlers);
  prefService->RegisterBooleanPref(prefs::kCustomHandlersEnabled, true);
}

// Delegate --------------------------------------------------------------------

ProtocolHandlerRegistry::Delegate::~Delegate() {
}

void ProtocolHandlerRegistry::Delegate::RegisterExternalHandler(
    const std::string& protocol) {
  ChildProcessSecurityPolicy* policy =
    ChildProcessSecurityPolicy::GetInstance();
  if (!policy->IsWebSafeScheme(protocol)) {
    policy->RegisterWebSafeScheme(protocol);
  }
  net::URLRequest::RegisterProtocolFactory(protocol,
      &ProtocolHandlerRegistry::Factory);
}

void ProtocolHandlerRegistry::Delegate::DeregisterExternalHandler(
    const std::string& protocol) {
  net::URLRequest::RegisterProtocolFactory(protocol, NULL);
}

bool ProtocolHandlerRegistry::Delegate::IsExternalHandlerRegistered(
    const std::string& protocol) {
  return net::URLRequest::IsHandledProtocol(protocol);
}
