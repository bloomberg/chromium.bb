// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/custom_handlers/protocol_handler.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/browser/child_process_security_policy.h"
#include "content/common/notification_service.h"
#include "net/base/network_delegate.h"
#include "net/url_request/url_request_redirect_job.h"

// ProtocolHandlerRegistry -----------------------------------------------------

ProtocolHandlerRegistry::ProtocolHandlerRegistry(Profile* profile,
    Delegate* delegate)
    : profile_(profile),
      delegate_(delegate),
      enabled_(true),
      is_loading_(false) {
}

ProtocolHandlerRegistry::~ProtocolHandlerRegistry() {
  DCHECK(default_client_observers_.empty());
}

void ProtocolHandlerRegistry::Finalize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  STLDeleteElements(&default_client_observers_);
}

const ProtocolHandlerRegistry::ProtocolHandlerList*
ProtocolHandlerRegistry::GetHandlerList(
    const std::string& scheme) const {
  lock_.AssertAcquired();
  ProtocolHandlerMultiMap::const_iterator p = protocol_handlers_.find(scheme);
  if (p == protocol_handlers_.end()) {
    return NULL;
  }
  return &p->second;
}

ProtocolHandlerRegistry::ProtocolHandlerList
ProtocolHandlerRegistry::GetHandlersFor(
    const std::string& scheme) const {
  base::AutoLock auto_lock(lock_);
  ProtocolHandlerMultiMap::const_iterator p = protocol_handlers_.find(scheme);
  if (p == protocol_handlers_.end()) {
    return ProtocolHandlerList();
  }
  return p->second;
}

void ProtocolHandlerRegistry::RegisterProtocolHandler(
    const ProtocolHandler& handler) {
  DCHECK(CanSchemeBeOverriddenInternal(handler.protocol()));
  DCHECK(!handler.IsEmpty());
  lock_.AssertAcquired();
  if (IsRegisteredInternal(handler)) {
    return;
  }
  if (enabled_ && !delegate_->IsExternalHandlerRegistered(handler.protocol()))
    delegate_->RegisterExternalHandler(handler.protocol());
  InsertHandler(handler);
}

void ProtocolHandlerRegistry::InsertHandler(const ProtocolHandler& handler) {
  lock_.AssertAcquired();
  ProtocolHandlerMultiMap::iterator p =
      protocol_handlers_.find(handler.protocol());

  if (p != protocol_handlers_.end()) {
    p->second.push_back(handler);
    return;
  }

  ProtocolHandlerList new_list;
  new_list.push_back(handler);
  protocol_handlers_[handler.protocol()] = new_list;
}

void ProtocolHandlerRegistry::IgnoreProtocolHandler(
    const ProtocolHandler& handler) {
  lock_.AssertAcquired();
  ignored_protocol_handlers_.push_back(handler);
}

void ProtocolHandlerRegistry::Enable() {
  {
    base::AutoLock auto_lock(lock_);
    if (enabled_) {
      return;
    }
    enabled_ = true;
    ProtocolHandlerMap::const_iterator p;
    for (p = default_handlers_.begin(); p != default_handlers_.end(); ++p) {
      delegate_->RegisterExternalHandler(p->first);
    }
    Save();
  }
  NotifyChanged();
}

void ProtocolHandlerRegistry::Disable() {
  {
    base::AutoLock auto_lock(lock_);
    if (!enabled_) {
      return;
    }
    enabled_ = false;
    ProtocolHandlerMap::const_iterator p;
    for (p = default_handlers_.begin(); p != default_handlers_.end(); ++p) {
      delegate_->DeregisterExternalHandler(p->first);
    }
    Save();
  }
  NotifyChanged();
}

std::vector<const DictionaryValue*>
ProtocolHandlerRegistry::GetHandlersFromPref(const char* pref_name) const {
  lock_.AssertAcquired();
  std::vector<const DictionaryValue*> result;
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->HasPrefPath(pref_name)) {
    return result;
  }

  const ListValue* handlers = prefs->GetList(pref_name);
  if (handlers) {
    for (size_t i = 0; i < handlers->GetSize(); ++i) {
      DictionaryValue* dict;
      if (!handlers->GetDictionary(i, &dict))
        continue;
      if (ProtocolHandler::IsValidDict(dict)) {
        result.push_back(dict);
      }
    }
  }
  return result;
}

void ProtocolHandlerRegistry::Load() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock auto_lock(lock_);
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

  // For each default protocol handler, check that we are still registered
  // with the OS as the default application.
  bool check_os_registration = true;
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch(switches::kDisableCustomProtocolOSCheck))
    check_os_registration = false;
  if (check_os_registration) {
    for (ProtocolHandlerMap::const_iterator p = default_handlers_.begin();
         p != default_handlers_.end(); ++p) {
      ProtocolHandler handler = p->second;
      DefaultClientObserver* observer = new DefaultClientObserver(this);
      scoped_refptr<ShellIntegration::DefaultProtocolClientWorker> worker;
      worker = new ShellIntegration::DefaultProtocolClientWorker(
          observer, handler.protocol());
      observer->SetWorker(worker);
      default_client_observers_.push_back(observer);
      worker->StartCheckIsDefault();
    }
  }
}

