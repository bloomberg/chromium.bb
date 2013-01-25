// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_resource_providers.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/file_version_info.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "extensions/common/constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/sqlite/sqlite3.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "v8/include/v8.h"

#if defined(OS_MACOSX)
#include "ui/gfx/image/image_skia_util_mac.h"
#endif
#if defined(OS_WIN)
#include "chrome/browser/app_icon_win.h"
#include "ui/gfx/icon_util.h"
#endif  // defined(OS_WIN)

using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::RenderProcessHost;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;
using extensions::Extension;

namespace {

// Returns the appropriate message prefix ID for tabs and extensions,
// reflecting whether they are apps or in incognito mode.
int GetMessagePrefixID(bool is_app,
                       bool is_extension,
                       bool is_incognito,
                       bool is_prerender,
                       bool is_instant_preview,
                       bool is_background) {
  if (is_app) {
    if (is_background) {
      return IDS_TASK_MANAGER_BACKGROUND_PREFIX;
    } else if (is_incognito) {
      return IDS_TASK_MANAGER_APP_INCOGNITO_PREFIX;
    } else {
      return IDS_TASK_MANAGER_APP_PREFIX;
    }
  } else if (is_extension) {
    if (is_incognito)
      return IDS_TASK_MANAGER_EXTENSION_INCOGNITO_PREFIX;
    else
      return IDS_TASK_MANAGER_EXTENSION_PREFIX;
  } else if (is_prerender) {
    return IDS_TASK_MANAGER_PRERENDER_PREFIX;
  } else if (is_instant_preview) {
    return IDS_TASK_MANAGER_INSTANT_PREVIEW_PREFIX;
  } else {
    return IDS_TASK_MANAGER_TAB_PREFIX;
  }
}

string16 GetProfileNameFromInfoCache(Profile* profile) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(
      profile->GetOriginalProfile()->GetPath());
  if (index == std::string::npos)
    return string16();
  else
    return cache.GetNameOfProfileAtIndex(index);
}

string16 GetTitleFromWebContents(WebContents* web_contents) {
  string16 title = web_contents->GetTitle();
  if (title.empty()) {
    GURL url = web_contents->GetURL();
    title = UTF8ToUTF16(url.spec());
    // Force URL to be LTR.
    title = base::i18n::GetDisplayStringInLTRDirectionality(title);
  } else {
    // Since the tab_title will be concatenated with
    // IDS_TASK_MANAGER_TAB_PREFIX, we need to explicitly set the tab_title to
    // be LTR format if there is no strong RTL charater in it. Otherwise, if
    // IDS_TASK_MANAGER_TAB_PREFIX is an RTL word, the concatenated result
    // might be wrong. For example, http://mail.yahoo.com, whose title is
    // "Yahoo! Mail: The best web-based Email!", without setting it explicitly
    // as LTR format, the concatenated result will be "!Yahoo! Mail: The best
    // web-based Email :BAT", in which the capital letters "BAT" stands for
    // the Hebrew word for "tab".
    base::i18n::AdjustStringForLocaleDirection(&title);
  }
  return title;
}

bool IsContentsPrerendering(WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile);
  return prerender_manager &&
         prerender_manager->IsWebContentsPrerendering(web_contents, NULL);
}

bool IsContentsInstant(WebContents* web_contents) {
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->instant_controller() &&
        (*i)->instant_controller()->instant()->
            GetPreviewContents() == web_contents) {
      return true;
    }
  }

  return false;
}

