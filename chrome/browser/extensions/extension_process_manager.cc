// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_process_manager.h"

#include "chrome/browser/ui/browser_window.h"
#include "content/browser/browsing_instance.h"
#if defined(OS_MACOSX)
#include "chrome/browser/extensions/extension_host_mac.h"
#endif
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"

namespace {

// Incognito profiles use this process manager. It is mostly a shim that decides
// whether to fall back on the original profile's ExtensionProcessManager based
// on whether a given extension uses "split" or "spanning" incognito behavior.
class IncognitoExtensionProcessManager : public ExtensionProcessManager {
 public:
  explicit IncognitoExtensionProcessManager(Profile* profile);
  virtual ~IncognitoExtensionProcessManager() {}
  virtual ExtensionHost* CreateViewHost(const Extension* extension,
                                        const GURL& url,
                                        Browser* browser,
                                        ViewType::Type view_type) OVERRIDE;
  virtual void CreateBackgroundHost(const Extension* extension,
                                    const GURL& url);
  virtual SiteInstance* GetSiteInstanceForURL(const GURL& url);
  virtual RenderProcessHost* GetExtensionProcess(const GURL& url);
  virtual const Extension* GetExtensionForSiteInstance(int site_instance_id);

 private:
  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Returns the extension for an URL, which can either be a chrome-extension
  // URL or a web app URL.
  const Extension* GetExtensionOrAppByURL(const GURL& url);

  // Returns true if the extension is allowed to run in incognito mode.
  bool IsIncognitoEnabled(const Extension* extension);

  ExtensionProcessManager* original_manager_;
};

static void CreateBackgroundHostForExtensionLoad(
    ExtensionProcessManager* manager, const Extension* extension) {
  // Start the process for the master page, if it exists and we're not lazy.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLazyBackgroundPages) &&
      extension->background_url().is_valid())
    manager->CreateBackgroundHost(extension, extension->background_url());
}

static void CreateBackgroundHostsForProfileStartup(
    ExtensionProcessManager* manager, const ExtensionList* extensions) {
  for (ExtensionList::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    CreateBackgroundHostForExtensionLoad(manager, *extension);
  }
}

}  // namespace

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
  Profile* original_profile = profile->GetOriginalProfile();
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 Source<Profile>(original_profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 Source<Profile>(original_profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 Source<Profile>(original_profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(profile));
  // We can listen to everything for SITE_INSTANCE_DELETED because we check the
  // |site_instance_id| in UnregisterExtensionSiteInstance.
  registrar_.Add(this, content::NOTIFICATION_SITE_INSTANCE_DELETED,
                 NotificationService::AllBrowserContextsAndSources());
  // Same for NOTIFICATION_RENDERER_PROCESS_CLOSED.
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                 NotificationService::AllSources());
}

ExtensionProcessManager::~ExtensionProcessManager() {
  CloseBackgroundHosts();
  DCHECK(background_hosts_.empty());
}

