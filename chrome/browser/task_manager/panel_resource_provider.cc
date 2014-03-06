// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/panel_resource_provider.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

using content::RenderProcessHost;
using content::RenderViewHost;
using content::WebContents;
using extensions::Extension;

namespace task_manager {

class PanelResource : public RendererResource {
 public:
  explicit PanelResource(Panel* panel);
  virtual ~PanelResource();

  // Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual base::string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;

 private:
  Panel* panel_;
  // Determines prefix for title reflecting whether extensions are apps
  // or in incognito mode.
  int message_prefix_id_;

  DISALLOW_COPY_AND_ASSIGN(PanelResource);
};

PanelResource::PanelResource(Panel* panel)
    : RendererResource(
        panel->GetWebContents()->GetRenderProcessHost()->GetHandle(),
        panel->GetWebContents()->GetRenderViewHost()),
      panel_(panel) {
  ExtensionService* service = panel_->profile()->GetExtensionService();
  message_prefix_id_ = util::GetMessagePrefixID(
      service->extensions()->GetByID(panel_->extension_id())->is_app(),
      true,  // is_extension
      panel_->profile()->IsOffTheRecord(),
      false,   // is_prerender
      false);  // is_background
}

PanelResource::~PanelResource() {
}

Resource::Type PanelResource::GetType() const {
  return EXTENSION;
}

base::string16 PanelResource::GetTitle() const {
  base::string16 title = panel_->GetWindowTitle();
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

base::string16 PanelResource::GetProfileName() const {
  return util::GetProfileNameFromInfoCache(panel_->profile());
}

gfx::ImageSkia PanelResource::GetIcon() const {
  gfx::Image icon = panel_->GetCurrentPageIcon();
  return icon.IsEmpty() ? gfx::ImageSkia() : *icon.ToImageSkia();
}

WebContents* PanelResource::GetWebContents() const {
  return panel_->GetWebContents();
}

////////////////////////////////////////////////////////////////////////////////
// PanelResourceProvider class
////////////////////////////////////////////////////////////////////////////////

PanelResourceProvider::PanelResourceProvider(TaskManager* task_manager)
    : updating_(false),
      task_manager_(task_manager) {
}

PanelResourceProvider::~PanelResourceProvider() {
}

Resource* PanelResourceProvider::GetResource(
    int origin_pid,
    int child_id,
    int route_id) {
  // If an origin PID was specified, the request is from a plugin, not the
  // render view host process
  if (origin_pid)
    return NULL;

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(child_id, route_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  for (PanelResourceMap::iterator i = resources_.begin();
       i != resources_.end(); ++i) {
    if (web_contents == i->first->GetWebContents())
      return i->second;
  }

  // Can happen if the panel went away while a network request was being
  // performed.
  return NULL;
}

void PanelResourceProvider::StartUpdating() {
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

void PanelResourceProvider::StopUpdating() {
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

void PanelResourceProvider::Add(Panel* panel) {
  if (!updating_)
    return;

  PanelResourceMap::const_iterator iter = resources_.find(panel);
  if (iter != resources_.end())
    return;

  PanelResource* resource = new PanelResource(panel);
  resources_[panel] = resource;
  task_manager_->AddResource(resource);
}

void PanelResourceProvider::Remove(Panel* panel) {
  if (!updating_)
    return;

  PanelResourceMap::iterator iter = resources_.find(panel);
  if (iter == resources_.end())
    return;

  PanelResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  resources_.erase(iter);
  delete resource;
}

void PanelResourceProvider::Observe(int type,
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

}  // namespace task_manager