bool IsContentsBackgroundPrinted(WebContents* web_contents) {
  printing::BackgroundPrintingManager* printing_manager =
      g_browser_process->background_printing_manager();
  return printing_manager->HasPrintPreviewDialog(web_contents);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TaskManagerRendererResource class
////////////////////////////////////////////////////////////////////////////////
TaskManagerRendererResource::TaskManagerRendererResource(
    base::ProcessHandle process, content::RenderViewHost* render_view_host)
    : content::RenderViewHostObserver(render_view_host),
      process_(process),
      render_view_host_(render_view_host),
      pending_stats_update_(false),
      fps_(0.0f),
      pending_fps_update_(false),
      v8_memory_allocated_(0),
      v8_memory_used_(0),
      pending_v8_memory_allocated_update_(false) {
  // We cache the process and pid as when a Tab/BackgroundContents is closed the
  // process reference becomes NULL and the TaskManager still needs it.
  pid_ = base::GetProcId(process_);
  unique_process_id_ = render_view_host_->GetProcess()->GetID();
  memset(&stats_, 0, sizeof(stats_));
}

TaskManagerRendererResource::~TaskManagerRendererResource() {
}

void TaskManagerRendererResource::Refresh() {
  if (!render_view_host()) {
    // Store information about where this TaskManagerRendererResource was
    // created and where the RenderViewHost was destroyed on the stack to help
    // debugging where we forgot to delete this resource.
    base::debug::StackTrace creation_stack = creation_stack_;
    base::debug::Alias(&creation_stack);
    CHECK(destruction_stack_);
    base::debug::StackTrace destruction_stack = *destruction_stack_;
    base::debug::Alias(&destruction_stack);
    CHECK(false);
  }

  if (!pending_stats_update_) {
    render_view_host_->Send(new ChromeViewMsg_GetCacheResourceStats);
    pending_stats_update_ = true;
  }
  if (!pending_fps_update_) {
    render_view_host_->Send(
        new ChromeViewMsg_GetFPS(render_view_host_->GetRoutingID()));
    pending_fps_update_ = true;
  }
  if (!pending_v8_memory_allocated_update_) {
    render_view_host_->Send(new ChromeViewMsg_GetV8HeapStats);
    pending_v8_memory_allocated_update_ = true;
  }
}

WebKit::WebCache::ResourceTypeStats
TaskManagerRendererResource::GetWebCoreCacheStats() const {
  return stats_;
}

float TaskManagerRendererResource::GetFPS() const {
  return fps_;
}

size_t TaskManagerRendererResource::GetV8MemoryAllocated() const {
  return v8_memory_allocated_;
}

size_t TaskManagerRendererResource::GetV8MemoryUsed() const {
  return v8_memory_used_;
}

void TaskManagerRendererResource::NotifyResourceTypeStats(
    const WebKit::WebCache::ResourceTypeStats& stats) {
  stats_ = stats;
  pending_stats_update_ = false;
}

void TaskManagerRendererResource::NotifyFPS(float fps) {
  fps_ = fps;
  pending_fps_update_ = false;
}

void TaskManagerRendererResource::NotifyV8HeapStats(
    size_t v8_memory_allocated, size_t v8_memory_used) {
  v8_memory_allocated_ = v8_memory_allocated;
  v8_memory_used_ = v8_memory_used;
  pending_v8_memory_allocated_update_ = false;
}

base::ProcessHandle TaskManagerRendererResource::GetProcess() const {
  return process_;
}

int TaskManagerRendererResource::GetUniqueChildProcessId() const {
  return unique_process_id_;
}

TaskManager::Resource::Type TaskManagerRendererResource::GetType() const {
  return RENDERER;
}

int TaskManagerRendererResource::GetRoutingID() const {
  return render_view_host_->GetRoutingID();
}

bool TaskManagerRendererResource::ReportsCacheStats() const {
  return true;
}

bool TaskManagerRendererResource::ReportsFPS() const {
  return true;
}

bool TaskManagerRendererResource::ReportsV8MemoryStats() const {
  return true;
}

bool TaskManagerRendererResource::CanInspect() const {
  return true;
}

void TaskManagerRendererResource::Inspect() const {
  DevToolsWindow::OpenDevToolsWindow(render_view_host_);
}

bool TaskManagerRendererResource::SupportNetworkUsage() const {
  return true;
}

void TaskManagerRendererResource::RenderViewHostDestroyed(
    content::RenderViewHost* render_view_host) {
  destruction_stack_.reset(new base::debug::StackTrace());
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerTabContentsResource class
////////////////////////////////////////////////////////////////////////////////

// static
gfx::ImageSkia* TaskManagerTabContentsResource::prerender_icon_ = NULL;

TaskManagerTabContentsResource::TaskManagerTabContentsResource(
    WebContents* web_contents)
    : TaskManagerRendererResource(
          web_contents->GetRenderProcessHost()->GetHandle(),
          web_contents->GetRenderViewHost()),
      web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      is_instant_preview_(IsContentsInstant(web_contents)) {
  if (!prerender_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    prerender_icon_ = rb.GetImageSkiaNamed(IDR_PRERENDER);
  }
}

TaskManagerTabContentsResource::~TaskManagerTabContentsResource() {
}

void TaskManagerTabContentsResource::InstantCommitted() {
  DCHECK(is_instant_preview_);
  is_instant_preview_ = false;
}

bool TaskManagerTabContentsResource::HostsExtension() const {
  return web_contents_->GetURL().SchemeIs(extensions::kExtensionScheme);
}

TaskManager::Resource::Type TaskManagerTabContentsResource::GetType() const {
  return HostsExtension() ? EXTENSION : RENDERER;
}

string16 TaskManagerTabContentsResource::GetTitle() const {
  // Fall back on the URL if there's no title.
  GURL url = web_contents_->GetURL();
  string16 tab_title = GetTitleFromWebContents(web_contents_);

  // Only classify as an app if the URL is an app and the tab is hosting an
  // extension process.  (It's possible to be showing the URL from before it
  // was installed as an app.)
  ExtensionService* extension_service = profile_->GetExtensionService();
  extensions::ProcessMap* process_map = extension_service->process_map();
  bool is_app = extension_service->IsInstalledApp(url) &&
      process_map->Contains(web_contents_->GetRenderProcessHost()->GetID());

  int message_id = GetMessagePrefixID(
      is_app,
      HostsExtension(),
      profile_->IsOffTheRecord(),
      IsContentsPrerendering(web_contents_),
      is_instant_preview_,
      false);
  return l10n_util::GetStringFUTF16(message_id, tab_title);
}

string16 TaskManagerTabContentsResource::GetProfileName() const {
  return GetProfileNameFromInfoCache(profile_);
}

gfx::ImageSkia TaskManagerTabContentsResource::GetIcon() const {
  if (IsContentsPrerendering(web_contents_))
    return *prerender_icon_;
  return FaviconTabHelper::FromWebContents(web_contents_)->
      GetFavicon().AsImageSkia();
}

WebContents* TaskManagerTabContentsResource::GetWebContents() const {
  return web_contents_;
}

const Extension* TaskManagerTabContentsResource::GetExtension() const {
  if (HostsExtension()) {
    ExtensionService* extension_service = profile_->GetExtensionService();
    return extension_service->extensions()->GetByID(
        web_contents_->GetURL().host());
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerTabContentsResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerTabContentsResourceProvider::
    TaskManagerTabContentsResourceProvider(TaskManager* task_manager)
    :  updating_(false),
       task_manager_(task_manager) {
}

TaskManagerTabContentsResourceProvider::
    ~TaskManagerTabContentsResourceProvider() {
}

TaskManager::Resource* TaskManagerTabContentsResourceProvider::GetResource(
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

  std::map<WebContents*, TaskManagerTabContentsResource*>::iterator
      res_iter = resources_.find(web_contents);
  if (res_iter == resources_.end()) {
    // Can happen if the tab was closed while a network request was being
    // performed.
    return NULL;
  }
  return res_iter->second;
}

void TaskManagerTabContentsResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // The contents that are tracked by this resource provider are those that
  // are tab contents (WebContents serving as a tab in a Browser), instant
  // pages, prerender pages, and background printed pages.

  // Add all the existing WebContentses.
  for (TabContentsIterator iterator; !iterator.done(); ++iterator)
    Add(*iterator);

  // Add all the instant pages.
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->instant_controller() &&
        (*i)->instant_controller()->instant()->GetPreviewContents()) {
      Add((*i)->instant_controller()->instant()->GetPreviewContents());
    }
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

  // Add all the pages being background printed.
  printing::BackgroundPrintingManager* printing_manager =
      g_browser_process->background_printing_manager();
  for (printing::BackgroundPrintingManager::WebContentsSet::iterator i =
           printing_manager->begin();
       i != printing_manager->end(); ++i) {
    Add(*i);
  }

  // Then we register for notifications to get new web contents.
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_SWAPPED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_INSTANT_COMMITTED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void TaskManagerTabContentsResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Then we unregister for notifications to get new web contents.
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_SWAPPED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(this, chrome::NOTIFICATION_INSTANT_COMMITTED,
      content::NotificationService::AllBrowserContextsAndSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
}

void TaskManagerTabContentsResourceProvider::AddToTaskManager(
    WebContents* web_contents) {
  TaskManagerTabContentsResource* resource =
      new TaskManagerTabContentsResource(web_contents);
  resources_[web_contents] = resource;
  task_manager_->AddResource(resource);
}

void TaskManagerTabContentsResourceProvider::Add(WebContents* web_contents) {
  if (!updating_)
    return;

  // The contents that are tracked by this resource provider are those that
  // are tab contents (WebContents serving as a tab in a Browser), instant
  // pages, prerender pages, and background printed pages.
  if (!chrome::FindBrowserWithWebContents(web_contents) &&
      !IsContentsPrerendering(web_contents) &&
      !IsContentsInstant(web_contents) &&
      !IsContentsBackgroundPrinted(web_contents)) {
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

void TaskManagerTabContentsResourceProvider::Remove(WebContents* web_contents) {
  if (!updating_)
    return;
  std::map<WebContents*, TaskManagerTabContentsResource*>::iterator
      iter = resources_.find(web_contents);
  if (iter == resources_.end()) {
    // Since WebContents are destroyed asynchronously (see TabContentsCollector
    // in navigation_controller.cc), we can be notified of a tab being removed
    // that we don't know.  This can happen if the user closes a tab and quickly
    // opens the task manager, before the tab is actually destroyed.
    return;
  }

  // Remove the resource from the Task Manager.
  TaskManagerTabContentsResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  // And from the provider.
  resources_.erase(iter);
  // Finally, delete the resource.
  delete resource;
}

void TaskManagerTabContentsResourceProvider::InstantCommitted(
    WebContents* web_contents) {
  if (!updating_)
    return;
  std::map<WebContents*, TaskManagerTabContentsResource*>::iterator
      iter = resources_.find(web_contents);
  DCHECK(iter != resources_.end());
  if (iter != resources_.end())
    iter->second->InstantCommitted();
}

void TaskManagerTabContentsResourceProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  WebContents* web_contents = content::Source<WebContents>(source).ptr();

  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_CONNECTED:
      Add(web_contents);
      break;
    case content::NOTIFICATION_WEB_CONTENTS_SWAPPED:
      Remove(web_contents);
      Add(web_contents);
      break;
    case content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED:
      Remove(web_contents);
      break;
    case chrome::NOTIFICATION_INSTANT_COMMITTED:
      InstantCommitted(web_contents);
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerPanelResource class
////////////////////////////////////////////////////////////////////////////////

TaskManagerPanelResource::TaskManagerPanelResource(Panel* panel)
    : TaskManagerRendererResource(
        panel->GetWebContents()->GetRenderProcessHost()->GetHandle(),
        panel->GetWebContents()->GetRenderViewHost()),
      panel_(panel) {
  message_prefix_id_ = GetMessagePrefixID(
      GetExtension()->is_app(), true, panel->profile()->IsOffTheRecord(),
      false, false, false);
}

TaskManagerPanelResource::~TaskManagerPanelResource() {
}

TaskManager::Resource::Type TaskManagerPanelResource::GetType() const {
  return EXTENSION;
}

string16 TaskManagerPanelResource::GetTitle() const {
  string16 title = panel_->GetWindowTitle();
  // Since the title will be concatenated with an IDS_TASK_MANAGER_* prefix
  // we need to explicitly set the title to be LTR format if there is no
  // strong RTL charater in it. Otherwise, if the task manager prefix is an
  // RTL word, the concatenated result might be wrong. For example,
  // a page whose title is "Yahoo! Mail: The best web-based Email!", without
  // setting it explicitly as LTR format, the concatenated result will be
  // "!Yahoo! Mail: The best web-based Email :PPA", in which the capital
  // letters "PPA" stands for the Hebrew word for "app".
  base::i18n::AdjustStringForLocaleDirection(&title);

  return l10n_util::GetStringFUTF16(message_prefix_id_, title);
}

string16 TaskManagerPanelResource::GetProfileName() const {
  return GetProfileNameFromInfoCache(panel_->profile());
}

gfx::ImageSkia TaskManagerPanelResource::GetIcon() const {
  gfx::Image icon = panel_->GetCurrentPageIcon();
  return icon.IsEmpty() ? gfx::ImageSkia() : *icon.ToImageSkia();
}

WebContents* TaskManagerPanelResource::GetWebContents() const {
  return panel_->GetWebContents();
}

const Extension* TaskManagerPanelResource::GetExtension() const {
  ExtensionService* extension_service =
      panel_->profile()->GetExtensionService();
  return extension_service->extensions()->GetByID(panel_->extension_id());
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerPanelResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerPanelResourceProvider::TaskManagerPanelResourceProvider(
    TaskManager* task_manager)
    : updating_(false),
      task_manager_(task_manager) {
}

TaskManagerPanelResourceProvider::~TaskManagerPanelResourceProvider() {
}

TaskManager::Resource* TaskManagerPanelResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  // If an origin PID was specified, the request is from a plugin, not the
  // render view host process
  if (origin_pid)
    return NULL;

  for (PanelResourceMap::iterator i = resources_.begin();
       i != resources_.end(); ++i) {
    WebContents* contents = i->first->GetWebContents();
    if (contents &&
        contents->GetRenderProcessHost()->GetID() == render_process_host_id &&
        contents->GetRenderViewHost()->GetRoutingID() == routing_id) {
      return i->second;
    }
  }

  // Can happen if the panel went away while a network request was being
  // performed.
  return NULL;
}

void TaskManagerPanelResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Add all the Panels.
  std::vector<Panel*> panels = PanelManager::GetInstance()->panels();
  for (size_t i = 0; i < panels.size(); ++i)
    Add(panels[i]);

  // Then we register for notifications to get new and remove closed panels.
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                 content::NotificationService::AllSources());
}

void TaskManagerPanelResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Unregister for notifications about new/removed panels.
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                    content::NotificationService::AllSources());
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                    content::NotificationService::AllSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());
  resources_.clear();
}

