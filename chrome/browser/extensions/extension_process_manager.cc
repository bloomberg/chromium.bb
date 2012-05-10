// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"

#if defined(OS_MACOSX)
#include "chrome/browser/extensions/extension_host_mac.h"
#endif

using content::BrowserThread;
using content::OpenURLParams;
using content::Referrer;
using content::RenderViewHost;
using content::SiteInstance;

namespace {

std::string GetExtensionID(RenderViewHost* render_view_host) {
  // This works for both apps and extensions because the site has been
  // normalized to the extension URL for apps.
  if (!render_view_host->GetSiteInstance())
    return "";

  return render_view_host->GetSiteInstance()->GetSite().host();
}

// Incognito profiles use this process manager. It is mostly a shim that decides
// whether to fall back on the original profile's ExtensionProcessManager based
// on whether a given extension uses "split" or "spanning" incognito behavior.
class IncognitoExtensionProcessManager : public ExtensionProcessManager {
 public:
  explicit IncognitoExtensionProcessManager(Profile* profile);
  virtual ~IncognitoExtensionProcessManager() {}
  virtual ExtensionHost* CreateViewHost(
      const Extension* extension,
      const GURL& url,
      Browser* browser,
      content::ViewType view_type) OVERRIDE;
  virtual void CreateBackgroundHost(const Extension* extension,
                                    const GURL& url);
  virtual SiteInstance* GetSiteInstanceForURL(const GURL& url);

 private:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Returns true if the extension is allowed to run in incognito mode.
  bool IsIncognitoEnabled(const Extension* extension);

  ExtensionProcessManager* original_manager_;
};

static void CreateBackgroundHostForExtensionLoad(
    ExtensionProcessManager* manager, const Extension* extension) {
  if (extension->has_persistent_background_page()) {
    manager->CreateBackgroundHost(extension, extension->GetBackgroundURL());
  }
}

static void CreateBackgroundHostsForProfileStartup(
    ExtensionProcessManager* manager, const ExtensionSet* extensions) {
  for (ExtensionSet::const_iterator extension = extensions->begin();
       extension != extensions->end(); ++extension) {
    CreateBackgroundHostForExtensionLoad(manager, *extension);
  }
}

}  // namespace

struct ExtensionProcessManager::BackgroundPageData {
  // The count of things keeping the lazy background page alive.
  int lazy_keepalive_count;

  // This is used with the ShouldUnload message, to ensure that the extension
  // remained idle between sending the message and receiving the ack.
  int close_sequence_id;

  // True if the page responded to the ShouldUnload message and is currently
  // dispatching the unload event. We use this to ignore any activity
  // generated during the unload event that would otherwise keep the
  // extension alive.
  bool is_closing;

  BackgroundPageData()
      : lazy_keepalive_count(0), close_sequence_id(0), is_closing(false) {}
};

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
  : site_instance_(SiteInstance::Create(profile)),
    weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  Profile* original_profile = profile->GetOriginalProfile();
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(original_profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(original_profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(original_profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(profile));
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_DEVTOOLS_WINDOW_OPENING,
                 content::Source<content::BrowserContext>(profile));
  registrar_.Add(this, content::NOTIFICATION_DEVTOOLS_WINDOW_CLOSING,
                 content::Source<content::BrowserContext>(profile));

  event_page_idle_time_ = base::TimeDelta::FromSeconds(10);
  unsigned idle_time_sec = 0;
  if (base::StringToUint(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kEventPageIdleTime), &idle_time_sec)) {
    event_page_idle_time_ = base::TimeDelta::FromSeconds(idle_time_sec);
  }
  event_page_unloading_time_ = base::TimeDelta::FromSeconds(5);
  unsigned unloading_time_sec = 0;
  if (base::StringToUint(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kEventPageUnloadingTime), &unloading_time_sec)) {
    event_page_unloading_time_ = base::TimeDelta::FromSeconds(
        unloading_time_sec);
  }
}

ExtensionProcessManager::~ExtensionProcessManager() {
  CloseBackgroundHosts();
  DCHECK(background_hosts_.empty());
}

