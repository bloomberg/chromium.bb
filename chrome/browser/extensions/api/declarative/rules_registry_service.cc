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
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_constants.h"
#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_rules_registry.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

// Registers |web_request_rules_registry| on the IO thread.
void RegisterToExtensionWebRequestEventRouterOnIO(
    void* profile,
    const RulesRegistryService::WebViewKey& webview_key,
    scoped_refptr<WebRequestRulesRegistry> web_request_rules_registry) {
  ExtensionWebRequestEventRouter::GetInstance()->RegisterRulesRegistry(
      profile, webview_key, web_request_rules_registry);
}

bool IsWebView(const RulesRegistryService::WebViewKey& webview_key) {
  return webview_key.embedder_process_id && webview_key.webview_instance_id;
}

}  // namespace

RulesRegistryService::RulesRegistryService(content::BrowserContext* context)
    : content_rules_registry_(NULL),
      extension_registry_observer_(this),
      profile_(Profile::FromBrowserContext(context)) {
  if (profile_) {
    extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
    registrar_.Add(
        this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::NotificationService::AllBrowserContextsAndSources());
    EnsureDefaultRulesRegistriesRegistered(WebViewKey(0, 0));
  }
}

RulesRegistryService::~RulesRegistryService() {}

void RulesRegistryService::EnsureDefaultRulesRegistriesRegistered(
    const WebViewKey& webview_key) {
  if (!profile_)
    return;

  RulesRegistryKey key(declarative_webrequest_constants::kOnRequest,
                       webview_key);
  // If we can find the key in the |rule_registries_| then we have already
  // installed the default registries.
  if (ContainsKey(rule_registries_, key))
    return;


  RulesCacheDelegate* web_request_cache_delegate = NULL;
  if (!IsWebView(webview_key)) {
    web_request_cache_delegate =
        new RulesCacheDelegate(true /*log_storage_init_delay*/);
    cache_delegates_.push_back(web_request_cache_delegate);
  }
  scoped_refptr<WebRequestRulesRegistry> web_request_rules_registry(
      new WebRequestRulesRegistry(profile_,
                                  web_request_cache_delegate,
                                  webview_key));

  RegisterRulesRegistry(web_request_rules_registry);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RegisterToExtensionWebRequestEventRouterOnIO,
          profile_, webview_key, web_request_rules_registry));

  // Only create a ContentRulesRegistry for regular pages and not webviews.
  if (!IsWebView(webview_key)) {
    RulesCacheDelegate* content_rules_cache_delegate =
        new RulesCacheDelegate(false /*log_storage_init_delay*/);
    cache_delegates_.push_back(content_rules_cache_delegate);
    scoped_refptr<ContentRulesRegistry> content_rules_registry(
        new ContentRulesRegistry(profile_, content_rules_cache_delegate));
    RegisterRulesRegistry(content_rules_registry);
    content_rules_registry_ = content_rules_registry.get();
  }
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
          profile_, WebViewKey(0, 0),
          scoped_refptr<WebRequestRulesRegistry>(NULL)));
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<RulesRegistryService> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<RulesRegistryService>*
RulesRegistryService::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
RulesRegistryService* RulesRegistryService::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<RulesRegistryService>::Get(context);
}

void RulesRegistryService::RegisterRulesRegistry(
    scoped_refptr<RulesRegistry> rule_registry) {
  const std::string event_name(rule_registry->event_name());
  RulesRegistryKey key(event_name, rule_registry->webview_key());
  DCHECK(rule_registries_.find(key) == rule_registries_.end());
  rule_registries_[key] = rule_registry;
}

scoped_refptr<RulesRegistry> RulesRegistryService::GetRulesRegistry(
    const WebViewKey& webview_key,
    const std::string& event_name) {
  EnsureDefaultRulesRegistriesRegistered(webview_key);

  RulesRegistryKey key(event_name, webview_key);
  RulesRegistryMap::const_iterator i = rule_registries_.find(key);
  if (i == rule_registries_.end())
    return scoped_refptr<RulesRegistry>();
  return i->second;
}

void RulesRegistryService::RemoveWebViewRulesRegistries(int process_id) {
  DCHECK_NE(0, process_id);

  std::set<RulesRegistryKey> registries_to_delete;
  for (RulesRegistryMap::iterator it = rule_registries_.begin();
       it != rule_registries_.end(); ++it) {
    const RulesRegistryKey& key = it->first;
    const WebViewKey& webview_key = key.webview_key;
    int embedder_process_id = webview_key.embedder_process_id;
    // |process_id| will always be non-zero.
    // |embedder_process_id| will only be non-zero if the key corresponds to a
    // webview registry.
    // Thus, |embedder_process_id| == |process_id| ==> the process ID is a
    // webview embedder.
    if (embedder_process_id != process_id)
      continue;

    // Modifying the container while iterating is bad so we'll save the keys we
    // wish to delete in another container, and delete them in another loop.
    registries_to_delete.insert(key);
  }
  for (std::set<RulesRegistryKey>::iterator it = registries_to_delete.begin();
       it != registries_to_delete.end(); ++it) {
    rule_registries_.erase(*it);
  }
}

void RulesRegistryService::SimulateExtensionUninstalled(
    const std::string& extension_id) {
  NotifyRegistriesHelper(&RulesRegistry::OnExtensionUninstalled, extension_id);
}

void RulesRegistryService::NotifyRegistriesHelper(
    void (RulesRegistry::*notification_callback)(const std::string&),
    const std::string& extension_id) {
  RulesRegistryMap::iterator i;
  for (i = rule_registries_.begin(); i != rule_registries_.end(); ++i) {
    scoped_refptr<RulesRegistry> registry = i->second;
    if (content::BrowserThread::CurrentlyOn(registry->owner_thread())) {
      (registry->*notification_callback)(extension_id);
    } else {
      content::BrowserThread::PostTask(
          registry->owner_thread(),
          FROM_HERE,
          base::Bind(notification_callback, registry, extension_id));
    }
  }
}

void RulesRegistryService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  NotifyRegistriesHelper(&RulesRegistry::OnExtensionLoaded, extension->id());
}

void RulesRegistryService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  NotifyRegistriesHelper(&RulesRegistry::OnExtensionUnloaded, extension->id());
}

void RulesRegistryService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  NotifyRegistriesHelper(&RulesRegistry::OnExtensionUninstalled,
                         extension->id());
}

void RulesRegistryService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_RENDERER_PROCESS_TERMINATED, type);

  content::RenderProcessHost* process =
      content::Source<content::RenderProcessHost>(source).ptr();
  RemoveWebViewRulesRegistries(process->GetID());
}

}  // namespace extensions