void TaskManagerPanelResourceProvider::Add(Panel* panel) {
  if (!updating_)
    return;

  PanelResourceMap::const_iterator iter = resources_.find(panel);
  if (iter != resources_.end())
    return;

  TaskManagerPanelResource* resource = new TaskManagerPanelResource(panel);
  resources_[panel] = resource;
  task_manager_->AddResource(resource);
}

void TaskManagerPanelResourceProvider::Remove(Panel* panel) {
  if (!updating_)
    return;

  PanelResourceMap::iterator iter = resources_.find(panel);
  if (iter == resources_.end())
    return;

  TaskManagerPanelResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  resources_.erase(iter);
  delete resource;
}

void TaskManagerPanelResourceProvider::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  WebContents* web_contents = content::Source<WebContents>(source).ptr();
  if (chrome::GetViewType(web_contents) != chrome::VIEW_TYPE_PANEL)
    return;

  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_CONNECTED:
    {
      std::vector<Panel*>panels = PanelManager::GetInstance()->panels();
      for (size_t i = 0; i < panels.size(); ++i) {
        if (panels[i]->GetWebContents() == web_contents) {
          Add(panels[i]);
          break;
        }
      }
      break;
    }
    case content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED:
    {
      for (PanelResourceMap::iterator iter = resources_.begin();
           iter != resources_.end(); ++iter) {
        Panel* panel = iter->first;
        WebContents* panel_contents = panel->GetWebContents();
        if (!panel_contents || panel_contents == web_contents) {
          Remove(panel);
          break;
        }
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected notificiation.";
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerBackgroundContentsResource class
////////////////////////////////////////////////////////////////////////////////

gfx::ImageSkia* TaskManagerBackgroundContentsResource::default_icon_ = NULL;

// TODO(atwilson): http://crbug.com/116893
// HACK: if the process handle is invalid, we use the current process's handle.
// This preserves old behavior but is incorrect, and should be fixed.
TaskManagerBackgroundContentsResource::TaskManagerBackgroundContentsResource(
    BackgroundContents* background_contents,
    const string16& application_name)
    : TaskManagerRendererResource(
          background_contents->web_contents()->GetRenderProcessHost()->
              GetHandle() ?
              background_contents->web_contents()->GetRenderProcessHost()->
                  GetHandle() :
              base::Process::Current().handle(),
          background_contents->web_contents()->GetRenderViewHost()),
      background_contents_(background_contents),
      application_name_(application_name) {
  // Just use the same icon that other extension resources do.
  // TODO(atwilson): Use the favicon when that's available.
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PLUGINS_FAVICON);
  }
  // Ensure that the string has the appropriate direction markers (see comment
  // in TaskManagerTabContentsResource::GetTitle()).
  base::i18n::AdjustStringForLocaleDirection(&application_name_);
}