void ExtensionProcessManager::EnsureBrowserWhenRequired(
    Browser* browser,
    content::ViewType view_type) {
  if (!browser) {
#if defined (OS_CHROMEOS)
  // On ChromeOS we'll only use ExtensionView, which
  // does not use the browser parameter.
  // TODO(rkc): Remove all this once we create a new host for
  // screensaver extensions (crosbug.com/28211).
  DCHECK(view_type == chrome::VIEW_TYPE_EXTENSION_POPUP ||
         view_type == chrome::VIEW_TYPE_EXTENSION_DIALOG);
#else
  // A NULL browser may only be given for pop-up views.
  DCHECK(view_type == chrome::VIEW_TYPE_EXTENSION_POPUP);
#endif
  }
}


ExtensionHost* ExtensionProcessManager::CreateViewHost(
    const Extension* extension,
    const GURL& url,
    Browser* browser,
    content::ViewType view_type) {
  DCHECK(extension);
  EnsureBrowserWhenRequired(browser, view_type);
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
    const GURL& url, Browser* browser, content::ViewType view_type) {
  EnsureBrowserWhenRequired(browser, view_type);
  ExtensionService* service = GetProfile()->GetExtensionService();
  if (service) {
    const Extension* extension =
        service->extensions()->GetByID(url.host());
    if (extension)
      return CreateViewHost(extension, url, browser, view_type);
  }
  return NULL;
}

ExtensionHost* ExtensionProcessManager::CreatePopupHost(
    const Extension* extension, const GURL& url, Browser* browser) {
  return CreateViewHost(
      extension, url, browser, chrome::VIEW_TYPE_EXTENSION_POPUP);
}

ExtensionHost* ExtensionProcessManager::CreatePopupHost(
    const GURL& url, Browser* browser) {
  return CreateViewHost(url, browser, chrome::VIEW_TYPE_EXTENSION_POPUP);
}

ExtensionHost* ExtensionProcessManager::CreateDialogHost(
    const GURL& url, Browser* browser) {
  return CreateViewHost(url, browser, chrome::VIEW_TYPE_EXTENSION_DIALOG);
}

ExtensionHost* ExtensionProcessManager::CreateInfobarHost(
    const Extension* extension, const GURL& url, Browser* browser) {
  return CreateViewHost(
      extension, url, browser, chrome::VIEW_TYPE_EXTENSION_INFOBAR);
}

ExtensionHost* ExtensionProcessManager::CreateInfobarHost(
    const GURL& url, Browser* browser) {
  return CreateViewHost(url, browser, chrome::VIEW_TYPE_EXTENSION_INFOBAR);
}

