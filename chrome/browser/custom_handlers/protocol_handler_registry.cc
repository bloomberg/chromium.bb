// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/custom_handlers/protocol_handler.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_io_data.h"
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
}

ProtocolHandlerRegistry::ProtocolHandlerList&
ProtocolHandlerRegistry::GetHandlerListFor(const std::string& scheme) {
  ProtocolHandlerMultiMap::iterator p = protocol_handlers_.find(scheme);
  if (p == protocol_handlers_.end()) {
    protocol_handlers_[scheme] = ProtocolHandlerList();
  }
  return protocol_handlers_[scheme];
}

void ProtocolHandlerRegistry::RegisterProtocolHandler(
    const ProtocolHandler& handler) {
  DCHECK(CanSchemeBeOverridden(handler.protocol()));
  DCHECK(!handler.IsEmpty());
  if (IsRegistered(handler)) {
    return;
  }
  if (enabled_ && !delegate_->IsExternalHandlerRegistered(handler.protocol())) {
    delegate_->RegisterExternalHandler(handler.protocol());
  }
  GetHandlerListFor(handler.protocol()).push_back(handler);
}

void ProtocolHandlerRegistry::IgnoreProtocolHandler(
    const ProtocolHandler& handler) {
  ignored_protocol_handlers_.push_back(handler);
}

void ProtocolHandlerRegistry::Enable() {
  if (enabled_) {
    return;
  }
  enabled_ = true;
  for (ProtocolHandlerMultiMap::const_iterator p = protocol_handlers_.begin();
       p != protocol_handlers_.end(); ++p) {
    delegate_->RegisterExternalHandler(p->first);
  }
}

void ProtocolHandlerRegistry::Disable() {
  if (!enabled_) {
    return;
  }
  enabled_ = false;
  for (ProtocolHandlerMultiMap::const_iterator p = protocol_handlers_.begin();
       p != protocol_handlers_.end(); ++p) {
    delegate_->DeregisterExternalHandler(p->first);
  }
}

std::vector<const DictionaryValue*>
ProtocolHandlerRegistry::GetHandlersFromPref(const char* pref_name) const {
  std::vector<const DictionaryValue*> result;
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->HasPrefPath(pref_name)) {
    return result;
  }

  const ListValue* handlers = prefs->GetList(pref_name);
  if (handlers) {
    for (size_t i = 0; i < handlers->GetSize(); ++i) {
      DictionaryValue* dict;
      handlers->GetDictionary(i, &dict);
      if (ProtocolHandler::IsValidDict(dict)) {
        result.push_back(dict);
      }
    }
  }
  return result;
}

void ProtocolHandlerRegistry::Load() {
  is_loading_ = true;
  PrefService* prefs = profile_->GetPrefs();
  if (prefs->HasPrefPath(prefs::kCustomHandlersEnabled)) {
    enabled_ = prefs->GetBoolean(prefs::kCustomHandlersEnabled);
  }
  std::vector<const DictionaryValue*> registered_handlers =
    GetHandlersFromPref(prefs::kRegisteredProtocolHandlers);
  for (std::vector<const DictionaryValue*>::const_iterator p =
      registered_handlers.begin();
      p != registered_handlers.end(); ++p) {
    ProtocolHandler handler = ProtocolHandler::CreateProtocolHandler(*p);
    RegisterProtocolHandler(handler);
    bool is_default = false;
    if ((*p)->GetBoolean("default", &is_default) && is_default) {
      SetDefault(handler);
    }
  }
  std::vector<const DictionaryValue*> ignored_handlers =
    GetHandlersFromPref(prefs::kIgnoredProtocolHandlers);
  for (std::vector<const DictionaryValue*>::const_iterator p =
       ignored_handlers.begin();
       p != ignored_handlers.end(); ++p) {
    IgnoreProtocolHandler(ProtocolHandler::CreateProtocolHandler(*p));
  }
  is_loading_ = false;
}

void ProtocolHandlerRegistry::RegisterHandlerFromValue(
    const DictionaryValue* value) {
  ProtocolHandler handler = ProtocolHandler::CreateProtocolHandler(value);
  if (!handler.IsEmpty()) {
    RegisterProtocolHandler(handler);
  }
}

