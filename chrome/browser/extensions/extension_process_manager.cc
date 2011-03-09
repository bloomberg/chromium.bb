// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_process_manager.h"

#include "chrome/browser/browser_window.h"
#include "content/browser/browsing_instance.h"
#if defined(OS_MACOSX)
#include "chrome/browser/extensions/extension_host_mac.h"
#endif
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace {

// Incognito profiles use this process manager. It is mostly a shim that decides
// whether to fall back on the original profile's ExtensionProcessManager based
// on whether a given extension uses "split" or "spanning" incognito behavior.
class IncognitoExtensionProcessManager : public ExtensionProcessManager {
 public:
  explicit IncognitoExtensionProcessManager(Profile* profile);
  virtual ~IncognitoExtensionProcessManager() {}
  virtual ExtensionHost* CreateView(const Extension* extension,
                                    const GURL& url,
                                    Browser* browser,
                                    ViewType::Type view_type);
  virtual void CreateBackgroundHost(const Extension* extension,
                                    const GURL& url);
  virtual SiteInstance* GetSiteInstanceForURL(const GURL& url);
  virtual RenderProcessHost* GetExtensionProcess(const GURL& url);

 private:
  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Returns the extension for an URL, which can either be a chrome-extension
  // URL or a web app URL.
  const Extension* GetExtensionOrAppByURL(const GURL& url);

  // Returns true if the extension is allowed to run in incognito mode.
  bool IsIncognitoEnabled(const Extension* extension);

  ExtensionProcessManager* original_manager_;
};

static void CreateBackgroundHost(
    ExtensionProcessManager* manager, const Extension* extension) {
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

}  // namespace

extern bool g_log_bug53991;

//
// ExtensionProcessManager
//

// static
ExtensionProcessManager* ExtensionProcessManager::Create(Profile* profile) {
  return (profile->IsOffTheRecord()) ?
      new IncognitoExtensionProcessManager(profile) :
      new ExtensionProcessManager(profile);
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
  registrar_.Add(this, NotificationType::APP_TERMINATING,
                 NotificationService::AllSources());
}

ExtensionProcessManager::~ExtensionProcessManager() {
  VLOG_IF(1, g_log_bug53991) << "~ExtensionProcessManager: " << this;
  CloseBackgroundHosts();
  DCHECK(background_hosts_.empty());
}

ExtensionHost* ExtensionProcessManager::CreateView(const Extension* extension,
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
  ExtensionService* service =
      browsing_instance_->profile()->GetExtensionService();
  if (service) {
    const Extension* extension = service->GetExtensionByURL(url);
    if (extension)
      return CreateView(extension, url, browser, view_type);
  }
  return NULL;
}

ExtensionHost* ExtensionProcessManager::CreatePopup(const Extension* extension,
                                                    const GURL& url,
                                                    Browser* browser) {
  return CreateView(extension, url, browser, ViewType::EXTENSION_POPUP);
}

ExtensionHost* ExtensionProcessManager::CreatePopup(const GURL& url,
                                                    Browser* browser) {
  return CreateView(url, browser, ViewType::EXTENSION_POPUP);
}

ExtensionHost* ExtensionProcessManager::CreateInfobar(
    const Extension* extension, const GURL& url, Browser* browser) {
  return CreateView(extension, url, browser, ViewType::EXTENSION_INFOBAR);
}

ExtensionHost* ExtensionProcessManager::CreateInfobar(const GURL& url,
                                                      Browser* browser) {
  return CreateView(url, browser, ViewType::EXTENSION_INFOBAR);
}

void ExtensionProcessManager::CreateBackgroundHost(
    const Extension* extension, const GURL& url) {
  // Don't create multiple background hosts for an extension.
  if (GetBackgroundHostForExtension(extension))
    return;

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
}

void ExtensionProcessManager::OpenOptionsPage(const Extension* extension,
                                              Browser* browser) {
  DCHECK(!extension->options_url().is_empty());

  // Force the options page to open in non-OTR window, because it won't be
  // able to save settings from OTR.
  if (!browser || browser->profile()->IsOffTheRecord()) {
    browser = Browser::GetOrCreateTabbedBrowser(
        browsing_instance_->profile()->GetOriginalProfile());
  }

  browser->OpenURL(extension->options_url(), GURL(), SINGLETON_TAB,
                   PageTransition::LINK);
  browser->window()->Show();
  browser->GetSelectedTabContents()->Activate();
}

ExtensionHost* ExtensionProcessManager::GetBackgroundHostForExtension(
    const Extension* extension) {
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
  // TODO(mpcomplete): This is the only place we actually read process_ids_.
  // Is it necessary?
  ProcessIDMap::const_iterator it = process_ids_.find(extension_id);
  if (it != process_ids_.end() && (*it).second == process_id)
    return;

  // Extension ids should get removed from the map before the process ids get
  // reused from a dead renderer.
  DCHECK(it == process_ids_.end());
  process_ids_[extension_id] = process_id;

  ExtensionService* extension_service =
      browsing_instance_->profile()->GetExtensionService();

  std::vector<std::string> page_action_ids;
  const Extension* extension =
      extension_service->GetExtensionById(extension_id, false);
  if (extension->page_action())
    page_action_ids.push_back(extension->page_action()->id());

  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  rph->Send(new ViewMsg_Extension_UpdatePageActions(extension_id,
                                                    page_action_ids));
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
    const GURL& url) {
  if (!browsing_instance_->HasSiteInstance(url))
    return NULL;
  scoped_refptr<SiteInstance> site(
      browsing_instance_->GetSiteInstanceForURL(url));
  if (site->HasProcess())
    return site->GetProcess();
  return NULL;
}

RenderProcessHost* ExtensionProcessManager::GetExtensionProcess(
    const std::string& extension_id) {
  return GetExtensionProcess(
      Extension::GetBaseURLFromExtensionId(extension_id));
}

SiteInstance* ExtensionProcessManager::GetSiteInstanceForURL(const GURL& url) {
  return browsing_instance_->GetSiteInstanceForURL(url);
}

bool ExtensionProcessManager::HasExtensionHost(ExtensionHost* host) const {
  return all_hosts_.find(host) != all_hosts_.end();
}

void ExtensionProcessManager::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_READY: {
      CreateBackgroundHosts(this,
          Source<Profile>(source).ptr()->GetExtensionService()->extensions());
      break;
    }

    case NotificationType::EXTENSION_LOADED: {
      ExtensionService* service =
          Source<Profile>(source).ptr()->GetExtensionService();
      if (service->is_ready()) {
        const Extension* extension = Details<const Extension>(details).ptr();
        ::CreateBackgroundHost(this, extension);
      }
      break;
    }

    case NotificationType::EXTENSION_UNLOADED: {
      const Extension* extension =
          Details<UnloadedExtensionInfo>(details)->extension;
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

    case NotificationType::APP_TERMINATING: {
      // Close background hosts when the last browser is closed so that they
      // have time to shutdown various objects on different threads. Our
      // destructor is called too late in the shutdown sequence.
      CloseBackgroundHosts();
      break;
    }

    default:
      NOTREACHED();
  }
}