void ExtensionProcessManager::CreateBackgroundHost(
    const Extension* extension, const GURL& url) {
  // Hosted apps are taken care of from BackgroundContentsService. Ignore them
  // here.
  if (extension->is_hosted_app())
    return;

  // Don't create multiple background hosts for an extension.
  if (GetBackgroundHostForExtension(extension->id()))
    return;

  ExtensionHost* host =
#if defined(OS_MACOSX)
      new ExtensionHostMac(extension, GetSiteInstanceForURL(url), url,
                           chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
#else
      new ExtensionHost(extension, GetSiteInstanceForURL(url), url,
                        chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
#endif

  host->CreateRenderViewSoon();
  OnExtensionHostCreated(host, true);
}

void ExtensionProcessManager::OpenOptionsPage(const Extension* extension,
                                              Browser* browser) {
  DCHECK(!extension->options_url().is_empty());

  // Force the options page to open in non-OTR window, because it won't be
  // able to save settings from OTR.
  if (!browser || browser->profile()->IsOffTheRecord()) {
    Profile* profile = GetProfile();
    browser = Browser::GetOrCreateTabbedBrowser(profile->GetOriginalProfile());
  }

  OpenURLParams params(extension->options_url(), Referrer(), SINGLETON_TAB,
                       content::PAGE_TRANSITION_LINK, false);
  browser->OpenURL(params);
  browser->window()->Show();
  browser->GetSelectedWebContents()->GetRenderViewHost()->GetDelegate()->
      Activate();
}

ExtensionHost* ExtensionProcessManager::GetBackgroundHostForExtension(
    const std::string& extension_id) {
  for (ExtensionHostSet::iterator iter = background_hosts_.begin();
       iter != background_hosts_.end(); ++iter) {
    ExtensionHost* host = *iter;
    if (host->extension_id() == extension_id)
      return host;
  }
  return NULL;
}

std::set<RenderViewHost*>
    ExtensionProcessManager::GetRenderViewHostsForExtension(
        const std::string& extension_id) {
  std::set<RenderViewHost*> result;

  SiteInstance* site_instance = GetSiteInstanceForURL(
      Extension::GetBaseURLFromExtensionId(extension_id));
  if (!site_instance)
    return result;

  // Gather up all the views for that site.
  for (ExtensionRenderViews::iterator view = all_extension_views_.begin();
       view != all_extension_views_.end(); ++view) {
    if (view->first->GetSiteInstance() == site_instance)
      result.insert(view->first);
  }

  return result;
}

const Extension* ExtensionProcessManager::GetExtensionForRenderViewHost(
    content::RenderViewHost* render_view_host) {
  if (!render_view_host->GetSiteInstance())
    return NULL;

  ExtensionService* service =
      ExtensionSystem::Get(GetProfile())->extension_service();
  return service->extensions()->GetByID(GetExtensionID(render_view_host));
}

void ExtensionProcessManager::RegisterRenderViewHost(
    RenderViewHost* render_view_host,
    const Extension* extension) {
  all_extension_views_[render_view_host] = content::VIEW_TYPE_INVALID;
}

void ExtensionProcessManager::UnregisterRenderViewHost(
    RenderViewHost* render_view_host) {
  ExtensionRenderViews::iterator view =
      all_extension_views_.find(render_view_host);
  if (view == all_extension_views_.end())
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED,
      content::Source<Profile>(GetProfile()),
      content::Details<RenderViewHost>(render_view_host));

  content::ViewType view_type = view->second;
  all_extension_views_.erase(view);

  // Keepalive count, balanced in UpdateRegisteredRenderView.
  if (view_type != content::VIEW_TYPE_INVALID &&
      view_type != chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    const Extension* extension = GetExtensionForRenderViewHost(
        render_view_host);
    if (extension)
      DecrementLazyKeepaliveCount(extension);
  }
}

void ExtensionProcessManager::UpdateRegisteredRenderView(
    RenderViewHost* render_view_host) {
  ExtensionRenderViews::iterator view =
      all_extension_views_.find(render_view_host);
  if (view == all_extension_views_.end())
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_VIEW_REGISTERED,
      content::Source<Profile>(GetProfile()),
      content::Details<RenderViewHost>(render_view_host));

  view->second = render_view_host->GetDelegate()->GetRenderViewType();

  // Keep the lazy background page alive as long as any non-background-page
  // extension views are visible. Keepalive count balanced in
  // UnregisterRenderViewHost.
  IncrementLazyKeepaliveCountForView(render_view_host);
}

SiteInstance* ExtensionProcessManager::GetSiteInstanceForURL(const GURL& url) {
  return site_instance_->GetRelatedSiteInstance(url);
}

bool ExtensionProcessManager::IsBackgroundHostClosing(
    const std::string& extension_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  return (host && background_page_data_[extension_id].is_closing);
}

int ExtensionProcessManager::GetLazyKeepaliveCount(const Extension* extension) {
  if (!extension->has_lazy_background_page())
    return 0;

  return background_page_data_[extension->id()].lazy_keepalive_count;
}

int ExtensionProcessManager::IncrementLazyKeepaliveCount(
     const Extension* extension) {
  if (!extension->has_lazy_background_page())
    return 0;

  int& count = background_page_data_[extension->id()].lazy_keepalive_count;
  if (++count == 1)
    OnLazyBackgroundPageActive(extension->id());

  return count;
}

int ExtensionProcessManager::DecrementLazyKeepaliveCount(
     const Extension* extension) {
  if (!extension->has_lazy_background_page())
    return 0;

  int& count = background_page_data_[extension->id()].lazy_keepalive_count;
  DCHECK_GT(count, 0);
  if (--count == 0) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ExtensionProcessManager::OnLazyBackgroundPageIdle,
                   weak_ptr_factory_.GetWeakPtr(), extension->id(),
                   ++background_page_data_[extension->id()].close_sequence_id),
        event_page_idle_time_);
  }

  return count;
}
void ExtensionProcessManager::IncrementLazyKeepaliveCountForView(
    RenderViewHost* render_view_host) {
  content::ViewType view_type =
      render_view_host->GetDelegate()->GetRenderViewType();
  if (view_type != content::VIEW_TYPE_INVALID &&
      view_type != chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    const Extension* extension = GetExtensionForRenderViewHost(
        render_view_host);
    if (extension)
      IncrementLazyKeepaliveCount(extension);
  }
}

