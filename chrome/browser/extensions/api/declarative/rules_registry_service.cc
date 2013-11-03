// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/declarative/rules_cache_delegate.h"
#include "chrome/browser/extensions/api/declarative_content/content_rules_registry.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rules_registry.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

namespace {

// Registers |web_request_rules_registry| on the IO thread.
void RegisterToExtensionWebRequestEventRouterOnIO(
    void* profile,
    scoped_refptr<WebRequestRulesRegistry> web_request_rules_registry) {
  ExtensionWebRequestEventRouter::GetInstance()->RegisterRulesRegistry(
      profile, web_request_rules_registry);
}

}  // namespace

RulesRegistryService::RulesRegistryService(Profile* profile)
    : content_rules_registry_(NULL),
      profile_(profile) {
  if (profile) {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
        content::Source<Profile>(profile->GetOriginalProfile()));
    RegisterDefaultRulesRegistries();
  }
}

RulesRegistryService::~RulesRegistryService() {}

void RulesRegistryService::RegisterDefaultRulesRegistries() {
  scoped_ptr<RulesCacheDelegate> web_request_cache_delegate(
      new RulesCacheDelegate(true /*log_storage_init_delay*/));
  scoped_refptr<WebRequestRulesRegistry> web_request_rules_registry(
      new WebRequestRulesRegistry(profile_, web_request_cache_delegate.get()));
  cache_delegates_.push_back(web_request_cache_delegate.release());

  RegisterRulesRegistry(web_request_rules_registry);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RegisterToExtensionWebRequestEventRouterOnIO,
          profile_, web_request_rules_registry));

#if defined(ENABLE_EXTENSIONS)
  scoped_ptr<RulesCacheDelegate> content_rules_cache_delegate(
      new RulesCacheDelegate(false /*log_storage_init_delay*/));
  scoped_refptr<ContentRulesRegistry> content_rules_registry(
      new ContentRulesRegistry(profile_, content_rules_cache_delegate.get()));
  cache_delegates_.push_back(content_rules_cache_delegate.release());

  RegisterRulesRegistry(content_rules_registry);
  content_rules_registry_ = content_rules_registry.get();
#endif  // defined(ENABLE_EXTENSIONS)
}

void RulesRegistryService::Shutdown() {
  // Release the references to all registries. This would happen soon during
  // destruction of |*this|, but we need the ExtensionWebRequestEventRouter to
  // be the last to reference the WebRequestRulesRegistry objects, so that
  // the posted task below causes their destruction on the IO thread, not on UI
  // where the destruction of |*this| takes place.
  // TODO(vabr): Remove once http://crbug.com/218451#c6 gets addressed.
  rule_registries_.clear();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RegisterToExtensionWebRequestEventRouterOnIO,
          profile_, scoped_refptr<WebRequestRulesRegistry>(NULL)));
}

static base::LazyInstance<ProfileKeyedAPIFactory<RulesRegistryService> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<RulesRegistryService>*
RulesRegistryService::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
RulesRegistryService* RulesRegistryService::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<RulesRegistryService>::GetForProfile(profile);
}

void RulesRegistryService::RegisterRulesRegistry(
    scoped_refptr<RulesRegistry> rule_registry) {
  const std::string event_name(rule_registry->event_name());
  DCHECK(rule_registries_.find(event_name) == rule_registries_.end());
  rule_registries_[event_name] = rule_registry;
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
    if (content::BrowserThread::CurrentlyOn(registry->owner_thread())) {
      registry->OnExtensionUnloaded(extension_id);
    } else {
      content::BrowserThread::PostTask(
          registry->owner_thread(),
          FROM_HERE,
          base::Bind(
              &RulesRegistry::OnExtensionUnloaded, registry, extension_id));
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