void ProtocolHandlerRegistry::Save() {
  lock_.AssertAcquired();
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
  base::AutoLock auto_lock(lock_);
  return CanSchemeBeOverriddenInternal(scheme);
}

bool ProtocolHandlerRegistry::CanSchemeBeOverriddenInternal(
    const std::string& scheme) const {
  lock_.AssertAcquired();
  const ProtocolHandlerList* handlers = GetHandlerList(scheme);
  // If we already have a handler for this scheme, we can add more.
  if (handlers != NULL && !handlers->empty())
    return true;
  // Don't override a scheme if it already has an external handler.
  return !delegate_->IsExternalHandlerRegistered(scheme);
}

void ProtocolHandlerRegistry::GetRegisteredProtocols(
    std::vector<std::string>* output) const {
  base::AutoLock auto_lock(lock_);
  ProtocolHandlerMultiMap::const_iterator p;
  for (p = protocol_handlers_.begin(); p != protocol_handlers_.end(); ++p) {
    if (!p->second.empty())
      output->push_back(p->first);
  }
}

void ProtocolHandlerRegistry::RemoveIgnoredHandler(
    const ProtocolHandler& handler) {
  bool should_notify = false;
  {
    base::AutoLock auto_lock(lock_);
    ProtocolHandlerList::iterator p = std::find(
        ignored_protocol_handlers_.begin(), ignored_protocol_handlers_.end(),
        handler);
    if (p != ignored_protocol_handlers_.end()) {
      ignored_protocol_handlers_.erase(p);
      Save();
      should_notify = true;
    }
  }
  if (should_notify)
    NotifyChanged();
}

bool ProtocolHandlerRegistry::IsRegistered(
    const ProtocolHandler& handler) const {
  base::AutoLock auto_lock(lock_);
  return IsRegisteredInternal(handler);
}

bool ProtocolHandlerRegistry::IsRegisteredInternal(
    const ProtocolHandler& handler) const {
  lock_.AssertAcquired();
  const ProtocolHandlerList* handlers = GetHandlerList(handler.protocol());
  if (!handlers) {
    return false;
  }
  return std::find(handlers->begin(), handlers->end(), handler) !=
      handlers->end();
}

bool ProtocolHandlerRegistry::IsIgnored(const ProtocolHandler& handler) const {
  base::AutoLock auto_lock(lock_);
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
  base::AutoLock auto_lock(lock_);
  return IsHandledProtocolInternal(scheme);
}

bool ProtocolHandlerRegistry::IsHandledProtocolInternal(
    const std::string& scheme) const {
  lock_.AssertAcquired();
  return enabled_ && !GetHandlerForInternal(scheme).IsEmpty();
}

void ProtocolHandlerRegistry::RemoveHandler(const ProtocolHandler& handler) {
  {
    base::AutoLock auto_lock(lock_);
    RemoveHandlerInternal(handler);
  }
  NotifyChanged();
}

void ProtocolHandlerRegistry::RemoveHandlerInternal(
    const ProtocolHandler& handler) {
  ProtocolHandlerList& handlers = protocol_handlers_[handler.protocol()];
  ProtocolHandlerList::iterator p =
      std::find(handlers.begin(), handlers.end(), handler);
  if (p != handlers.end()) {
    handlers.erase(p);
  }
  ProtocolHandlerMap::iterator q = default_handlers_.find(handler.protocol());
  if (q != default_handlers_.end() && q->second == handler) {
    default_handlers_.erase(q);
  }

  if (!IsHandledProtocolInternal(handler.protocol())) {
    delegate_->DeregisterExternalHandler(handler.protocol());
  }
  Save();
}

void ProtocolHandlerRegistry::RemoveDefaultHandler(const std::string& scheme) {
  {
    base::AutoLock auto_lock(lock_);
    ProtocolHandler current_default = GetHandlerForInternal(scheme);
    if (!current_default.IsEmpty())
      RemoveHandlerInternal(current_default);
  }
  NotifyChanged();
}

net::URLRequestJob* ProtocolHandlerRegistry::MaybeCreateJob(
    net::URLRequest* request) const {
  base::AutoLock auto_lock(lock_);
  ProtocolHandler handler = GetHandlerForInternal(request->url().scheme());
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
  lock_.AssertAcquired();
  ListValue* protocol_handlers = new ListValue();

  for (ProtocolHandlerMultiMap::iterator i = protocol_handlers_.begin();
       i != protocol_handlers_.end(); ++i) {
    for (ProtocolHandlerList::iterator j = i->second.begin();
         j != i->second.end(); ++j) {
      DictionaryValue* encoded = j->Encode();
      if (IsDefaultInternal(*j)) {
        encoded->Set("default", Value::CreateBooleanValue(true));
      }
      protocol_handlers->Append(encoded);
    }
  }
  return protocol_handlers;
}

Value* ProtocolHandlerRegistry::EncodeIgnoredHandlers() {
  lock_.AssertAcquired();
  ListValue* handlers = new ListValue();

  for (ProtocolHandlerList::iterator i = ignored_protocol_handlers_.begin();
       i != ignored_protocol_handlers_.end(); ++i) {
    handlers->Append(i->Encode());
  }
  return handlers;
}

