// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_extension_process_resource_provider.h"

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::WebContents;
using extensions::Extension;

namespace {

// Returns the appropriate message prefix ID for tabs and extensions,
// reflecting whether they are apps or in incognito mode.
int GetMessagePrefixID(bool is_app,
                       bool is_extension,
                       bool is_incognito,
                       bool is_prerender,
                       bool is_instant_overlay,
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
  } else if (is_instant_overlay) {
    return IDS_TASK_MANAGER_INSTANT_OVERLAY_PREFIX;
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

}  // namespace

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
  extensions::ViewType view_type = extensions::GetViewType(web_contents);
  return view_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
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
  extensions::ViewType view_type = extensions::GetViewType(web_contents);
  // Don't add WebContents (those are handled by
  // TaskManagerTabContentsResourceProvider) or background contents (handled
  // by TaskManagerBackgroundResourceProvider).
#if defined(USE_ASH)
  return (view_type != extensions::VIEW_TYPE_TAB_CONTENTS &&
          view_type != extensions::VIEW_TYPE_BACKGROUND_CONTENTS);
#else
  return (view_type != extensions::VIEW_TYPE_TAB_CONTENTS &&
          view_type != extensions::VIEW_TYPE_BACKGROUND_CONTENTS &&
          view_type != extensions::VIEW_TYPE_PANEL);
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