ExtensionHost* ExtensionProcessManager::CreateViewHost(
    const Extension* extension,
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

ExtensionHost* ExtensionProcessManager::CreateViewHost(
    const GURL& url, Browser* browser, ViewType::Type view_type) {
  // A NULL browser may only be given for pop-up views.
  DCHECK(browser || (!browser && view_type == ViewType::EXTENSION_POPUP));
  Profile* profile =
      Profile::FromBrowserContext(browsing_instance_->browser_context());
  ExtensionService* service = profile->GetExtensionService();
  if (service) {
    const Extension* extension = service->GetExtensionByURL(url);
    if (extension)
      return CreateViewHost(extension, url, browser, view_type);
  }
  return NULL;
}

ExtensionHost* ExtensionProcessManager::CreatePopupHost(
    const Extension* extension, const GURL& url, Browser* browser) {
  return CreateViewHost(extension, url, browser, ViewType::EXTENSION_POPUP);
}

ExtensionHost* ExtensionProcessManager::CreatePopupHost(
    const GURL& url, Browser* browser) {
  return CreateViewHost(url, browser, ViewType::EXTENSION_POPUP);
}

ExtensionHost* ExtensionProcessManager::CreateDialogHost(
    const GURL& url, Browser* browser) {
  return CreateViewHost(url, browser, ViewType::EXTENSION_DIALOG);
}

ExtensionHost* ExtensionProcessManager::CreateInfobarHost(
    const Extension* extension, const GURL& url, Browser* browser) {
  return CreateViewHost(extension, url, browser, ViewType::EXTENSION_INFOBAR);
}

ExtensionHost* ExtensionProcessManager::CreateInfobarHost(
    const GURL& url, Browser* browser) {
  return CreateViewHost(url, browser, ViewType::EXTENSION_INFOBAR);
}

void ExtensionProcessManager::CreateBackgroundHost(
    const Extension* extension, const GURL& url) {
  // Hosted apps are taken care of from BackgroundContentsService. Ignore them
  // here.
  if (extension->is_hosted_app())
    return;

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
    Profile* profile =
        Profile::FromBrowserContext(browsing_instance_->browser_context());
    browser = Browser::GetOrCreateTabbedBrowser(profile->GetOriginalProfile());
  }

  browser->OpenURL(extension->options_url(), GURL(), SINGLETON_TAB,
                   PageTransition::LINK);
  browser->window()->Show();
  static_cast<RenderViewHostDelegate*>(browser->GetSelectedTabContents())->
      Activate();
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

void ExtensionProcessManager::RegisterExtensionSiteInstance(
    int site_instance_id, const std::string& extension_id) {
  SiteInstanceIDMap::const_iterator it = extension_ids_.find(site_instance_id);
  if (it != extension_ids_.end() && (*it).second == extension_id)
    return;

  // SiteInstance ids should get removed from the map before the extension ids
  // get used for a new SiteInstance.
  DCHECK(it == extension_ids_.end());
  extension_ids_[site_instance_id] = extension_id;
}

void ExtensionProcessManager::UnregisterExtensionSiteInstance(
    int site_instance_id) {
  SiteInstanceIDMap::iterator it = extension_ids_.find(site_instance_id);
  if (it != extension_ids_.end())
    extension_ids_.erase(it++);
}

void ExtensionProcessManager::RegisterProcessHost(int host_id) {
  process_ids_.insert(host_id);
}

void ExtensionProcessManager::UnregisterProcessHost(int host_id) {
  process_ids_.erase(host_id);
}

bool ExtensionProcessManager::IsExtensionProcessHost(int host_id) const {
  return process_ids_.find(host_id) != process_ids_.end();
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

const Extension* ExtensionProcessManager::GetExtensionForSiteInstance(
    int site_instance_id) {
  SiteInstanceIDMap::const_iterator it = extension_ids_.find(site_instance_id);
  if (it != extension_ids_.end()) {
    // Look up the extension by ID, including disabled extensions in case
    // this gets called while an old process is still around.
    Profile* profile =
        Profile::FromBrowserContext(browsing_instance_->browser_context());
    ExtensionService* service = profile->GetExtensionService();
    return service->GetExtensionById(it->second, false);
  }

  return NULL;
}

SiteInstance* ExtensionProcessManager::GetSiteInstanceForURL(const GURL& url) {
  return browsing_instance_->GetSiteInstanceForURL(url);
}

bool ExtensionProcessManager::HasExtensionHost(ExtensionHost* host) const {
  return all_hosts_.find(host) != all_hosts_.end();
}

void ExtensionProcessManager::Observe(int type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY: {
      CreateBackgroundHostsForProfileStartup(this,
          Source<Profile>(source).ptr()->GetExtensionService()->extensions());
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      ExtensionService* service =
          Source<Profile>(source).ptr()->GetExtensionService();
      if (service->is_ready()) {
        const Extension* extension = Details<const Extension>(details).ptr();
        ::CreateBackgroundHostForExtensionLoad(this, extension);
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          Details<UnloadedExtensionInfo>(details)->extension;
      for (ExtensionHostSet::iterator iter = background_hosts_.begin();
           iter != background_hosts_.end(); ++iter) {
        ExtensionHost* host = *iter;
        if (host->extension_id() == extension->id()) {
          delete host;
          // |host| should deregister itself from our structures.
          DCHECK(background_hosts_.find(host) == background_hosts_.end());
          break;
        }
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      ExtensionHost* host = Details<ExtensionHost>(details).ptr();
      all_hosts_.erase(host);
      background_hosts_.erase(host);
      break;
    }

    case content::NOTIFICATION_SITE_INSTANCE_DELETED: {
      SiteInstance* site_instance = Source<SiteInstance>(source).ptr();
      UnregisterExtensionSiteInstance(site_instance->id());
      break;
    }

    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      RenderProcessHost* process_host =
          Source<RenderProcessHost>(source).ptr();
      UnregisterProcessHost(process_host->id());
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
      ExtensionHost* host = Details<ExtensionHost>(details).ptr();
      if (host->extension_host_type() == ViewType::EXTENSION_BACKGROUND_PAGE) {
        delete host;
        // |host| should deregister itself from our structures.
        CHECK(background_hosts_.find(host) == background_hosts_.end());
      }
      break;
    }

    case content::NOTIFICATION_APP_TERMINATING: {
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
  DCHECK_EQ(browsing_instance_->browser_context(), host->profile());

  all_hosts_.insert(host);
  if (is_background)
    background_hosts_.insert(host);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_CREATED,
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

//
// IncognitoExtensionProcessManager
//

IncognitoExtensionProcessManager::IncognitoExtensionProcessManager(
    Profile* profile)
    : ExtensionProcessManager(profile),
      original_manager_(profile->GetOriginalProfile()->
                            GetExtensionProcessManager()) {
  DCHECK(profile->IsOffTheRecord());

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                 NotificationService::AllSources());
}

ExtensionHost* IncognitoExtensionProcessManager::CreateViewHost(
    const Extension* extension,
    const GURL& url,
    Browser* browser,
    ViewType::Type view_type) {
  if (extension->incognito_split_mode()) {
    if (IsIncognitoEnabled(extension)) {
      return ExtensionProcessManager::CreateViewHost(extension, url,
                                                     browser, view_type);
    } else {
      NOTREACHED() <<
          "We shouldn't be trying to create an incognito extension view unless "
          "it has been enabled for incognito.";
      return NULL;
    }
  } else {
    return original_manager_->CreateViewHost(extension, url,
                                             browser, view_type);
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

const Extension* IncognitoExtensionProcessManager::GetExtensionForSiteInstance(
    int site_instance_id) {
  const Extension* extension =
      ExtensionProcessManager::GetExtensionForSiteInstance(site_instance_id);
  if (extension && extension->incognito_split_mode()) {
    return extension;
  } else {
    return original_manager_->GetExtensionForSiteInstance(site_instance_id);
  }
}

const Extension* IncognitoExtensionProcessManager::GetExtensionOrAppByURL(
    const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(browsing_instance_->browser_context());
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return NULL;
  return (url.SchemeIs(chrome::kExtensionScheme)) ?
      service->GetExtensionByURL(url) : service->GetExtensionByWebExtent(url);
}

bool IncognitoExtensionProcessManager::IsIncognitoEnabled(
    const Extension* extension) {
  Profile* profile =
      Profile::FromBrowserContext(browsing_instance_->browser_context());
  ExtensionService* service = profile->GetExtensionService();
  return service && service->IsIncognitoEnabled(extension->id());
}

void IncognitoExtensionProcessManager::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_WINDOW_READY: {
      if (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableLazyBackgroundPages))
        break;
      // We want to spawn our background hosts as soon as the user opens an
      // incognito window. Watch for new browsers and create the hosts if
      // it matches our profile.
      Browser* browser = Source<Browser>(source).ptr();
      if (browser->profile() == browsing_instance_->browser_context()) {
        // On Chrome OS, a login screen is implemented as a browser.
        // This browser has no extension service.  In this case,
        // service will be NULL.
        Profile* profile =
            Profile::FromBrowserContext(browsing_instance_->browser_context());
        ExtensionService* service = profile->GetExtensionService();
        if (service && service->is_ready())
          CreateBackgroundHostsForProfileStartup(this, service->extensions());
      }
      break;
    }
    default:
      ExtensionProcessManager::Observe(type, source, details);
      break;
  }
}