void ProtocolHandlerRegistry::OnAcceptRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  {
    base::AutoLock auto_lock(lock_);
    RegisterProtocolHandler(handler);
    SetDefault(handler);
    Save();
  }
  NotifyChanged();
}

void ProtocolHandlerRegistry::OnDenyRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  {
    base::AutoLock auto_lock(lock_);
    RegisterProtocolHandler(handler);
    Save();
  }
  NotifyChanged();
}

void ProtocolHandlerRegistry::OnIgnoreRegisterProtocolHandler(
    const ProtocolHandler& handler) {
  base::AutoLock auto_lock(lock_);
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
  lock_.AssertAcquired();
  ProtocolHandlerMap::const_iterator p = default_handlers_.find(
      handler.protocol());
  // If we're not loading, and we are setting a default for a new protocol,
  // register with the OS.
  if (!is_loading_ && p == default_handlers_.end())
      delegate_->RegisterWithOSAsDefaultClient(handler.protocol());
  default_handlers_.erase(handler.protocol());
  default_handlers_.insert(std::make_pair(handler.protocol(), handler));
}

void ProtocolHandlerRegistry::ClearDefault(const std::string& scheme) {
  {
    base::AutoLock auto_lock(lock_);
    default_handlers_.erase(scheme);
    Save();
  }
  NotifyChanged();
}

bool ProtocolHandlerRegistry::IsDefault(const ProtocolHandler& handler) const {
  base::AutoLock auto_lock(lock_);
  return IsDefaultInternal(handler);
}

bool ProtocolHandlerRegistry::IsDefaultInternal(
    const ProtocolHandler& handler) const {
  lock_.AssertAcquired();
  return GetHandlerForInternal(handler.protocol()) == handler;
}

const ProtocolHandler& ProtocolHandlerRegistry::GetHandlerFor(
    const std::string& scheme) const {
  base::AutoLock auto_lock(lock_);
  return GetHandlerForInternal(scheme);
}

const ProtocolHandler& ProtocolHandlerRegistry::GetHandlerForInternal(
    const std::string& scheme) const {
  lock_.AssertAcquired();
  ProtocolHandlerMap::const_iterator p = default_handlers_.find(scheme);
  if (p != default_handlers_.end()) {
    return p->second;
  }
  return ProtocolHandler::EmptyProtocolHandler();
}

int ProtocolHandlerRegistry::GetHandlerIndex(const std::string& scheme) const {
  base::AutoLock auto_lock(lock_);
  const ProtocolHandler& handler = GetHandlerForInternal(scheme);
  if (handler.IsEmpty())
    return -1;
  const ProtocolHandlerList* handlers = GetHandlerList(scheme);
  if (!handlers)
    return -1;

  ProtocolHandlerList::const_iterator p;
  int i;
  for (i = 0, p = handlers->begin(); p != handlers->end(); ++p, ++i) {
    if (*p == handler)
      return i;
  }
  return -1;
}

void ProtocolHandlerRegistry::NotifyChanged() {
  // NOTE(koz): This must not hold the lock because observers watching for this
  // event may call back into ProtocolHandlerRegistry.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NotificationService::current()->Notify(
      NotificationType::PROTOCOL_HANDLER_REGISTRY_CHANGED,
      Source<Profile>(profile_),
      NotificationService::NoDetails());
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

void ProtocolHandlerRegistry::Delegate::RegisterWithOSAsDefaultClient(
    const std::string& protocol) {
  // The worker is automatically freed when it is no longer needed due
  // to reference counting.
  scoped_refptr<ShellIntegration::DefaultProtocolClientWorker> worker;
  worker = new ShellIntegration::DefaultProtocolClientWorker(NULL, protocol);
  worker->StartSetAsDefault();
}

bool ProtocolHandlerRegistry::Delegate::IsExternalHandlerRegistered(
    const std::string& protocol) {
  // NOTE(koz): This function is safe to call from any thread, despite living
  // in ProfileIOData.
  return ProfileIOData::IsHandledProtocol(protocol);
}

// DefaultClientObserver ------------------------------------------------------

ProtocolHandlerRegistry::DefaultClientObserver::DefaultClientObserver(
    ProtocolHandlerRegistry* registry)
    : registry_(registry), worker_(NULL) {
  DCHECK(registry_);
}

ProtocolHandlerRegistry::DefaultClientObserver::~DefaultClientObserver() {
  if (worker_)
    worker_->ObserverDestroyed();
}

void
ProtocolHandlerRegistry::DefaultClientObserver::SetDefaultWebClientUIState(
    ShellIntegration::DefaultWebClientUIState state) {
  if (worker_) {
    if (state == ShellIntegration::STATE_NOT_DEFAULT)
      registry_->RemoveDefaultHandler(worker_->protocol());
  } else {
    NOTREACHED();
  }
}

void ProtocolHandlerRegistry::DefaultClientObserver::SetWorker(
    ShellIntegration::DefaultProtocolClientWorker* worker) {
  worker_ = worker;
}