void ProtocolHandlerRegistry::Save() {
  if (is_loading_) {
    return;
  }
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

bool ProtocolHandlerRegistry::CanSchemeBeOverridden(
    const std::string& scheme) const {
  return IsHandledProtocol(scheme) ||
    !delegate_->IsExternalHandlerRegistered(scheme);
}

void ProtocolHandlerRegistry::GetHandledProtocols(
    std::vector<std::string>* output) const {
  ProtocolHandlerMultiMap::const_iterator p;
  for (p = protocol_handlers_.begin(); p != protocol_handlers_.end(); ++p) {
    output->push_back(p->first);
  }
}

void ProtocolHandlerRegistry::RemoveIgnoredHandler(
    const ProtocolHandler& handler) {
  for (ProtocolHandlerList::iterator p = ignored_protocol_handlers_.begin();
       p != ignored_protocol_handlers_.end(); ++p) {
    if (handler == *p) {
      ignored_protocol_handlers_.erase(p);
      break;
    }
  }
}

bool ProtocolHandlerRegistry::IsRegistered(const ProtocolHandler& handler) {
  ProtocolHandlerList& handlers(GetHandlerListFor(handler.protocol()));
  return std::find(handlers.begin(), handlers.end(), handler) != handlers.end();
}

bool ProtocolHandlerRegistry::IsIgnored(const ProtocolHandler& handler) const {
  ProtocolHandlerList::const_iterator i;
  for (i = ignored_protocol_handlers_.begin();
      i != ignored_protocol_handlers_.end(); ++i) {
    if (*i == handler) {
      return true;
    }
  }
  return false;
}

bool ProtocolHandlerRegistry::IsHandledProtocol(
    const std::string& scheme) const {
  return protocol_handlers_.find(scheme) != protocol_handlers_.end();
}

void ProtocolHandlerRegistry::RemoveHandler(const ProtocolHandler& handler) {
  ProtocolHandlerList& handlers(GetHandlerListFor(handler.protocol()));
  ProtocolHandlerList::iterator p =
    std::find(handlers.begin(), handlers.end(), handler);
  if (p != handlers.end()) {
    handlers.erase(p);
  }

  ProtocolHandlerMap::iterator it = default_handlers_.find(handler.protocol());
  if (it != default_handlers_.end() && it->second == handler) {
    default_handlers_.erase(it);
  }
}

net::URLRequestJob* ProtocolHandlerRegistry::MaybeCreateJob(
    net::URLRequest* request) const {
  ProtocolHandler handler = GetHandlerFor(request->url().scheme());

  if (handler.IsEmpty()) {
    return NULL;
  }

  GURL translated_url(handler.TranslateUrl(request->url()));

  if (!translated_url.is_valid()) {
    return NULL;
  }

  return new net::URLRequestRedirectJob(request, translated_url);
}

Value* ProtocolHandlerRegistry::EncodeRegisteredHandlers() {
  ListValue* protocol_handlers = new ListValue();

  for (ProtocolHandlerMultiMap::iterator i = protocol_handlers_.begin();
       i != protocol_handlers_.end(); ++i) {
    for (ProtocolHandlerList::iterator j = i->second.begin();
         j != i->second.end(); ++j) {
      DictionaryValue* encoded = j->Encode();
      if (IsDefault(*j)) {
        encoded->Set("default", Value::CreateBooleanValue(true));
      }
      protocol_handlers->Append(encoded);
    }
  }
  return protocol_handlers;
}

Value* ProtocolHandlerRegistry::EncodeIgnoredHandlers() {
  ListValue* handlers = new ListValue();

  for (ProtocolHandlerList::iterator i = ignored_protocol_handlers_.begin();
       i != ignored_protocol_handlers_.end(); ++i) {
    handlers->Append(i->Encode());
  }
  return handlers;
}

void ProtocolHandlerRegistry::OnAcceptRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  RegisterProtocolHandler(handler);
  Save();
}

void ProtocolHandlerRegistry::OnDenyRegisterProtocolHandler(
    const ProtocolHandler& handler) {
}

void ProtocolHandlerRegistry::OnIgnoreRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  IgnoreProtocolHandler(handler);
  Save();
}

void ProtocolHandlerRegistry::RegisterPrefs(PrefService* pref_service) {
  pref_service->RegisterListPref(prefs::kRegisteredProtocolHandlers,
                                 PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterListPref(prefs::kIgnoredProtocolHandlers,
                                 PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterBooleanPref(prefs::kCustomHandlersEnabled, true,
                                    PrefService::UNSYNCABLE_PREF);
}

void ProtocolHandlerRegistry::SetDefault(const ProtocolHandler& handler) {
  default_handlers_.erase(handler.protocol());
  default_handlers_.insert(std::make_pair(handler.protocol(), handler));
  Save();
}

void ProtocolHandlerRegistry::ClearDefault(const std::string& scheme) {
  default_handlers_.erase(scheme);
  Save();
}

bool ProtocolHandlerRegistry::IsDefault(const ProtocolHandler& handler) const {
  return GetHandlerFor(handler.protocol()) == handler;
}

const ProtocolHandler& ProtocolHandlerRegistry::GetHandlerFor(
    const std::string& scheme) const {
  ProtocolHandlerMap::const_iterator p = default_handlers_.find(scheme);
  if (p != default_handlers_.end()) {
    return p->second;
  }
  ProtocolHandlerMultiMap::const_iterator q =
    protocol_handlers_.find(scheme);
  if (q != protocol_handlers_.end() && q->second.size() == 1) {
    return q->second[0];
  }
  return ProtocolHandler::EmptyProtocolHandler();
}

bool ProtocolHandlerRegistry::HasDefault(
    const std::string& scheme) const {
  return !GetHandlerFor(scheme).IsEmpty();
}

bool ProtocolHandlerRegistry::HasHandler(const std::string& scheme) {
  return !GetHandlerListFor(scheme).empty();
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
}

void ProtocolHandlerRegistry::Delegate::DeregisterExternalHandler(
    const std::string& protocol) {
}

bool ProtocolHandlerRegistry::Delegate::IsExternalHandlerRegistered(
    const std::string& protocol) {
  return ProfileIOData::IsHandledProtocol(protocol);
}