void ExtensionProcessManager::OnExtensionHostCreated(ExtensionHost* host,
                                                     bool is_background) {
  DCHECK_EQ(browsing_instance_->profile(), host->profile());

  all_hosts_.insert(host);
  if (is_background)
    background_hosts_.insert(host);
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_HOST_CREATED,
      Source<ExtensionProcessManager>(this),
      Details<ExtensionHost>(host));
}

void ExtensionProcessManager::CloseBackgroundHosts() {
  VLOG_IF(1, g_log_bug53991) << "CloseBackgroundHosts: " << this;
  for (ExtensionHostSet::iterator iter = background_hosts_.begin();
       iter != background_hosts_.end(); ) {
    ExtensionHostSet::iterator current = iter++;
    delete *current;
  }
}

//
// IncognitoExtensionProcessManager
//

IncognitoExtensionProcessManager::IncognitoExtensionProcessManager(
    Profile* profile)
    : ExtensionProcessManager(profile),
      original_manager_(profile->GetOriginalProfile()->
                            GetExtensionProcessManager()) {
  DCHECK(profile->IsOffTheRecord());

  registrar_.Add(this, NotificationType::BROWSER_WINDOW_READY,
                 NotificationService::AllSources());
}

ExtensionHost* IncognitoExtensionProcessManager::CreateView(
    const Extension* extension,
    const GURL& url,
    Browser* browser,
    ViewType::Type view_type) {
  if (extension->incognito_split_mode()) {
    if (IsIncognitoEnabled(extension)) {
      return ExtensionProcessManager::CreateView(extension, url,
                                                 browser, view_type);
    } else {
      NOTREACHED() <<
          "We shouldn't be trying to create an incognito extension view unless "
          "it has been enabled for incognito.";
      return NULL;
    }
  } else {
    return original_manager_->CreateView(extension, url, browser, view_type);
  }
}