void ExtensionProcessManager::OnLazyBackgroundPageIdle(
    const std::string& extension_id, int sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host && !background_page_data_[extension_id].is_closing &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    // Tell the renderer we are about to close. This is a simple ping that the
    // renderer will respond to. The purpose is to control sequencing: if the
    // extension remains idle until the renderer responds with an ACK, then we
    // know that the extension process is ready to shut down. If our
    // close_sequence_id has already changed, then we would ignore the
    // ShouldUnloadAck, so we don't send the ping.
    host->render_view_host()->Send(new ExtensionMsg_ShouldUnload(
        extension_id, sequence_id));
  }
}

void ExtensionProcessManager::OnLazyBackgroundPageActive(
    const std::string& extension_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host && !background_page_data_[extension_id].is_closing) {
    // Cancel the current close sequence by changing the close_sequence_id,
    // which causes us to ignore the next ShouldUnloadAck.
    ++background_page_data_[extension_id].close_sequence_id;
  }
}

void ExtensionProcessManager::OnShouldUnloadAck(
     const std::string& extension_id, int sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    background_page_data_[extension_id].is_closing = true;
    host->render_view_host()->Send(new ExtensionMsg_Unload(extension_id));
  }
}

void ExtensionProcessManager::OnUnloadAck(const std::string& extension_id) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ExtensionProcessManager::CloseLazyBackgroundPageNow,
                 weak_ptr_factory_.GetWeakPtr(), extension_id),
      event_page_unloading_time_);
}

void ExtensionProcessManager::CloseLazyBackgroundPageNow(
    const std::string& extension_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host)
    CloseBackgroundHost(host);
}

void ExtensionProcessManager::OnNetworkRequestStarted(
    RenderViewHost* render_view_host) {
  ExtensionHost* host = GetBackgroundHostForExtension(
      GetExtensionID(render_view_host));
  if (host && host->render_view_host() == render_view_host)
    IncrementLazyKeepaliveCount(host->extension());
}

void ExtensionProcessManager::OnNetworkRequestDone(
    RenderViewHost* render_view_host) {
  ExtensionHost* host = GetBackgroundHostForExtension(
      GetExtensionID(render_view_host));
  if (host && host->render_view_host() == render_view_host)
    DecrementLazyKeepaliveCount(host->extension());
}

void ExtensionProcessManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY: {
      CreateBackgroundHostsForProfileStartup(this,
          content::Source<Profile>(source).ptr()->
              GetExtensionService()->extensions());
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      ExtensionService* service =
          content::Source<Profile>(source).ptr()->GetExtensionService();
      if (service->is_ready()) {
        const Extension* extension =
            content::Details<const Extension>(details).ptr();
        ::CreateBackgroundHostForExtensionLoad(this, extension);
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      for (ExtensionHostSet::iterator iter = background_hosts_.begin();
           iter != background_hosts_.end(); ++iter) {
        ExtensionHost* host = *iter;
        if (host->extension_id() == extension->id()) {
          CloseBackgroundHost(host);
          break;
        }
      }
      background_page_data_.erase(extension->id());
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      if (background_hosts_.erase(host))
        ClearBackgroundPageData(host->extension()->id());
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      if (host->extension_host_type() ==
          chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
        CloseBackgroundHost(host);
      }
      break;
    }

    case content::NOTIFICATION_WEB_CONTENTS_CONNECTED: {
      content::WebContents* contents =
          content::Source<content::WebContents>(source).ptr();
      UpdateRegisteredRenderView(contents->GetRenderViewHost());
      break;
    }

    case content::NOTIFICATION_APP_TERMINATING: {
      // Close background hosts when the last browser is closed so that they
      // have time to shutdown various objects on different threads. Our
      // destructor is called too late in the shutdown sequence.
      CloseBackgroundHosts();
      break;
    }

    case content::NOTIFICATION_DEVTOOLS_WINDOW_OPENING: {
      RenderViewHost* render_view_host =
          content::Details<RenderViewHost>(details).ptr();
      // Keep the lazy background page alive while it's being inspected.
      // Balanced in response to the CLOSING notification.
      if (render_view_host->GetDelegate()->GetRenderViewType() ==
              chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
        const Extension* extension = GetExtensionForRenderViewHost(
            render_view_host);
        if (extension)
          IncrementLazyKeepaliveCount(extension);
      }
      break;
    }

    case content::NOTIFICATION_DEVTOOLS_WINDOW_CLOSING: {
      RenderViewHost* render_view_host =
          content::Details<RenderViewHost>(details).ptr();
      // Balanced in response to the OPENING notification.
      if (render_view_host->GetDelegate()->GetRenderViewType() ==
              chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
        const Extension* extension = GetExtensionForRenderViewHost(
            render_view_host);
        if (extension)
          DecrementLazyKeepaliveCount(extension);
      }
      break;
    }

    default:
      NOTREACHED();
  }
}

