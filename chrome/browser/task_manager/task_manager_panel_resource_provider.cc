// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_panel_resource_provider.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_render_resource.h"
#include "chrome/browser/task_manager/task_manager_resource_util.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/base/l10n/l10n_util.h"

using content::RenderProcessHost;
using content::RenderViewHost;
using content::WebContents;
using extensions::Extension;

class TaskManagerPanelResource : public TaskManagerRendererResource {
 public:
  explicit TaskManagerPanelResource(Panel* panel);
  virtual ~TaskManagerPanelResource();

  // TaskManager::Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual const extensions::Extension* GetExtension() const OVERRIDE;

 private:
  Panel* panel_;
  // Determines prefix for title reflecting whether extensions are apps
  // or in incognito mode.
  int message_prefix_id_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerPanelResource);
};

TaskManagerPanelResource::TaskManagerPanelResource(Panel* panel)
    : TaskManagerRendererResource(
        panel->GetWebContents()->GetRenderProcessHost()->GetHandle(),
        panel->GetWebContents()->GetRenderViewHost()),
      panel_(panel) {
  message_prefix_id_ = TaskManagerResourceUtil::GetMessagePrefixID(
      GetExtension()->is_app(),
      true,  // is_extension
      panel->profile()->IsOffTheRecord(),
      false,  // is_prerender
      false,  // is_instant_overlay
      false);  // is_background
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
  return TaskManagerResourceUtil::GetProfileNameFromInfoCache(
      panel_->profile());
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
  if (extensions::GetViewType(web_contents) != extensions::VIEW_TYPE_PANEL)
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
