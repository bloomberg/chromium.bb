// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/extension_process_resource_provider.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::WebContents;
using extensions::Extension;

namespace task_manager {

class ExtensionProcessResource : public Resource {
 public:
  explicit ExtensionProcessResource(
      content::RenderViewHost* render_view_host);
  virtual ~ExtensionProcessResource();

  // Resource methods:
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual base::ProcessHandle GetProcess() const OVERRIDE;
  virtual int GetUniqueChildProcessId() const OVERRIDE;
  virtual Type GetType() const OVERRIDE;
  virtual bool CanInspect() const OVERRIDE;
  virtual void Inspect() const OVERRIDE;
  virtual bool SupportNetworkUsage() const OVERRIDE;
  virtual void SetSupportNetworkUsage() OVERRIDE;
  virtual const extensions::Extension* GetExtension() const OVERRIDE;

  // Returns the pid of the extension process.
  int process_id() const { return pid_; }

  // Returns true if the associated extension has a background page.
  virtual bool IsBackground() const OVERRIDE;

 private:
  // The icon painted for the extension process.
  static gfx::ImageSkia* default_icon_;

  content::RenderViewHost* render_view_host_;

  // Cached data about the extension.
  base::ProcessHandle process_handle_;
  int pid_;
  int unique_process_id_;
  string16 title_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionProcessResource);
};

gfx::ImageSkia* ExtensionProcessResource::default_icon_ = NULL;

ExtensionProcessResource::ExtensionProcessResource(
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
  int message_id = util::GetMessagePrefixID(
      GetExtension()->is_app(),
      true,  // is_extension
      profile->IsOffTheRecord(),
      false,  // is_prerender
      false,  // is_instant_overlay
      IsBackground());
  title_ = l10n_util::GetStringFUTF16(message_id, extension_name);
}

ExtensionProcessResource::~ExtensionProcessResource() {
}

string16 ExtensionProcessResource::GetTitle() const {
  return title_;
}

string16 ExtensionProcessResource::GetProfileName() const {
  return util::GetProfileNameFromInfoCache(
      Profile::FromBrowserContext(
          render_view_host_->GetProcess()->GetBrowserContext()));
}

gfx::ImageSkia ExtensionProcessResource::GetIcon() const {
  return *default_icon_;
}

base::ProcessHandle ExtensionProcessResource::GetProcess() const {
  return process_handle_;
}

int ExtensionProcessResource::GetUniqueChildProcessId() const {
  return unique_process_id_;
}

Resource::Type ExtensionProcessResource::GetType() const {
  return EXTENSION;
}

bool ExtensionProcessResource::CanInspect() const {
  return true;
}

void ExtensionProcessResource::Inspect() const {
  DevToolsWindow::OpenDevToolsWindow(render_view_host_);
}

bool ExtensionProcessResource::SupportNetworkUsage() const {
  return true;
}

void ExtensionProcessResource::SetSupportNetworkUsage() {
  NOTREACHED();
}

const Extension* ExtensionProcessResource::GetExtension() const {
  Profile* profile = Profile::FromBrowserContext(
      render_view_host_->GetProcess()->GetBrowserContext());
  extensions::ProcessManager* process_manager =
      extensions::ExtensionSystem::Get(profile)->process_manager();
  return process_manager->GetExtensionForRenderViewHost(render_view_host_);
}

bool ExtensionProcessResource::IsBackground() const {
  WebContents* web_contents =
      WebContents::FromRenderViewHost(render_view_host_);
  extensions::ViewType view_type = extensions::GetViewType(web_contents);
  return view_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

ExtensionProcessResourceProvider::
    ExtensionProcessResourceProvider(TaskManager* task_manager)
    : task_manager_(task_manager),
      updating_(false) {
}

ExtensionProcessResourceProvider::~ExtensionProcessResourceProvider() {
}

Resource* ExtensionProcessResourceProvider::GetResource(
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

void ExtensionProcessResourceProvider::StartUpdating() {
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
    extensions::ProcessManager* process_manager =
        extensions::ExtensionSystem::Get(profiles[i])->process_manager();
    if (process_manager) {
      const extensions::ProcessManager::ViewSet all_views =
          process_manager->GetAllViews();
      extensions::ProcessManager::ViewSet::const_iterator jt =
          all_views.begin();
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

void ExtensionProcessResourceProvider::StopUpdating() {
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

void ExtensionProcessResourceProvider::Observe(
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

bool ExtensionProcessResourceProvider::
    IsHandledByThisProvider(content::RenderViewHost* render_view_host) {
  WebContents* web_contents = WebContents::FromRenderViewHost(render_view_host);
  // Don't add WebContents that belong to a guest (those are handled by
  // GuestResourceProvider). Otherwise they will be added twice, and
  // in this case they will have the app's name as a title (due to the
  // ExtensionProcessResource constructor).
  if (web_contents->GetRenderProcessHost()->IsGuest())
    return false;
  extensions::ViewType view_type = extensions::GetViewType(web_contents);
  // Don't add WebContents (those are handled by
  // TabContentsResourceProvider) or background contents (handled
  // by BackgroundResourceProvider).
#if defined(USE_ASH)
  return (view_type != extensions::VIEW_TYPE_TAB_CONTENTS &&
          view_type != extensions::VIEW_TYPE_BACKGROUND_CONTENTS);
#else
  return (view_type != extensions::VIEW_TYPE_TAB_CONTENTS &&
          view_type != extensions::VIEW_TYPE_BACKGROUND_CONTENTS &&
          view_type != extensions::VIEW_TYPE_PANEL);
#endif  // USE_ASH
}

void ExtensionProcessResourceProvider::AddToTaskManager(
    content::RenderViewHost* render_view_host) {
  if (!IsHandledByThisProvider(render_view_host))
    return;

  ExtensionProcessResource* resource =
      new ExtensionProcessResource(render_view_host);
  DCHECK(resources_.find(render_view_host) == resources_.end());
  resources_[render_view_host] = resource;
  task_manager_->AddResource(resource);
}

void ExtensionProcessResourceProvider::RemoveFromTaskManager(
    content::RenderViewHost* render_view_host) {
  if (!updating_)
    return;
  std::map<content::RenderViewHost*, ExtensionProcessResource*>
      ::iterator iter = resources_.find(render_view_host);
  if (iter == resources_.end())
    return;

  // Remove the resource from the Task Manager.
  ExtensionProcessResource* resource = iter->second;
  task_manager_->RemoveResource(resource);

  // Remove it from the provider.
  resources_.erase(iter);

  // Finally, delete the resource.
  delete resource;
}

}  // namespace task_manager