Profile* ExtensionProcessManager::GetProfile() const {
  return Profile::FromBrowserContext(site_instance_->GetBrowserContext());
}

void ExtensionProcessManager::OnExtensionHostCreated(ExtensionHost* host,
                                                     bool is_background) {
  DCHECK_EQ(site_instance_->GetBrowserContext(), host->profile());
  if (is_background)
    background_hosts_.insert(host);
}

void ExtensionProcessManager::CloseBackgroundHost(ExtensionHost* host) {
  CHECK(host->extension_host_type() ==
        chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  delete host;
  // |host| should deregister itself from our structures.
  CHECK(background_hosts_.find(host) == background_hosts_.end());
}

void ExtensionProcessManager::CloseBackgroundHosts() {
  for (ExtensionHostSet::iterator iter = background_hosts_.begin();
       iter != background_hosts_.end(); ) {
    ExtensionHostSet::iterator current = iter++;
    delete *current;
  }
}

void ExtensionProcessManager::ClearBackgroundPageData(
    const std::string& extension_id) {
  background_page_data_.erase(extension_id);

  // Re-register all RenderViews for this extension. We do this to restore
  // the lazy_keepalive_count (if any) to properly reflect the number of open
  // views.
  for (ExtensionRenderViews::const_iterator it = all_extension_views_.begin();
       it != all_extension_views_.end(); ++it) {
    if (GetExtensionID(it->first) == extension_id)
      IncrementLazyKeepaliveCountForView(it->first);
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
                 content::NotificationService::AllSources());
}

const ExtensionProcessManager::ViewSet
ExtensionProcessManager::GetAllViews() const {
  ViewSet result;
  for (ExtensionRenderViews::const_iterator iter =
           all_extension_views_.begin();
       iter != all_extension_views_.end(); ++iter) {
    result.insert(iter->first);
  }
  return result;
}

ExtensionHost* IncognitoExtensionProcessManager::CreateViewHost(
    const Extension* extension,
    const GURL& url,
    Browser* browser,
    content::ViewType view_type) {
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
  ExtensionService* service = GetProfile()->GetExtensionService();
  if (service) {
    const Extension* extension = service->extensions()->GetExtensionOrAppByURL(
        ExtensionURLInfo(url));
    if (extension && !extension->incognito_split_mode())
      return original_manager_->GetSiteInstanceForURL(url);
  }
  return ExtensionProcessManager::GetSiteInstanceForURL(url);
}

bool IncognitoExtensionProcessManager::IsIncognitoEnabled(
    const Extension* extension) {
  // Keep in sync with duplicate in extension_info_map.cc.
  ExtensionService* service = GetProfile()->GetExtensionService();
  return service && service->IsIncognitoEnabled(extension->id());
}

void IncognitoExtensionProcessManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_WINDOW_READY: {
      // We want to spawn our background hosts as soon as the user opens an
      // incognito window. Watch for new browsers and create the hosts if
      // it matches our profile.
      Browser* browser = content::Source<Browser>(source).ptr();
      if (browser->profile() == site_instance_->GetBrowserContext()) {
        // On Chrome OS, a login screen is implemented as a browser.
        // This browser has no extension service.  In this case,
        // service will be NULL.
        ExtensionService* service = GetProfile()->GetExtensionService();
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