void IncognitoExtensionProcessManager::CreateBackgroundHost(
    const Extension* extension, const GURL& url) {
  if (extension->incognito_split_mode()) {
    if (IsIncognitoEnabled(extension))
      ExtensionProcessManager::CreateBackgroundHost(extension, url);
  } else {
    // Do nothing. If an extension is spanning, then its original-profile
    // background page is shared with incognito, so we don't create another.
  }
}

SiteInstance* IncognitoExtensionProcessManager::GetSiteInstanceForURL(
    const GURL& url) {
  const Extension* extension = GetExtensionOrAppByURL(url);
  if (!extension || extension->incognito_split_mode()) {
    return ExtensionProcessManager::GetSiteInstanceForURL(url);
  } else {
    return original_manager_->GetSiteInstanceForURL(url);
  }
}

RenderProcessHost* IncognitoExtensionProcessManager::GetExtensionProcess(
    const GURL& url) {
  const Extension* extension = GetExtensionOrAppByURL(url);
  if (!extension || extension->incognito_split_mode()) {
    return ExtensionProcessManager::GetExtensionProcess(url);
  } else {
    return original_manager_->GetExtensionProcess(url);
  }
}

const Extension* IncognitoExtensionProcessManager::GetExtensionOrAppByURL(
    const GURL& url) {
  ExtensionService* service =
      browsing_instance_->profile()->GetExtensionService();
  if (!service)
    return NULL;
  return (url.SchemeIs(chrome::kExtensionScheme)) ?
      service->GetExtensionByURL(url) : service->GetExtensionByWebExtent(url);
}

bool IncognitoExtensionProcessManager::IsIncognitoEnabled(
    const Extension* extension) {
  ExtensionService* service =
      browsing_instance_->profile()->GetExtensionService();
  return service && service->IsIncognitoEnabled(extension);
}

void IncognitoExtensionProcessManager::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::BROWSER_WINDOW_READY: {
      // We want to spawn our background hosts as soon as the user opens an
      // incognito window. Watch for new browsers and create the hosts if
      // it matches our profile.
      Browser* browser = Source<Browser>(source).ptr();
      if (browser->profile() == browsing_instance_->profile()) {
        // On Chrome OS, a login screen is implemented as a browser.
        // This browser has no extension service.  In this case,
        // service will be NULL.
        ExtensionService* service =
            browsing_instance_->profile()->GetExtensionService();
        if (service && service->is_ready())
          CreateBackgroundHosts(this, service->extensions());
      }
      break;
    }
    default:
      ExtensionProcessManager::Observe(type, source, details);
      break;
  }
}