TaskManagerBackgroundContentsResource::~TaskManagerBackgroundContentsResource(
    ) {
}

string16 TaskManagerBackgroundContentsResource::GetTitle() const {
  string16 title = application_name_;

  if (title.empty()) {
    // No title (can't locate the parent app for some reason) so just display
    // the URL (properly forced to be LTR).
    title = base::i18n::GetDisplayStringInLTRDirectionality(
        UTF8ToUTF16(background_contents_->GetURL().spec()));
  }
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_BACKGROUND_PREFIX, title);
}

string16 TaskManagerBackgroundContentsResource::GetProfileName() const {
  return string16();
}

gfx::ImageSkia TaskManagerBackgroundContentsResource::GetIcon() const {
  return *default_icon_;
}

bool TaskManagerBackgroundContentsResource::IsBackground() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerBackgroundContentsResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerBackgroundContentsResourceProvider::
    TaskManagerBackgroundContentsResourceProvider(TaskManager* task_manager)
    : updating_(false),
      task_manager_(task_manager) {
}

TaskManagerBackgroundContentsResourceProvider::
    ~TaskManagerBackgroundContentsResourceProvider() {
}

TaskManager::Resource*
TaskManagerBackgroundContentsResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  // If an origin PID was specified, the request is from a plugin, not the
  // render view host process
  if (origin_pid)
    return NULL;

  for (Resources::iterator i = resources_.begin(); i != resources_.end(); i++) {
    WebContents* tab = i->first->web_contents();
    if (tab->GetRenderProcessHost()->GetID() == render_process_host_id
        && tab->GetRenderViewHost()->GetRoutingID() == routing_id) {
      return i->second;
    }
  }

  // Can happen if the page went away while a network request was being
  // performed.
  return NULL;
}

void TaskManagerBackgroundContentsResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Add all the existing BackgroundContents from every profile, including
  // incognito profiles.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  size_t num_default_profiles = profiles.size();
  for (size_t i = 0; i < num_default_profiles; ++i) {
    if (profiles[i]->HasOffTheRecordProfile()) {
      profiles.push_back(profiles[i]->GetOffTheRecordProfile());
    }
  }
  for (size_t i = 0; i < profiles.size(); ++i) {
    BackgroundContentsService* background_contents_service =
        BackgroundContentsServiceFactory::GetForProfile(profiles[i]);
    std::vector<BackgroundContents*> contents =
        background_contents_service->GetBackgroundContents();
    ExtensionService* extension_service = profiles[i]->GetExtensionService();
    for (std::vector<BackgroundContents*>::iterator iterator = contents.begin();
         iterator != contents.end(); ++iterator) {
      string16 application_name;
      // Lookup the name from the parent extension.
      if (extension_service) {
        const string16& application_id =
            background_contents_service->GetParentApplicationId(*iterator);
        const Extension* extension = extension_service->GetExtensionById(
            UTF16ToUTF8(application_id), false);
        if (extension)
          application_name = UTF8ToUTF16(extension->name());
      }
      Add(*iterator, application_name);
    }
  }

  // Then we register for notifications to get new BackgroundContents.
  registrar_.Add(this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void TaskManagerBackgroundContentsResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Unregister for notifications
  registrar_.Remove(
      this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_OPENED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED,
      content::NotificationService::AllBrowserContextsAndSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
}

void TaskManagerBackgroundContentsResourceProvider::AddToTaskManager(
    BackgroundContents* background_contents,
    const string16& application_name) {
  TaskManagerBackgroundContentsResource* resource =
      new TaskManagerBackgroundContentsResource(background_contents,
                                                application_name);
  resources_[background_contents] = resource;
  task_manager_->AddResource(resource);
}

void TaskManagerBackgroundContentsResourceProvider::Add(
    BackgroundContents* contents, const string16& application_name) {
  if (!updating_)
    return;

  // TODO(atwilson): http://crbug.com/116893
  // We should check that the process handle is valid here, but it won't
  // be in the case of NOTIFICATION_BACKGROUND_CONTENTS_OPENED.

  // Should never add the same BackgroundContents twice.
  DCHECK(resources_.find(contents) == resources_.end());
  AddToTaskManager(contents, application_name);
}

void TaskManagerBackgroundContentsResourceProvider::Remove(
    BackgroundContents* contents) {
  if (!updating_)
    return;
  Resources::iterator iter = resources_.find(contents);
  DCHECK(iter != resources_.end());

  // Remove the resource from the Task Manager.
  TaskManagerBackgroundContentsResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  // And from the provider.
  resources_.erase(iter);
  // Finally, delete the resource.
  delete resource;
}

void TaskManagerBackgroundContentsResourceProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_OPENED: {
      // Get the name from the parent application. If no parent application is
      // found, just pass an empty string - BackgroundContentsResource::GetTitle
      // will display the URL instead in this case. This should never happen
      // except in rare cases when an extension is being unloaded or chrome is
      // exiting while the task manager is displayed.
      string16 application_name;
      ExtensionService* service =
          content::Source<Profile>(source)->GetExtensionService();
      if (service) {
        std::string application_id = UTF16ToUTF8(
            content::Details<BackgroundContentsOpenedDetails>(details)->
                application_id);
        const Extension* extension =
            service->GetExtensionById(application_id, false);
        // Extension can be NULL when running unit tests.
        if (extension)
          application_name = UTF8ToUTF16(extension->name());
      }
      Add(content::Details<BackgroundContentsOpenedDetails>(details)->contents,
          application_name);
      // Opening a new BackgroundContents needs to force the display to refresh
      // (applications may now be considered "background" that weren't before).
      task_manager_->ModelChanged();
      break;
    }
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED: {
      BackgroundContents* contents =
          content::Details<BackgroundContents>(details).ptr();
      // Should never get a NAVIGATED before OPENED.
      DCHECK(resources_.find(contents) != resources_.end());
      // Preserve the application name.
      string16 application_name(
          resources_.find(contents)->second->application_name());
      Remove(contents);
      Add(contents, application_name);
      break;
    }
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_DELETED:
      Remove(content::Details<BackgroundContents>(details).ptr());
      // Closing a BackgroundContents needs to force the display to refresh
      // (applications may now be considered "foreground" that weren't before).
      task_manager_->ModelChanged();
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerChildProcessResource class
////////////////////////////////////////////////////////////////////////////////
gfx::ImageSkia* TaskManagerChildProcessResource::default_icon_ = NULL;

TaskManagerChildProcessResource::TaskManagerChildProcessResource(
    content::ProcessType type,
    const string16& name,
    base::ProcessHandle handle,
    int unique_process_id)
    : type_(type),
      name_(name),
      handle_(handle),
      unique_process_id_(unique_process_id),
      network_usage_support_(false) {
  // We cache the process id because it's not cheap to calculate, and it won't
  // be available when we get the plugin disconnected notification.
  pid_ = base::GetProcId(handle);
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PLUGINS_FAVICON);
    // TODO(jabdelmalek): use different icon for web workers.
  }
}

