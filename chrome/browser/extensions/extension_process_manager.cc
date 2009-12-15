// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_process_manager.h"

#include "chrome/browser/browsing_instance.h"
#if defined(OS_MACOSX)
#include "chrome/browser/extensions/extension_host_mac.h"
#endif
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/render_messages.h"

static void CreateBackgroundHost(
    ExtensionProcessManager* manager, Extension* extension) {
  // Start the process for the master page, if it exists.
  if (extension->background_url().is_valid())
    manager->CreateBackgroundHost(extension, extension->background_url());
}

static void CreateBackgroundHosts(
    ExtensionProcessManager* manager, const ExtensionList* extensions) {
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    CreateBackgroundHost(manager, *extension);
  }
}

ExtensionProcessManager::ExtensionProcessManager(Profile* profile)
    : browsing_instance_(new BrowsingInstance(profile)) {
  registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_HOST_DESTROYED,
                 Source<Profile>(profile));
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
#if defined(OS_WIN) || defined(OS_LINUX)
  registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                 NotificationService::AllSources());
#elif defined(OS_MACOSX)
  registrar_.Add(this, NotificationType::APP_TERMINATING,
                 NotificationService::AllSources());
#endif
}

ExtensionProcessManager::~ExtensionProcessManager() {
  CloseBackgroundHosts();
  DCHECK(background_hosts_.empty());
}

ExtensionHost* ExtensionProcessManager::CreateView(Extension* extension,
                                                   const GURL& url,
                                                   Browser* browser,
                                                   ViewType::Type view_type) {
  DCHECK(extension);
  // A NULL browser may only be given for pop-up views.
  DCHECK(browser || (!browser && view_type == ViewType::EXTENSION_POPUP));
  ExtensionHost* host =
#if defined(OS_MACOSX)
      new ExtensionHostMac(extension, GetSiteInstanceForURL(url), url,
                           view_type);
#else
      new ExtensionHost(extension, GetSiteInstanceForURL(url), url, view_type);
#endif
  host->CreateView(browser);
  OnExtensionHostCreated(host, false);
  return host;
}

ExtensionHost* ExtensionProcessManager::CreateView(const GURL& url,
                                                   Browser* browser,
                                                   ViewType::Type view_type) {
  // A NULL browser may only be given for pop-up views.
  DCHECK(browser || (!browser && view_type == ViewType::EXTENSION_POPUP));
  ExtensionsService* service =
    browsing_instance_->profile()->GetExtensionsService();
  if (service) {
    Extension* extension = service->GetExtensionByURL(url);
    if (extension)
      return CreateView(extension, url, browser, view_type);
  }
  return NULL;
}

ExtensionHost* ExtensionProcessManager::CreateToolstrip(Extension* extension,
                                                        const GURL& url,
                                                        Browser* browser) {
  return CreateView(extension, url, browser, ViewType::EXTENSION_TOOLSTRIP);
}

ExtensionHost* ExtensionProcessManager::CreateToolstrip(const GURL& url,
                                                        Browser* browser) {
  return CreateView(url, browser, ViewType::EXTENSION_TOOLSTRIP);
}

ExtensionHost* ExtensionProcessManager::CreatePopup(Extension* extension,
                                                    const GURL& url,
                                                    Browser* browser) {
  return CreateView(extension, url, browser, ViewType::EXTENSION_POPUP);
}

ExtensionHost* ExtensionProcessManager::CreatePopup(const GURL& url,
                                                    Browser* browser) {
  return CreateView(url, browser, ViewType::EXTENSION_POPUP);
}

ExtensionHost* ExtensionProcessManager::CreateBackgroundHost(
    Extension* extension, const GURL& url) {
  ExtensionHost* host =
#if defined(OS_MACOSX)
      new ExtensionHostMac(extension, GetSiteInstanceForURL(url), url,
                           ViewType::EXTENSION_BACKGROUND_PAGE);
#else
      new ExtensionHost(extension, GetSiteInstanceForURL(url), url,
                        ViewType::EXTENSION_BACKGROUND_PAGE);
#endif

  host->CreateRenderViewSoon(NULL);  // create a RenderViewHost with no view
  OnExtensionHostCreated(host, true);
  return host;
}

ExtensionHost* ExtensionProcessManager::GetBackgroundHostForExtension(
    Extension* extension) {
  for (ExtensionHostSet::iterator iter = background_hosts_.begin();
       iter != background_hosts_.end(); ++iter) {
    ExtensionHost* host = *iter;
    if (host->extension() == extension)
      return host;
  }
  return NULL;
}

