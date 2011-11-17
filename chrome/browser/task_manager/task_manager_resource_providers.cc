// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_resource_providers.h"

#include <string>

#include "base/basictypes.h"
#include "base/file_version_info.h"
#include "base/i18n/rtl.h"
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
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_view_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "third_party/sqlite/sqlite3.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

#if defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif
#if defined(OS_WIN)
#include "chrome/browser/app_icon_win.h"
#include "ui/gfx/icon_util.h"
#endif  // defined(OS_WIN)

using content::BrowserThread;

namespace {

// Returns the appropriate message prefix ID for tabs and extensions,
// reflecting whether they are apps or in incognito mode.
int GetMessagePrefixID(bool is_app, bool is_extension,
                       bool is_incognito, bool is_prerender) {
  if (is_app) {
    if (is_incognito)
      return IDS_TASK_MANAGER_APP_INCOGNITO_PREFIX;
    else
      return IDS_TASK_MANAGER_APP_PREFIX;
  } else if (is_extension) {
    if (is_incognito)
      return IDS_TASK_MANAGER_EXTENSION_INCOGNITO_PREFIX;
    else
      return IDS_TASK_MANAGER_EXTENSION_PREFIX;
  } else if (is_prerender) {
    return IDS_TASK_MANAGER_PRERENDER_PREFIX;
  } else {
    return IDS_TASK_MANAGER_TAB_PREFIX;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TaskManagerRendererResource class
////////////////////////////////////////////////////////////////////////////////
TaskManagerRendererResource::TaskManagerRendererResource(
    base::ProcessHandle process, RenderViewHost* render_view_host)
    : process_(process),
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
  stats_.images.size = 0;
  stats_.cssStyleSheets.size = 0;
  stats_.scripts.size = 0;
  stats_.xslStyleSheets.size = 0;
  stats_.fonts.size = 0;
}

TaskManagerRendererResource::~TaskManagerRendererResource() {
}

void TaskManagerRendererResource::Refresh() {
  if (!pending_stats_update_) {
    render_view_host_->Send(new ChromeViewMsg_GetCacheResourceStats);
    pending_stats_update_ = true;
  }
  if (!pending_fps_update_) {
    render_view_host_->Send(
        new ChromeViewMsg_GetFPS(render_view_host_->routing_id()));
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

TaskManager::Resource::Type TaskManagerRendererResource::GetType() const {
  return RENDERER;
}

int TaskManagerRendererResource::GetRoutingId() const {
  return render_view_host_->routing_id();
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

////////////////////////////////////////////////////////////////////////////////
// TaskManagerTabContentsResource class
////////////////////////////////////////////////////////////////////////////////

// static
SkBitmap* TaskManagerTabContentsResource::prerender_icon_ = NULL;

TaskManagerTabContentsResource::TaskManagerTabContentsResource(
    TabContentsWrapper* tab_contents)
    : TaskManagerRendererResource(
          tab_contents->tab_contents()->GetRenderProcessHost()->GetHandle(),
          tab_contents->render_view_host()),
      tab_contents_(tab_contents) {
  if (!prerender_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    prerender_icon_ = rb.GetBitmapNamed(IDR_PRERENDER);
  }
}

TaskManagerTabContentsResource::~TaskManagerTabContentsResource() {
}

bool TaskManagerTabContentsResource::IsPrerendering() const {
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(
          tab_contents_->profile());
  return prerender_manager &&
         prerender_manager->IsTabContentsPrerendering(
             tab_contents_->tab_contents());
}

bool TaskManagerTabContentsResource::HostsExtension() const {
  return tab_contents_->tab_contents()->GetURL().SchemeIs(
      chrome::kExtensionScheme);
}

TaskManager::Resource::Type TaskManagerTabContentsResource::GetType() const {
  return HostsExtension() ? EXTENSION : RENDERER;
}

string16 TaskManagerTabContentsResource::GetTitle() const {
  // Fall back on the URL if there's no title.
  TabContents* contents = tab_contents_->tab_contents();
  string16 tab_title = contents->GetTitle();
  GURL url = contents->GetURL();
  if (tab_title.empty()) {
    tab_title = UTF8ToUTF16(url.spec());
    // Force URL to be LTR.
    tab_title = base::i18n::GetDisplayStringInLTRDirectionality(tab_title);
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
    base::i18n::AdjustStringForLocaleDirection(&tab_title);
  }

  // Only classify as an app if the URL is an app and the tab is hosting an
  // extension process.  (It's possible to be showing the URL from before it
  // was installed as an app.)
  ExtensionService* extension_service =
      tab_contents_->profile()->GetExtensionService();
  extensions::ProcessMap* process_map = extension_service->process_map();
  bool is_app = extension_service->IsInstalledApp(url) &&
      process_map->Contains(contents->GetRenderProcessHost()->id());

  int message_id = GetMessagePrefixID(
      is_app,
      HostsExtension(),
      tab_contents_->profile()->IsOffTheRecord(),
      IsPrerendering());
  return l10n_util::GetStringFUTF16(message_id, tab_title);
}

string16 TaskManagerTabContentsResource::GetProfileName() const {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  Profile* profile = tab_contents_->profile()->GetOriginalProfile();
  size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  if (index == std::string::npos)
    return string16();
  else
    return cache.GetNameOfProfileAtIndex(index);
}

SkBitmap TaskManagerTabContentsResource::GetIcon() const {
  if (IsPrerendering())
    return *prerender_icon_;
  return tab_contents_->favicon_tab_helper()->GetFavicon();
}

TabContentsWrapper* TaskManagerTabContentsResource::GetTabContents() const {
  return tab_contents_;
}

const Extension* TaskManagerTabContentsResource::GetExtension() const {
  if (HostsExtension()) {
    ExtensionService* extension_service =
        tab_contents_->profile()->GetExtensionService();
    return extension_service->GetExtensionByURL(
        tab_contents_->tab_contents()->GetURL());
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
  TabContents* tab_contents =
      tab_util::GetTabContentsByID(render_process_host_id, routing_id);
  if (!tab_contents)  // Not one of our resource.
    return NULL;

  // If an origin PID was specified then the request originated in a plugin
  // working on the TabContent's behalf, so ignore it.
  if (origin_pid)
    return NULL;

  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  std::map<TabContentsWrapper*, TaskManagerTabContentsResource*>::iterator
      res_iter = resources_.find(wrapper);
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

  // Add all the existing TabContents.
  for (TabContentsIterator iterator; !iterator.done(); ++iterator)
    Add(*iterator);

  // Then we register for notifications to get new tabs.
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_CONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_SWAPPED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DISCONNECTED,
                 content::NotificationService::AllBrowserContextsAndSources());
  // TAB_CONTENTS_DISCONNECTED should be enough to know when to remove a
  // resource.  This is an attempt at mitigating a crasher that seem to
  // indicate a resource is still referencing a deleted TabContents
  // (http://crbug.com/7321).
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void TaskManagerTabContentsResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Then we unregister for notifications to get new tabs.
  registrar_.Remove(
      this, content::NOTIFICATION_TAB_CONTENTS_CONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, content::NOTIFICATION_TAB_CONTENTS_SWAPPED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, content::NOTIFICATION_TAB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
      content::NotificationService::AllBrowserContextsAndSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
}

void TaskManagerTabContentsResourceProvider::AddToTaskManager(
    TabContentsWrapper* tab_contents) {
  TaskManagerTabContentsResource* resource =
      new TaskManagerTabContentsResource(tab_contents);
  resources_[tab_contents] = resource;
  task_manager_->AddResource(resource);
}

void TaskManagerTabContentsResourceProvider::Add(
    TabContentsWrapper* tab_contents) {
  if (!updating_)
    return;

  // Don't add dead tabs or tabs that haven't yet connected.
  if (!tab_contents->tab_contents()->GetRenderProcessHost()->GetHandle() ||
      !tab_contents->tab_contents()->notify_disconnection()) {
    return;
  }

  std::map<TabContentsWrapper*, TaskManagerTabContentsResource*>::const_iterator
      iter = resources_.find(tab_contents);
  if (iter != resources_.end()) {
    // The case may happen that we have added a TabContents as part of the
    // iteration performed during StartUpdating() call but the notification that
    // it has connected was not fired yet. So when the notification happens, we
    // already know about this tab and just ignore it.
    return;
  }
  AddToTaskManager(tab_contents);
}

void TaskManagerTabContentsResourceProvider::Remove(
    TabContentsWrapper* tab_contents) {
  if (!updating_)
    return;
  std::map<TabContentsWrapper*, TaskManagerTabContentsResource*>::iterator
      iter = resources_.find(tab_contents);
  if (iter == resources_.end()) {
    // Since TabContents are destroyed asynchronously (see TabContentsCollector
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

void TaskManagerTabContentsResourceProvider::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  TabContentsWrapper* tab_contents =
      TabContentsWrapper::GetCurrentWrapperForContents(
          content::Source<TabContents>(source).ptr());
  // A background page does not have a TabContentsWrapper.
  if (!tab_contents)
    return;
  switch (type) {
    case content::NOTIFICATION_TAB_CONTENTS_CONNECTED:
      Add(tab_contents);
      break;
    case content::NOTIFICATION_TAB_CONTENTS_SWAPPED:
      Remove(tab_contents);
      Add(tab_contents);
      break;
    case content::NOTIFICATION_TAB_CONTENTS_DESTROYED:
      // If this DCHECK is triggered, it could explain http://crbug.com/7321 .
      DCHECK(resources_.find(tab_contents) ==
             resources_.end()) << "TAB_CONTENTS_DESTROYED with no associated "
                                  "TAB_CONTENTS_DISCONNECTED";
      // Fall through.
    case content::NOTIFICATION_TAB_CONTENTS_DISCONNECTED:
      Remove(tab_contents);
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerBackgroundContentsResource class
////////////////////////////////////////////////////////////////////////////////

SkBitmap* TaskManagerBackgroundContentsResource::default_icon_ = NULL;

TaskManagerBackgroundContentsResource::TaskManagerBackgroundContentsResource(
    BackgroundContents* background_contents,
    const string16& application_name)
    : TaskManagerRendererResource(
          background_contents->tab_contents()->GetRenderProcessHost()->
              GetHandle(),
          background_contents->tab_contents()->render_view_host()),
      background_contents_(background_contents),
      application_name_(application_name) {
  // Just use the same icon that other extension resources do.
  // TODO(atwilson): Use the favicon when that's available.
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetBitmapNamed(IDR_PLUGIN);
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

SkBitmap TaskManagerBackgroundContentsResource::GetIcon() const {
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
    TabContents* tab = i->first->tab_contents();
    if (tab->render_view_host()->process()->id() == render_process_host_id &&
        tab->render_view_host()->routing_id() == routing_id) {
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

  // Don't add contents whose process is dead.
  if (!contents->tab_contents()->GetRenderProcessHost()->GetHandle())
    return;

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
SkBitmap* TaskManagerChildProcessResource::default_icon_ = NULL;

TaskManagerChildProcessResource::TaskManagerChildProcessResource(
    const ChildProcessInfo& child_proc)
    : child_process_(child_proc),
      title_(),
      network_usage_support_(false) {
  // We cache the process id because it's not cheap to calculate, and it won't
  // be available when we get the plugin disconnected notification.
  pid_ = child_proc.pid();
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetBitmapNamed(IDR_PLUGIN);
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

SkBitmap TaskManagerChildProcessResource::GetIcon() const {
  return *default_icon_;
}

base::ProcessHandle TaskManagerChildProcessResource::GetProcess() const {
  return child_process_.handle();
}

TaskManager::Resource::Type TaskManagerChildProcessResource::GetType() const {
  // Translate types to TaskManager::ResourceType, since ChildProcessInfo's type
  // is not available for all TaskManager resources.
  switch (child_process_.type()) {
    case ChildProcessInfo::PLUGIN_PROCESS:
    case ChildProcessInfo::PPAPI_PLUGIN_PROCESS:
    case ChildProcessInfo::PPAPI_BROKER_PROCESS:
      return TaskManager::Resource::PLUGIN;
    case ChildProcessInfo::NACL_LOADER_PROCESS:
    case ChildProcessInfo::NACL_BROKER_PROCESS:
      return TaskManager::Resource::NACL;
    case ChildProcessInfo::UTILITY_PROCESS:
      return TaskManager::Resource::UTILITY;
    case ChildProcessInfo::PROFILE_IMPORT_PROCESS:
      return TaskManager::Resource::PROFILE_IMPORT;
    case ChildProcessInfo::ZYGOTE_PROCESS:
      return TaskManager::Resource::ZYGOTE;
    case ChildProcessInfo::SANDBOX_HELPER_PROCESS:
      return TaskManager::Resource::SANDBOX_HELPER;
    case ChildProcessInfo::GPU_PROCESS:
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
  string16 title = child_process_.name();
  if (title.empty()) {
    switch (child_process_.type()) {
      case ChildProcessInfo::PLUGIN_PROCESS:
      case ChildProcessInfo::PPAPI_PLUGIN_PROCESS:
      case ChildProcessInfo::PPAPI_BROKER_PROCESS:
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

  switch (child_process_.type()) {
    case ChildProcessInfo::UTILITY_PROCESS:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX);

    case ChildProcessInfo::PROFILE_IMPORT_PROCESS:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX);

    case ChildProcessInfo::GPU_PROCESS:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_GPU_PREFIX);

    case ChildProcessInfo::NACL_BROKER_PROCESS:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NACL_BROKER_PREFIX);

    case ChildProcessInfo::PLUGIN_PROCESS:
    case ChildProcessInfo::PPAPI_PLUGIN_PROCESS:
      return l10n_util::GetStringFUTF16(
          IDS_TASK_MANAGER_PLUGIN_PREFIX, title, child_process_.version());

    case ChildProcessInfo::PPAPI_BROKER_PROCESS:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PLUGIN_BROKER_PREFIX,
                                        title, child_process_.version());

    case ChildProcessInfo::NACL_LOADER_PROCESS:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_NACL_PREFIX, title);

    // These types don't need display names or get them from elsewhere.
    case ChildProcessInfo::BROWSER_PROCESS:
    case ChildProcessInfo::RENDER_PROCESS:
    case ChildProcessInfo::ZYGOTE_PROCESS:
    case ChildProcessInfo::SANDBOX_HELPER_PROCESS:
    case ChildProcessInfo::MAX_PROCESS:
      NOTREACHED();
      break;

    case ChildProcessInfo::WORKER_PROCESS:
      NOTREACHED() << "Workers are not handled by this provider.";
      break;

    case ChildProcessInfo::UNKNOWN_PROCESS:
      NOTREACHED() << "Need localized name for child process type.";
  }

  return title;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerChildProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerChildProcessResourceProvider::
    TaskManagerChildProcessResourceProvider(TaskManager* task_manager)
    : updating_(false),
      task_manager_(task_manager) {
}

TaskManagerChildProcessResourceProvider::
    ~TaskManagerChildProcessResourceProvider() {
}

TaskManager::Resource* TaskManagerChildProcessResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  std::map<int, TaskManagerChildProcessResource*>::iterator iter =
      pid_to_resources_.find(origin_pid);
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
      NewRunnableMethod(
          this,
          &TaskManagerChildProcessResourceProvider::RetrieveChildProcessInfo));
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
  existing_child_process_info_.clear();
}

void TaskManagerChildProcessResourceProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED:
      Add(*content::Details<ChildProcessInfo>(details).ptr());
      break;
    case content::NOTIFICATION_CHILD_PROCESS_HOST_DISCONNECTED:
      Remove(*content::Details<ChildProcessInfo>(details).ptr());
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

void TaskManagerChildProcessResourceProvider::Add(
    const ChildProcessInfo& child_process_info) {
  if (!updating_)
    return;
  // Workers are handled by TaskManagerWorkerResourceProvider.
  if (child_process_info.type() == ChildProcessInfo::WORKER_PROCESS)
    return;
  std::map<ChildProcessInfo, TaskManagerChildProcessResource*>::
      const_iterator iter = resources_.find(child_process_info);
  if (iter != resources_.end()) {
    // The case may happen that we have added a child_process_info as part of
    // the iteration performed during StartUpdating() call but the notification
    // that it has connected was not fired yet. So when the notification
    // happens, we already know about this plugin and just ignore it.
    return;
  }
  AddToTaskManager(child_process_info);
}

void TaskManagerChildProcessResourceProvider::Remove(
    const ChildProcessInfo& child_process_info) {
  if (!updating_)
    return;
  if (child_process_info.type() == ChildProcessInfo::WORKER_PROCESS)
    return;
  std::map<ChildProcessInfo, TaskManagerChildProcessResource*>
      ::iterator iter = resources_.find(child_process_info);
  if (iter == resources_.end()) {
    // ChildProcessInfo disconnection notifications are asynchronous, so we
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
  std::map<int, TaskManagerChildProcessResource*>::iterator pid_iter =
      pid_to_resources_.find(resource->process_id());
  DCHECK(pid_iter != pid_to_resources_.end());
  if (pid_iter != pid_to_resources_.end())
    pid_to_resources_.erase(pid_iter);

  // Finally, delete the resource.
  delete resource;
}

void TaskManagerChildProcessResourceProvider::AddToTaskManager(
    const ChildProcessInfo& child_process_info) {
  TaskManagerChildProcessResource* resource =
      new TaskManagerChildProcessResource(child_process_info);
  resources_[child_process_info] = resource;
  pid_to_resources_[resource->process_id()] = resource;
  task_manager_->AddResource(resource);
}

// The ChildProcessInfo::Iterator has to be used from the IO thread.
void TaskManagerChildProcessResourceProvider::RetrieveChildProcessInfo() {
  for (BrowserChildProcessHost::Iterator iter; !iter.Done(); ++iter) {
    // Only add processes which are already started, since we need their handle.
    if ((*iter)->handle() != base::kNullProcessHandle)
      existing_child_process_info_.push_back(**iter);
  }
  // Now notify the UI thread that we have retrieved information about child
  // processes.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &TaskManagerChildProcessResourceProvider::ChildProcessInfoRetreived));
}

// This is called on the UI thread.
void TaskManagerChildProcessResourceProvider::ChildProcessInfoRetreived() {
  std::vector<ChildProcessInfo>::const_iterator iter;
  for (iter = existing_child_process_info_.begin();
       iter != existing_child_process_info_.end(); ++iter) {
    Add(*iter);
  }
  existing_child_process_info_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerExtensionProcessResource class
////////////////////////////////////////////////////////////////////////////////

SkBitmap* TaskManagerExtensionProcessResource::default_icon_ = NULL;

TaskManagerExtensionProcessResource::TaskManagerExtensionProcessResource(
    ExtensionHost* extension_host)
    : extension_host_(extension_host) {
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetBitmapNamed(IDR_PLUGIN);
  }
  process_handle_ = extension_host_->render_process_host()->GetHandle();
  pid_ = base::GetProcId(process_handle_);
  string16 extension_name = UTF8ToUTF16(GetExtension()->name());
  DCHECK(!extension_name.empty());

  int message_id = GetMessagePrefixID(GetExtension()->is_app(), true,
      extension_host_->profile()->IsOffTheRecord(), false);
  title_ = l10n_util::GetStringFUTF16(message_id, extension_name);
}

TaskManagerExtensionProcessResource::~TaskManagerExtensionProcessResource() {
}

string16 TaskManagerExtensionProcessResource::GetTitle() const {
  return title_;
}

string16 TaskManagerExtensionProcessResource::GetProfileName() const {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  Profile* profile = extension_host_->profile()->GetOriginalProfile();
  size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  if (index == std::string::npos)
    return string16();
  else
    return cache.GetNameOfProfileAtIndex(index);
}

SkBitmap TaskManagerExtensionProcessResource::GetIcon() const {
  return *default_icon_;
}

base::ProcessHandle TaskManagerExtensionProcessResource::GetProcess() const {
  return process_handle_;
}

TaskManager::Resource::Type
TaskManagerExtensionProcessResource::GetType() const {
  return EXTENSION;
}

bool TaskManagerExtensionProcessResource::CanInspect() const {
  return true;
}

void TaskManagerExtensionProcessResource::Inspect() const {
  DevToolsWindow::OpenDevToolsWindow(extension_host_->render_view_host());
}

bool TaskManagerExtensionProcessResource::SupportNetworkUsage() const {
  return true;
}

void TaskManagerExtensionProcessResource::SetSupportNetworkUsage() {
  NOTREACHED();
}

const Extension* TaskManagerExtensionProcessResource::GetExtension() const {
  return extension_host_->extension();
}

bool TaskManagerExtensionProcessResource::IsBackground() const {
  return extension_host_->extension_host_type() ==
      chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
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
  std::map<int, TaskManagerExtensionProcessResource*>::iterator iter =
      pid_to_resources_.find(origin_pid);
  if (iter != pid_to_resources_.end())
    return iter->second;
  else
    return NULL;
}

void TaskManagerExtensionProcessResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Add all the existing ExtensionHosts from all Profiles, including those from
  // incognito split mode.
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
        profiles[i]->GetExtensionProcessManager();
    if (process_manager) {
      ExtensionProcessManager::const_iterator jt;
      for (jt = process_manager->begin(); jt != process_manager->end(); ++jt) {
        // Don't add dead extension processes.
        if ((*jt)->IsRenderViewLive())
          AddToTaskManager(*jt);
      }
    }
  }

  // Register for notifications about extension process changes.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void TaskManagerExtensionProcessResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Unregister for notifications about extension process changes.
  registrar_.Remove(
      this, chrome::NOTIFICATION_EXTENSION_HOST_CREATED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Remove(
      this, chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllBrowserContextsAndSources());

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
  pid_to_resources_.clear();
}

void TaskManagerExtensionProcessResourceProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_CREATED:
      AddToTaskManager(content::Details<ExtensionHost>(details).ptr());
      break;
    case chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED:
    case chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED:
      RemoveFromTaskManager(content::Details<ExtensionHost>(details).ptr());
      break;
    default:
      NOTREACHED() << "Unexpected notification.";
      return;
  }
}

void TaskManagerExtensionProcessResourceProvider::AddToTaskManager(
    ExtensionHost* extension_host) {
  TaskManagerExtensionProcessResource* resource =
      new TaskManagerExtensionProcessResource(extension_host);
  DCHECK(resources_.find(extension_host) == resources_.end());
  resources_[extension_host] = resource;
  pid_to_resources_[resource->process_id()] = resource;
  task_manager_->AddResource(resource);
}

void TaskManagerExtensionProcessResourceProvider::RemoveFromTaskManager(
    ExtensionHost* extension_host) {
  if (!updating_)
    return;
  std::map<ExtensionHost*, TaskManagerExtensionProcessResource*>
      ::iterator iter = resources_.find(extension_host);
  if (iter == resources_.end())
    return;

  // Remove the resource from the Task Manager.
  TaskManagerExtensionProcessResource* resource = iter->second;
  task_manager_->RemoveResource(resource);

  // Remove it from the provider.
  resources_.erase(iter);

  // Remove it from our pid map.
  std::map<int, TaskManagerExtensionProcessResource*>::iterator pid_iter =
      pid_to_resources_.find(resource->process_id());
  DCHECK(pid_iter != pid_to_resources_.end());
  if (pid_iter != pid_to_resources_.end())
    pid_to_resources_.erase(pid_iter);

  // Finally, delete the resource.
  delete resource;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerBrowserProcessResource class
////////////////////////////////////////////////////////////////////////////////

SkBitmap* TaskManagerBrowserProcessResource::default_icon_ = NULL;

TaskManagerBrowserProcessResource::TaskManagerBrowserProcessResource()
    : title_() {
  int pid = base::GetCurrentProcId();
  bool success = base::OpenPrivilegedProcessHandle(pid, &process_);
  DCHECK(success);
#if defined(OS_WIN)
  if (!default_icon_) {
    HICON icon = GetAppIcon();
    if (icon) {
      default_icon_ = IconUtil::CreateSkBitmapFromHICON(icon);
    }
  }
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetBitmapNamed(IDR_PRODUCT_LOGO_16);
  }
#elif defined(OS_MACOSX)
  if (!default_icon_) {
    // IDR_PRODUCT_LOGO_16 doesn't quite look like chrome/mac's icns icon. Load
    // the real app icon (requires a nsimage->skbitmap->nsimage conversion :-().
    default_icon_ = new SkBitmap(gfx::AppplicationIconAtSize(16));
  }
#else
  // TODO(port): Port icon code.
  NOTIMPLEMENTED();
#endif  // defined(OS_WIN)
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

SkBitmap TaskManagerBrowserProcessResource::GetIcon() const {
  return *default_icon_;
}

size_t TaskManagerBrowserProcessResource::SqliteMemoryUsedBytes() const {
  return static_cast<size_t>(sqlite3_memory_used());
}

base::ProcessHandle TaskManagerBrowserProcessResource::GetProcess() const {
  return base::GetCurrentProcessHandle();  // process_;
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