TaskManagerChildProcessResource::~TaskManagerChildProcessResource() {
}

// TaskManagerResource methods:
string16 TaskManagerChildProcessResource::GetTitle() const {
  if (title_.empty())
    title_ = GetLocalizedTitle();

  return title_;
}

string16 TaskManagerChildProcessResource::GetProfileName() const {
  return string16();
}

gfx::ImageSkia TaskManagerChildProcessResource::GetIcon() const {
  return *default_icon_;
}

base::ProcessHandle TaskManagerChildProcessResource::GetProcess() const {
  return handle_;
}

int TaskManagerChildProcessResource::GetUniqueChildProcessId() const {
  return unique_process_id_;
}

TaskManager::Resource::Type TaskManagerChildProcessResource::GetType() const {
  // Translate types to TaskManager::ResourceType, since ChildProcessData's type
  // is not available for all TaskManager resources.
  switch (type_) {
    case content::PROCESS_TYPE_PLUGIN:
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
    case content::PROCESS_TYPE_PPAPI_BROKER:
      return TaskManager::Resource::PLUGIN;
    case content::PROCESS_TYPE_NACL_LOADER:
    case content::PROCESS_TYPE_NACL_BROKER:
      return TaskManager::Resource::NACL;
    case content::PROCESS_TYPE_UTILITY:
      return TaskManager::Resource::UTILITY;
    case content::PROCESS_TYPE_PROFILE_IMPORT:
      return TaskManager::Resource::PROFILE_IMPORT;
    case content::PROCESS_TYPE_ZYGOTE:
      return TaskManager::Resource::ZYGOTE;
    case content::PROCESS_TYPE_SANDBOX_HELPER:
      return TaskManager::Resource::SANDBOX_HELPER;
    case content::PROCESS_TYPE_GPU:
      return TaskManager::Resource::GPU;
    default:
      return TaskManager::Resource::UNKNOWN;
  }
}

bool TaskManagerChildProcessResource::SupportNetworkUsage() const {
  return network_usage_support_;
}

void TaskManagerChildProcessResource::SetSupportNetworkUsage() {
  network_usage_support_ = true;
}

string16 TaskManagerChildProcessResource::GetLocalizedTitle() const {
  string16 title = name_;
  if (title.empty()) {
    switch (type_) {
      case content::PROCESS_TYPE_PLUGIN:
      case content::PROCESS_TYPE_PPAPI_PLUGIN:
      case content::PROCESS_TYPE_PPAPI_BROKER:
        title = l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UNKNOWN_PLUGIN_NAME);
        break;
      default:
        // Nothing to do for non-plugin processes.
        break;
    }
  }

  // Explicitly mark name as LTR if there is no strong RTL character,
  // to avoid the wrong concatenation result similar to "!Yahoo Mail: the
  // best web-based Email: NIGULP", in which "NIGULP" stands for the Hebrew
  // or Arabic word for "plugin".
  base::i18n::AdjustStringForLocaleDirection(&title);

  switch (type_) {
    case content::PROCESS_TYPE_UTILITY:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX);

    case content::PROCESS_TYPE_PROFILE_IMPORT:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX);

    case content::PROCESS_TYPE_GPU:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_GPU_PREFIX);

    case content::PROCESS_TYPE_NACL_BROKER:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NACL_BROKER_PREFIX);

    case content::PROCESS_TYPE_PLUGIN:
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PLUGIN_PREFIX, title);

    case content::PROCESS_TYPE_PPAPI_BROKER:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PLUGIN_BROKER_PREFIX,
                                        title);

    case content::PROCESS_TYPE_NACL_LOADER:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_NACL_PREFIX, title);

    // These types don't need display names or get them from elsewhere.
    case content::PROCESS_TYPE_BROWSER:
    case content::PROCESS_TYPE_RENDERER:
    case content::PROCESS_TYPE_ZYGOTE:
    case content::PROCESS_TYPE_SANDBOX_HELPER:
    case content::PROCESS_TYPE_MAX:
      NOTREACHED();
      break;

    case content::PROCESS_TYPE_WORKER:
      NOTREACHED() << "Workers are not handled by this provider.";
      break;

    case content::PROCESS_TYPE_UNKNOWN:
      NOTREACHED() << "Need localized name for child process type.";
  }

  return title;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerChildProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerChildProcessResourceProvider::
    TaskManagerChildProcessResourceProvider(TaskManager* task_manager)
    : task_manager_(task_manager),
      updating_(false) {
}

TaskManagerChildProcessResourceProvider::
    ~TaskManagerChildProcessResourceProvider() {
}

TaskManager::Resource* TaskManagerChildProcessResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  PidResourceMap::iterator iter = pid_to_resources_.find(origin_pid);
  if (iter != pid_to_resources_.end())
    return iter->second;
  else
    return NULL;
}

void TaskManagerChildProcessResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Register for notifications to get new child processes.
  registrar_.Add(this, content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_CHILD_PROCESS_HOST_DISCONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());

  // Get the existing child processes.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &TaskManagerChildProcessResourceProvider::RetrieveChildProcessData,
          this));
}

void TaskManagerChildProcessResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Unregister for notifications to get new plugin processes.
  registrar_.Remove(
      this, content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this,
      content::NOTIFICATION_CHILD_PROCESS_HOST_DISCONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
  pid_to_resources_.clear();
}

void TaskManagerChildProcessResourceProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  content::ChildProcessData data =
      *content::Details<content::ChildProcessData>(details).ptr();
  switch (type) {
    case content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED:
      Add(data);
      break;
    case content::NOTIFICATION_CHILD_PROCESS_HOST_DISCONNECTED:
      Remove(data);
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

void TaskManagerChildProcessResourceProvider::Add(
    const content::ChildProcessData& child_process_data) {
  if (!updating_)
    return;
  // Workers are handled by TaskManagerWorkerResourceProvider.
  if (child_process_data.type == content::PROCESS_TYPE_WORKER)
    return;
  if (resources_.count(child_process_data.handle)) {
    // The case may happen that we have added a child_process_info as part of
    // the iteration performed during StartUpdating() call but the notification
    // that it has connected was not fired yet. So when the notification
    // happens, we already know about this plugin and just ignore it.
    return;
  }
  AddToTaskManager(child_process_data);
}

void TaskManagerChildProcessResourceProvider::Remove(
    const content::ChildProcessData& child_process_data) {
  if (!updating_)
    return;
  if (child_process_data.type == content::PROCESS_TYPE_WORKER)
    return;
  ChildProcessMap::iterator iter = resources_.find(child_process_data.handle);
  if (iter == resources_.end()) {
    // ChildProcessData disconnection notifications are asynchronous, so we
    // might be notified for a plugin we don't know anything about (if it was
    // closed before the task manager was shown and destroyed after that).
    return;
  }
  // Remove the resource from the Task Manager.
  TaskManagerChildProcessResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  // Remove it from the provider.
  resources_.erase(iter);
  // Remove it from our pid map.
  PidResourceMap::iterator pid_iter =
      pid_to_resources_.find(resource->process_id());
  DCHECK(pid_iter != pid_to_resources_.end());
  if (pid_iter != pid_to_resources_.end())
    pid_to_resources_.erase(pid_iter);

  // Finally, delete the resource.
  delete resource;
}

void TaskManagerChildProcessResourceProvider::AddToTaskManager(
    const content::ChildProcessData& child_process_data) {
  TaskManagerChildProcessResource* resource =
      new TaskManagerChildProcessResource(
          child_process_data.type,
          child_process_data.name,
          child_process_data.handle,
          child_process_data.id);
  resources_[child_process_data.handle] = resource;
  pid_to_resources_[resource->process_id()] = resource;
  task_manager_->AddResource(resource);
}

// The ChildProcessData::Iterator has to be used from the IO thread.
void TaskManagerChildProcessResourceProvider::RetrieveChildProcessData() {
  std::vector<content::ChildProcessData> child_processes;
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    // Only add processes which are already started, since we need their handle.
    if (iter.GetData().handle == base::kNullProcessHandle)
      continue;
    child_processes.push_back(iter.GetData());
  }
  // Now notify the UI thread that we have retrieved information about child
  // processes.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &TaskManagerChildProcessResourceProvider::ChildProcessDataRetreived,
          this, child_processes));
}

// This is called on the UI thread.
void TaskManagerChildProcessResourceProvider::ChildProcessDataRetreived(
    const std::vector<content::ChildProcessData>& child_processes) {
  for (size_t i = 0; i < child_processes.size(); ++i)
    Add(child_processes[i]);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TASK_MANAGER_CHILD_PROCESSES_DATA_READY,
      content::Source<TaskManagerChildProcessResourceProvider>(this),
      content::NotificationService::NoDetails());
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerExtensionProcessResource class
////////////////////////////////////////////////////////////////////////////////

gfx::ImageSkia* TaskManagerExtensionProcessResource::default_icon_ = NULL;

TaskManagerExtensionProcessResource::TaskManagerExtensionProcessResource(
    content::RenderViewHost* render_view_host)
    : render_view_host_(render_view_host) {
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PLUGINS_FAVICON);
  }
  process_handle_ = render_view_host_->GetProcess()->GetHandle();
  unique_process_id_ = render_view_host->GetProcess()->GetID();
  pid_ = base::GetProcId(process_handle_);
  string16 extension_name = UTF8ToUTF16(GetExtension()->name());
  DCHECK(!extension_name.empty());

  Profile* profile = Profile::FromBrowserContext(
      render_view_host->GetProcess()->GetBrowserContext());
  int message_id = GetMessagePrefixID(GetExtension()->is_app(), true,
      profile->IsOffTheRecord(), false, false, IsBackground());
  title_ = l10n_util::GetStringFUTF16(message_id, extension_name);
}

TaskManagerExtensionProcessResource::~TaskManagerExtensionProcessResource() {
}

string16 TaskManagerExtensionProcessResource::GetTitle() const {
  return title_;
}

string16 TaskManagerExtensionProcessResource::GetProfileName() const {
  return GetProfileNameFromInfoCache(Profile::FromBrowserContext(
      render_view_host_->GetProcess()->GetBrowserContext()));
}

gfx::ImageSkia TaskManagerExtensionProcessResource::GetIcon() const {
  return *default_icon_;
}

base::ProcessHandle TaskManagerExtensionProcessResource::GetProcess() const {
  return process_handle_;
}

int TaskManagerExtensionProcessResource::GetUniqueChildProcessId() const {
  return unique_process_id_;
}

TaskManager::Resource::Type
TaskManagerExtensionProcessResource::GetType() const {
  return EXTENSION;
}

bool TaskManagerExtensionProcessResource::CanInspect() const {
  return true;
}

void TaskManagerExtensionProcessResource::Inspect() const {
  DevToolsWindow::OpenDevToolsWindow(render_view_host_);
}

bool TaskManagerExtensionProcessResource::SupportNetworkUsage() const {
  return true;
}

void TaskManagerExtensionProcessResource::SetSupportNetworkUsage() {
  NOTREACHED();
}

const Extension* TaskManagerExtensionProcessResource::GetExtension() const {
  Profile* profile = Profile::FromBrowserContext(
      render_view_host_->GetProcess()->GetBrowserContext());
  ExtensionProcessManager* process_manager =
      extensions::ExtensionSystem::Get(profile)->process_manager();
  return process_manager->GetExtensionForRenderViewHost(render_view_host_);
}

bool TaskManagerExtensionProcessResource::IsBackground() const {
  WebContents* web_contents =
      WebContents::FromRenderViewHost(render_view_host_);
  chrome::ViewType view_type = chrome::GetViewType(web_contents);
  return view_type == chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerExtensionProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerExtensionProcessResourceProvider::
    TaskManagerExtensionProcessResourceProvider(TaskManager* task_manager)
    : task_manager_(task_manager),
      updating_(false) {
}

TaskManagerExtensionProcessResourceProvider::
    ~TaskManagerExtensionProcessResourceProvider() {
}

TaskManager::Resource* TaskManagerExtensionProcessResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  // If an origin PID was specified, the request is from a plugin, not the
  // render view host process
  if (origin_pid)
    return NULL;

  for (ExtensionRenderViewHostMap::iterator i = resources_.begin();
       i != resources_.end(); i++) {
    if (i->first->GetSiteInstance()->GetProcess()->GetID() ==
            render_process_host_id &&
        i->first->GetRoutingID() == routing_id)
      return i->second;
  }

  // Can happen if the page went away while a network request was being
  // performed.
  return NULL;
}

void TaskManagerExtensionProcessResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Add all the existing extension views from all Profiles, including those
  // from incognito split mode.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  size_t num_default_profiles = profiles.size();
  for (size_t i = 0; i < num_default_profiles; ++i) {
    if (profiles[i]->HasOffTheRecordProfile()) {
      profiles.push_back(profiles[i]->GetOffTheRecordProfile());
    }
  }

  for (size_t i = 0; i < profiles.size(); ++i) {
    ExtensionProcessManager* process_manager =
        extensions::ExtensionSystem::Get(profiles[i])->process_manager();
    if (process_manager) {
      const ExtensionProcessManager::ViewSet all_views =
          process_manager->GetAllViews();
      ExtensionProcessManager::ViewSet::const_iterator jt = all_views.begin();
      for (; jt != all_views.end(); ++jt) {
        content::RenderViewHost* rvh = *jt;
        // Don't add dead extension processes.
        if (!rvh->IsRenderViewLive())
          continue;

        AddToTaskManager(rvh);
      }
    }
  }

  // Register for notifications about extension process changes.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_VIEW_REGISTERED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void TaskManagerExtensionProcessResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Unregister for notifications about extension process changes.
  registrar_.Remove(
      this, chrome::NOTIFICATION_EXTENSION_VIEW_REGISTERED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, chrome::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED,
      content::NotificationService::AllBrowserContextsAndSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
}

void TaskManagerExtensionProcessResourceProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_VIEW_REGISTERED:
      AddToTaskManager(
          content::Details<content::RenderViewHost>(details).ptr());
      break;
    case chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED:
      RemoveFromTaskManager(
          content::Details<extensions::ExtensionHost>(details).ptr()->
          render_view_host());
      break;
    case chrome::NOTIFICATION_EXTENSION_VIEW_UNREGISTERED:
      RemoveFromTaskManager(
          content::Details<content::RenderViewHost>(details).ptr());
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

bool TaskManagerExtensionProcessResourceProvider::
    IsHandledByThisProvider(content::RenderViewHost* render_view_host) {
  WebContents* web_contents = WebContents::FromRenderViewHost(render_view_host);
  // Don't add WebContents that belong to a guest (those are handled by
  // TaskManagerGuestResourceProvider). Otherwise they will be added twice, and
  // in this case they will have the app's name as a title (due to the
  // TaskManagerExtensionProcessResource constructor).
  if (web_contents->GetRenderProcessHost()->IsGuest())
    return false;
  chrome::ViewType view_type = chrome::GetViewType(web_contents);
  // Don't add WebContents (those are handled by
  // TaskManagerTabContentsResourceProvider) or background contents (handled
  // by TaskManagerBackgroundResourceProvider).
#if defined(USE_ASH)
  return (view_type != chrome::VIEW_TYPE_TAB_CONTENTS &&
          view_type != chrome::VIEW_TYPE_BACKGROUND_CONTENTS);
#else
  return (view_type != chrome::VIEW_TYPE_TAB_CONTENTS &&
          view_type != chrome::VIEW_TYPE_BACKGROUND_CONTENTS &&
          view_type != chrome::VIEW_TYPE_PANEL);
#endif  // USE_ASH
}

void TaskManagerExtensionProcessResourceProvider::AddToTaskManager(
    content::RenderViewHost* render_view_host) {
  if (!IsHandledByThisProvider(render_view_host))
    return;

  TaskManagerExtensionProcessResource* resource =
      new TaskManagerExtensionProcessResource(render_view_host);
  DCHECK(resources_.find(render_view_host) == resources_.end());
  resources_[render_view_host] = resource;
  task_manager_->AddResource(resource);
}

void TaskManagerExtensionProcessResourceProvider::RemoveFromTaskManager(
    content::RenderViewHost* render_view_host) {
  if (!updating_)
    return;
  std::map<content::RenderViewHost*, TaskManagerExtensionProcessResource*>
      ::iterator iter = resources_.find(render_view_host);
  if (iter == resources_.end())
    return;

  // Remove the resource from the Task Manager.
  TaskManagerExtensionProcessResource* resource = iter->second;
  task_manager_->RemoveResource(resource);

  // Remove it from the provider.
  resources_.erase(iter);

  // Finally, delete the resource.
  delete resource;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerBrowserProcessResource class
////////////////////////////////////////////////////////////////////////////////

gfx::ImageSkia* TaskManagerBrowserProcessResource::default_icon_ = NULL;

TaskManagerBrowserProcessResource::TaskManagerBrowserProcessResource()
    : title_() {
  int pid = base::GetCurrentProcId();
  bool success = base::OpenPrivilegedProcessHandle(pid, &process_);
  DCHECK(success);
#if defined(OS_WIN)
  if (!default_icon_) {
    HICON icon = GetAppIcon();
    if (icon) {
      scoped_ptr<SkBitmap> bitmap(IconUtil::CreateSkBitmapFromHICON(icon));
      default_icon_ = new gfx::ImageSkia(
          gfx::ImageSkiaRep(*bitmap, ui::SCALE_FACTOR_100P));
    }
  }
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_16);
  }
#elif defined(OS_MACOSX)
  if (!default_icon_) {
    // IDR_PRODUCT_LOGO_16 doesn't quite look like chrome/mac's icns icon. Load
    // the real app icon (requires a nsimage->image_skia->nsimage
    // conversion :-().
    default_icon_ = new gfx::ImageSkia(gfx::ApplicationIconAtSize(16));
  }
#else
  // TODO(port): Port icon code.
  NOTIMPLEMENTED();
#endif  // defined(OS_WIN)
  default_icon_->MakeThreadSafe();
}

TaskManagerBrowserProcessResource::~TaskManagerBrowserProcessResource() {
  base::CloseProcessHandle(process_);
}

// TaskManagerResource methods:
string16 TaskManagerBrowserProcessResource::GetTitle() const {
  if (title_.empty()) {
    title_ = l10n_util::GetStringUTF16(IDS_TASK_MANAGER_WEB_BROWSER_CELL_TEXT);
  }
  return title_;
}

string16 TaskManagerBrowserProcessResource::GetProfileName() const {
  return string16();
}

gfx::ImageSkia TaskManagerBrowserProcessResource::GetIcon() const {
  return *default_icon_;
}