void ExtensionProcessManager::RegisterExtensionProcess(
    const std::string& extension_id, int process_id) {
  ProcessIDMap::const_iterator it = process_ids_.find(extension_id);
  if (it != process_ids_.end() && (*it).second == process_id)
    return;

  // Extension ids should get removed from the map before the process ids get
  // reused from a dead renderer.
  DCHECK(it == process_ids_.end());
  process_ids_[extension_id] = process_id;

  ExtensionsService* extension_service =
      browsing_instance_->profile()->GetExtensionsService();

  std::vector<std::string> page_action_ids;
  Extension* extension =
      extension_service->GetExtensionById(extension_id, false);
  if (extension->page_action())
    page_action_ids.push_back(extension->page_action()->id());

  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  rph->Send(new ViewMsg_Extension_UpdatePageActions(extension_id,
                                                    page_action_ids));

  // Send l10n messages to the renderer - if there are any.
  if (extension->message_bundle()) {
    rph->Send(new ViewMsg_Extension_SetL10nMessages(
        extension_id, *extension->message_bundle()->dictionary()));
  }
}

void ExtensionProcessManager::UnregisterExtensionProcess(int process_id) {
  ProcessIDMap::iterator it = process_ids_.begin();
  while (it != process_ids_.end()) {
    if (it->second == process_id)
      process_ids_.erase(it++);
    else
      ++it;
  }
}

RenderProcessHost* ExtensionProcessManager::GetExtensionProcess(
    const std::string& extension_id) {
  ProcessIDMap::const_iterator it = process_ids_.find(extension_id);
  if (it == process_ids_.end())
    return NULL;

  RenderProcessHost* rph = RenderProcessHost::FromID(it->second);
  DCHECK(rph) << "We should have unregistered this host.";
  return rph;
}

SiteInstance* ExtensionProcessManager::GetSiteInstanceForURL(const GURL& url) {
  return browsing_instance_->GetSiteInstanceForURL(url);
}

void ExtensionProcessManager::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_READY:
      CreateBackgroundHosts(this,
          Source<Profile>(source).ptr()->GetExtensionsService()->extensions());
      break;

    case NotificationType::EXTENSION_LOADED: {
      ExtensionsService* service =
          Source<Profile>(source).ptr()->GetExtensionsService();
      if (service->is_ready()) {
        Extension* extension = Details<Extension>(details).ptr();
        ::CreateBackgroundHost(this, extension);
      }
      break;
    }

    case NotificationType::EXTENSION_UNLOADED: {
      Extension* extension = Details<Extension>(details).ptr();
      for (ExtensionHostSet::iterator iter = background_hosts_.begin();
           iter != background_hosts_.end(); ++iter) {
        ExtensionHost* host = *iter;
        if (host->extension()->id() == extension->id()) {
          delete host;
          // |host| should deregister itself from our structures.
          DCHECK(background_hosts_.find(host) == background_hosts_.end());
          break;
        }
      }
      break;
    }

    case NotificationType::EXTENSION_HOST_DESTROYED: {
      ExtensionHost* host = Details<ExtensionHost>(details).ptr();
      all_hosts_.erase(host);
      background_hosts_.erase(host);
      break;
    }

    case NotificationType::RENDERER_PROCESS_TERMINATED:
    case NotificationType::RENDERER_PROCESS_CLOSED: {
      RenderProcessHost* host = Source<RenderProcessHost>(source).ptr();
      UnregisterExtensionProcess(host->id());
      break;
    }
#if defined(OS_WIN) || defined(OS_LINUX)
    case NotificationType::BROWSER_CLOSED: {
      // Close background hosts when the last browser is closed so that they
      // have time to shutdown various objects on different threads. Our
      // destructor is called too late in the shutdown sequence.
      bool app_closing_non_mac = *Details<bool>(details).ptr();
      if (app_closing_non_mac)
        CloseBackgroundHosts();
      break;
    }
#elif defined(OS_MACOSX)
    case NotificationType::APP_TERMINATING: {
      // Don't follow the behavior of having the last browser window closed
      // being an indication that the app should close.
      CloseBackgroundHosts();
      break;
    }
#endif

    default:
      NOTREACHED();
  }
}

void ExtensionProcessManager::OnExtensionHostCreated(ExtensionHost* host,
                                                     bool is_background) {
  all_hosts_.insert(host);
  if (is_background)
    background_hosts_.insert(host);
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_HOST_CREATED,
      Source<ExtensionProcessManager>(this),
      Details<ExtensionHost>(host));
}

void ExtensionProcessManager::CloseBackgroundHosts() {
  for (ExtensionHostSet::iterator iter = background_hosts_.begin();
       iter != background_hosts_.end(); ) {
    ExtensionHostSet::iterator current = iter++;
    delete *current;
  }
}
