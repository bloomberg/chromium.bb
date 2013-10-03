// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/tab_contents_resource_provider.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/background_printing_manager.h"
#endif

using content::WebContents;
using extensions::Extension;

namespace {

bool IsContentsPrerendering(WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile);
  return prerender_manager &&
         prerender_manager->IsWebContentsPrerendering(web_contents, NULL);
}

bool IsContentsBackgroundPrinted(WebContents* web_contents) {
#if defined(ENABLE_FULL_PRINTING)
  printing::BackgroundPrintingManager* printing_manager =
      g_browser_process->background_printing_manager();
  return printing_manager->HasPrintPreviewDialog(web_contents);
#else
  return false;
#endif
}

}  // namespace

namespace task_manager {

// Tracks a single tab contents, prerendered page, Instant page, or background
// printing page.
class TabContentsResource : public RendererResource {
 public:
  explicit TabContentsResource(content::WebContents* web_contents);
  virtual ~TabContentsResource();

  // Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual const extensions::Extension* GetExtension() const OVERRIDE;

 private:
  // Returns true if contains content rendered by an extension.
  bool HostsExtension() const;

  static gfx::ImageSkia* prerender_icon_;
  content::WebContents* web_contents_;
  Profile* profile_;
  bool is_instant_ntp_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsResource);
};

gfx::ImageSkia* TabContentsResource::prerender_icon_ = NULL;

TabContentsResource::TabContentsResource(
    WebContents* web_contents)
    : RendererResource(web_contents->GetRenderProcessHost()->GetHandle(),
                       web_contents->GetRenderViewHost()),
      web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      is_instant_ntp_(chrome::IsPreloadedInstantExtendedNTP(web_contents)) {
  if (!prerender_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    prerender_icon_ = rb.GetImageSkiaNamed(IDR_PRERENDER);
  }
}

TabContentsResource::~TabContentsResource() {
}

bool TabContentsResource::HostsExtension() const {
  return web_contents_->GetURL().SchemeIs(extensions::kExtensionScheme);
}

Resource::Type TabContentsResource::GetType() const {
  return HostsExtension() ? EXTENSION : RENDERER;
}

string16 TabContentsResource::GetTitle() const {
  // Fall back on the URL if there's no title.
  GURL url = web_contents_->GetURL();
  string16 tab_title = util::GetTitleFromWebContents(web_contents_);

  // Only classify as an app if the URL is an app and the tab is hosting an
  // extension process.  (It's possible to be showing the URL from before it
  // was installed as an app.)
  ExtensionService* extension_service = profile_->GetExtensionService();
  extensions::ProcessMap* process_map = extension_service->process_map();
  bool is_app = extension_service->IsInstalledApp(url) &&
      process_map->Contains(web_contents_->GetRenderProcessHost()->GetID());

  int message_id = util::GetMessagePrefixID(
      is_app,
      HostsExtension(),
      profile_->IsOffTheRecord(),
      IsContentsPrerendering(web_contents_),
      is_instant_ntp_,
      false);  // is_background
  return l10n_util::GetStringFUTF16(message_id, tab_title);
}

string16 TabContentsResource::GetProfileName() const {
  return util::GetProfileNameFromInfoCache(profile_);
}

gfx::ImageSkia TabContentsResource::GetIcon() const {
  if (IsContentsPrerendering(web_contents_))
    return *prerender_icon_;
  FaviconTabHelper::CreateForWebContents(web_contents_);
  return FaviconTabHelper::FromWebContents(web_contents_)->
      GetFavicon().AsImageSkia();
}

WebContents* TabContentsResource::GetWebContents() const {
  return web_contents_;
}

const Extension* TabContentsResource::GetExtension() const {
  if (HostsExtension()) {
    ExtensionService* extension_service = profile_->GetExtensionService();
    return extension_service->extensions()->GetByID(
        web_contents_->GetURL().host());
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TabContentsResourceProvider::
    TabContentsResourceProvider(TaskManager* task_manager)
    :  updating_(false),
       task_manager_(task_manager) {
}

TabContentsResourceProvider::~TabContentsResourceProvider() {
}

Resource* TabContentsResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_host_id, routing_id);
  if (!web_contents)  // Not one of our resource.
    return NULL;

  // If an origin PID was specified then the request originated in a plugin
  // working on the WebContents's behalf, so ignore it.
  if (origin_pid)
    return NULL;

  std::map<WebContents*, TabContentsResource*>::iterator
      res_iter = resources_.find(web_contents);
  if (res_iter == resources_.end()) {
    // Can happen if the tab was closed while a network request was being
    // performed.
    return NULL;
  }
  return res_iter->second;
}

void TabContentsResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // The contents that are tracked by this resource provider are those that
  // are tab contents (WebContents serving as a tab in a Browser), Instant
  // pages, prerender pages, and background printed pages.

  // Add all the existing WebContentses.
  for (TabContentsIterator iterator; !iterator.done(); iterator.Next()) {
    Add(*iterator);
    DevToolsWindow* docked =
        DevToolsWindow::GetDockedInstanceForInspectedTab(*iterator);
    if (docked)
      Add(docked->web_contents());
  }

  // Add all the prerender pages.
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i) {
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForProfile(profiles[i]);
    if (prerender_manager) {
      const std::vector<content::WebContents*> contentses =
          prerender_manager->GetAllPrerenderingContents();
      for (size_t j = 0; j < contentses.size(); ++j)
        Add(contentses[j]);
    }
  }

  // Add all the Instant Extended prerendered NTPs.
  for (size_t i = 0; i < profiles.size(); ++i) {
    const InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profiles[i]);
    if (instant_service && instant_service->GetNTPContents())
      Add(instant_service->GetNTPContents());
  }

#if defined(ENABLE_FULL_PRINTING)
  // Add all the pages being background printed.
  printing::BackgroundPrintingManager* printing_manager =
      g_browser_process->background_printing_manager();
  for (printing::BackgroundPrintingManager::WebContentsSet::iterator i =
           printing_manager->begin();
       i != printing_manager->end(); ++i) {
    Add(*i);
  }
#endif

  // Then we register for notifications to get new web contents.
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void TabContentsResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Then we unregister for notifications to get new web contents.
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
}

void TabContentsResourceProvider::AddToTaskManager(WebContents* web_contents) {
  TabContentsResource* resource = new TabContentsResource(web_contents);
  resources_[web_contents] = resource;
  task_manager_->AddResource(resource);
}

void TabContentsResourceProvider::Add(WebContents* web_contents) {
  if (!updating_)
    return;

  // The contents that are tracked by this resource provider are those that
  // are tab contents (WebContents serving as a tab in a Browser), Instant
  // pages, prerender pages, and background printed pages.
  if (!chrome::FindBrowserWithWebContents(web_contents) &&
      !IsContentsPrerendering(web_contents) &&
      !chrome::IsPreloadedInstantExtendedNTP(web_contents) &&
      !IsContentsBackgroundPrinted(web_contents) &&
      !DevToolsWindow::IsDevToolsWindow(web_contents->GetRenderViewHost())) {
    return;
  }

  // Don't add dead tabs or tabs that haven't yet connected.
  if (!web_contents->GetRenderProcessHost()->GetHandle() ||
      !web_contents->WillNotifyDisconnection()) {
    return;
  }

  if (resources_.count(web_contents)) {
    // The case may happen that we have added a WebContents as part of the
    // iteration performed during StartUpdating() call but the notification that
    // it has connected was not fired yet. So when the notification happens, we
    // already know about this tab and just ignore it.
    return;
  }
  AddToTaskManager(web_contents);
}

void TabContentsResourceProvider::Remove(WebContents* web_contents) {
  if (!updating_)
    return;
  std::map<WebContents*, TabContentsResource*>::iterator
      iter = resources_.find(web_contents);
  if (iter == resources_.end()) {
    // Since WebContents are destroyed asynchronously (see TabContentsCollector
    // in navigation_controller.cc), we can be notified of a tab being removed
    // that we don't know.  This can happen if the user closes a tab and quickly
    // opens the task manager, before the tab is actually destroyed.
    return;
  }

  // Remove the resource from the Task Manager.
  TabContentsResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  // And from the provider.
  resources_.erase(iter);
  // Finally, delete the resource.
  delete resource;
}

void TabContentsResourceProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  WebContents* web_contents = content::Source<WebContents>(source).ptr();

  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_CONNECTED:
      Add(web_contents);
      break;
    case content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED:
      Remove(web_contents);
      Add(web_contents);
      break;
    case content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED:
      Remove(web_contents);
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

}  // namespace task_manager
