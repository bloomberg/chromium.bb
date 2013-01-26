// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/extensions/api/declarative/initializing_rules_registry.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_storage_delegate.h"
#include "chrome/browser/extensions/api/declarative_content/content_constants.h"
#include "chrome/browser/extensions/api/declarative_content/content_rules_registry.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rules_registry.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

namespace {

// Returns the key to use for storing declarative rules in the state store.
std::string GetDeclarativeRuleStorageKey(const std::string& event_name,
                                         bool incognito) {
  if (incognito)
    return "declarative_rules.incognito." + event_name;
  else
    return "declarative_rules." + event_name;
}

// Registers |web_request_rules_registry| on the IO thread.
void RegisterToExtensionWebRequestEventRouterOnIO(
    void* profile,
    scoped_refptr<WebRequestRulesRegistry> web_request_rules_registry) {
  ExtensionWebRequestEventRouter::GetInstance()->RegisterRulesRegistry(
      profile, web_request_rules_registry);
}

}  // namespace

RulesRegistryService::RulesRegistryService(Profile* profile)
    : profile_(profile) {
  if (profile) {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                   content::Source<Profile>(profile->GetOriginalProfile()));
  }
}

RulesRegistryService::~RulesRegistryService() {
  for (size_t i = 0; i < delegates_.size(); ++i)
    delegates_[i]->CleanupOnUIThread();
}

void RulesRegistryService::RegisterDefaultRulesRegistries() {
  RulesRegistryStorageDelegate* delegate = new RulesRegistryStorageDelegate();
  scoped_refptr<WebRequestRulesRegistry> web_request_rules_registry(
      new WebRequestRulesRegistry(profile_, delegate));
  delegate->InitOnUIThread(profile_, web_request_rules_registry,
      GetDeclarativeRuleStorageKey(
          declarative_webrequest_constants::kOnRequest,
          profile_->IsOffTheRecord()));
  delegates_.push_back(delegate);

  RegisterRulesRegistry(declarative_webrequest_constants::kOnRequest,
                        web_request_rules_registry);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RegisterToExtensionWebRequestEventRouterOnIO,
          profile_, web_request_rules_registry));

#if defined(ENABLE_EXTENSIONS)
  delegate = new RulesRegistryStorageDelegate();
  scoped_refptr<ContentRulesRegistry> content_rules_registry(
      new ContentRulesRegistry(profile_, delegate));
  delegate->InitOnUIThread(profile_, content_rules_registry,
      GetDeclarativeRuleStorageKey(
          declarative_content_constants::kOnPageChanged,
          profile_->IsOffTheRecord()));
  delegates_.push_back(delegate);

  RegisterRulesRegistry(declarative_content_constants::kOnPageChanged,
                        content_rules_registry);
  content_rules_registry_ = content_rules_registry.get();
#endif  // defined(ENABLE_EXTENSIONS)
}

void RulesRegistryService::Shutdown() {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RegisterToExtensionWebRequestEventRouterOnIO,
          profile_, scoped_refptr<WebRequestRulesRegistry>(NULL)));
}

void RulesRegistryService::RegisterRulesRegistry(
    const std::string& event_name,
    scoped_refptr<RulesRegistry> rule_registry) {
  DCHECK(rule_registries_.find(event_name) == rule_registries_.end());
  rule_registries_[event_name] =
      make_scoped_refptr(new InitializingRulesRegistry(rule_registry));
}

scoped_refptr<RulesRegistry> RulesRegistryService::GetRulesRegistry(
    const std::string& event_name) const {
  RulesRegistryMap::const_iterator i = rule_registries_.find(event_name);
  if (i == rule_registries_.end())
    return scoped_refptr<RulesRegistry>();
  return i->second;
}

void RulesRegistryService::SimulateExtensionUnloaded(
    const std::string& extension_id) {
  OnExtensionUnloaded(extension_id);
}

void RulesRegistryService::OnExtensionUnloaded(
    const std::string& extension_id) {
  RulesRegistryMap::iterator i;
  for (i = rule_registries_.begin(); i != rule_registries_.end(); ++i) {
    scoped_refptr<RulesRegistry> registry = i->second;
    if (content::BrowserThread::CurrentlyOn(registry->GetOwnerThread())) {
      registry->OnExtensionUnloaded(extension_id);
    } else {
      content::BrowserThread::PostTask(
          registry->GetOwnerThread(), FROM_HERE,
          base::Bind(&RulesRegistry::OnExtensionUnloaded, registry,
                     extension_id));
    }
  }
}

void RulesRegistryService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      OnExtensionUnloaded(extension->id());
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace extensions