size_t TaskManagerBrowserProcessResource::SqliteMemoryUsedBytes() const {
  return static_cast<size_t>(sqlite3_memory_used());
}

base::ProcessHandle TaskManagerBrowserProcessResource::GetProcess() const {
  return base::GetCurrentProcessHandle();  // process_;
}

int TaskManagerBrowserProcessResource::GetUniqueChildProcessId() const {
  return 0;
}

TaskManager::Resource::Type TaskManagerBrowserProcessResource::GetType() const {
  return BROWSER;
}

bool TaskManagerBrowserProcessResource::SupportNetworkUsage() const {
  return true;
}

void TaskManagerBrowserProcessResource::SetSupportNetworkUsage() {
  NOTREACHED();
}

bool TaskManagerBrowserProcessResource::ReportsSqliteMemoryUsed() const {
  return true;
}

// BrowserProcess uses v8 for proxy resolver in certain cases.
bool TaskManagerBrowserProcessResource::ReportsV8MemoryStats() const {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  bool using_v8 = !command_line->HasSwitch(switches::kWinHttpProxyResolver);
  if (using_v8 && command_line->HasSwitch(switches::kSingleProcess)) {
    using_v8 = false;
  }
  return using_v8;
}

size_t TaskManagerBrowserProcessResource::GetV8MemoryAllocated() const {
  v8::HeapStatistics stats;
  v8::V8::GetHeapStatistics(&stats);
  return stats.total_heap_size();
}

size_t TaskManagerBrowserProcessResource::GetV8MemoryUsed() const {
  v8::HeapStatistics stats;
  v8::V8::GetHeapStatistics(&stats);
  return stats.used_heap_size();
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerBrowserProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerBrowserProcessResourceProvider::
    TaskManagerBrowserProcessResourceProvider(TaskManager* task_manager)
    : updating_(false),
      task_manager_(task_manager) {
}

TaskManagerBrowserProcessResourceProvider::
    ~TaskManagerBrowserProcessResourceProvider() {
}

TaskManager::Resource* TaskManagerBrowserProcessResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  if (origin_pid || render_process_host_id != -1) {
    return NULL;
  }

  return &resource_;
}

void TaskManagerBrowserProcessResourceProvider::StartUpdating() {
  task_manager_->AddResource(&resource_);
}

void TaskManagerBrowserProcessResourceProvider::StopUpdating() {
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGuestResource class
////////////////////////////////////////////////////////////////////////////////

TaskManagerGuestResource::TaskManagerGuestResource(
    RenderViewHost* render_view_host)
    : TaskManagerRendererResource(
          render_view_host->GetSiteInstance()->GetProcess()->GetHandle(),
          render_view_host) {
}

TaskManagerGuestResource::~TaskManagerGuestResource() {
}

TaskManager::Resource::Type TaskManagerGuestResource::GetType() const {
  return GUEST;
}

string16 TaskManagerGuestResource::GetTitle() const {
  WebContents* web_contents = GetWebContents();
  const int message_id = IDS_TASK_MANAGER_WEBVIEW_TAG_PREFIX;
  if (web_contents) {
    string16 title = GetTitleFromWebContents(web_contents);
    return l10n_util::GetStringFUTF16(message_id, title);
  }
  return l10n_util::GetStringFUTF16(message_id, string16());
}

string16 TaskManagerGuestResource::GetProfileName() const {
  WebContents* web_contents = GetWebContents();
  if (web_contents) {
    Profile* profile = Profile::FromBrowserContext(
        web_contents->GetBrowserContext());
    return GetProfileNameFromInfoCache(profile);
  }
  return string16();
}

gfx::ImageSkia TaskManagerGuestResource::GetIcon() const {
  WebContents* web_contents = GetWebContents();
  if (web_contents && FaviconTabHelper::FromWebContents(web_contents)) {
    return FaviconTabHelper::FromWebContents(web_contents)->
        GetFavicon().AsImageSkia();
  }
  return gfx::ImageSkia();
}

WebContents* TaskManagerGuestResource::GetWebContents() const {
  return WebContents::FromRenderViewHost(render_view_host());
}

const Extension* TaskManagerGuestResource::GetExtension() const {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGuestContentsResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerGuestResourceProvider::
    TaskManagerGuestResourceProvider(TaskManager* task_manager)
    :  updating_(false),
       task_manager_(task_manager) {
}

TaskManagerGuestResourceProvider::~TaskManagerGuestResourceProvider() {
}

TaskManager::Resource* TaskManagerGuestResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  // If an origin PID was specified then the request originated in a plugin
  // working on the WebContents's behalf, so ignore it.
  if (origin_pid)
    return NULL;

  for (GuestResourceMap::iterator i = resources_.begin();
       i != resources_.end(); ++i) {
    WebContents* contents = WebContents::FromRenderViewHost(i->first);
    if (contents &&
        contents->GetRenderProcessHost()->GetID() == render_process_host_id &&
        contents->GetRenderViewHost()->GetRoutingID() == routing_id) {
      return i->second;
    }
  }

  return NULL;
}

void TaskManagerGuestResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Add all the existing guest WebContents.
  for (RenderProcessHost::iterator i(
           RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost::RenderWidgetHostsIterator iter =
        i.GetCurrentValue()->GetRenderWidgetHostsIterator();
    for (; !iter.IsAtEnd(); iter.Advance()) {
      const RenderWidgetHost* widget = iter.GetCurrentValue();
      RenderViewHost* rvh =
          RenderViewHost::From(const_cast<RenderWidgetHost*>(widget));
      if (rvh->IsSubframe())
        Add(rvh);
    }
  }

  // Then we register for notifications to get new guests.
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
}

void TaskManagerGuestResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Unregister for notifications.
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
}

void TaskManagerGuestResourceProvider::Add(
    RenderViewHost* render_view_host) {
  TaskManagerGuestResource* resource =
      new TaskManagerGuestResource(render_view_host);
  resources_[render_view_host] = resource;
  task_manager_->AddResource(resource);
}

void TaskManagerGuestResourceProvider::Remove(
    RenderViewHost* render_view_host) {
  if (!updating_)
    return;

  GuestResourceMap::iterator iter = resources_.find(render_view_host);
  if (iter == resources_.end())
    return;

  TaskManagerGuestResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  resources_.erase(iter);
  delete resource;
}

void TaskManagerGuestResourceProvider::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  WebContents* web_contents = content::Source<WebContents>(source).ptr();
  if (!web_contents || !web_contents->GetRenderViewHost()->IsSubframe())
    return;

  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_CONNECTED:
      Add(web_contents->GetRenderViewHost());
      break;
    case content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED:
      Remove(web_contents->GetRenderViewHost());
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
  }
}
