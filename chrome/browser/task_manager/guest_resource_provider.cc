// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/guest_resource_provider.h"

#include "base/strings/string16.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

using content::RenderProcessHost;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;
using extensions::Extension;

namespace task_manager {

class GuestResource : public RendererResource {
 public:
  explicit GuestResource(content::RenderViewHost* render_view_host);
  virtual ~GuestResource();

  // Resource methods:
  virtual Type GetType() const OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetProfileName() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual const extensions::Extension* GetExtension() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestResource);
};

GuestResource::GuestResource(RenderViewHost* render_view_host)
    : RendererResource(
          render_view_host->GetSiteInstance()->GetProcess()->GetHandle(),
          render_view_host) {
}

GuestResource::~GuestResource() {
}

Resource::Type GuestResource::GetType() const {
  return GUEST;
}

string16 GuestResource::GetTitle() const {
  WebContents* web_contents = GetWebContents();
  const int message_id = IDS_TASK_MANAGER_WEBVIEW_TAG_PREFIX;
  if (web_contents) {
    string16 title = util::GetTitleFromWebContents(web_contents);
    return l10n_util::GetStringFUTF16(message_id, title);
  }
  return l10n_util::GetStringFUTF16(message_id, string16());
}

string16 GuestResource::GetProfileName() const {
  WebContents* web_contents = GetWebContents();
  if (web_contents) {
    Profile* profile = Profile::FromBrowserContext(
        web_contents->GetBrowserContext());
    return util::GetProfileNameFromInfoCache(profile);
  }
  return string16();
}

gfx::ImageSkia GuestResource::GetIcon() const {
  WebContents* web_contents = GetWebContents();
  if (web_contents && FaviconTabHelper::FromWebContents(web_contents)) {
    return FaviconTabHelper::FromWebContents(web_contents)->
        GetFavicon().AsImageSkia();
  }
  return gfx::ImageSkia();
}

WebContents* GuestResource::GetWebContents() const {
  return WebContents::FromRenderViewHost(render_view_host());
}

const Extension* GuestResource::GetExtension() const {
  return NULL;
}

GuestResourceProvider::GuestResourceProvider(TaskManager* task_manager)
    :  updating_(false),
       task_manager_(task_manager) {
}

GuestResourceProvider::~GuestResourceProvider() {
}

Resource* GuestResourceProvider::GetResource(
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

void GuestResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Add all the existing guest WebContents.
  scoped_ptr<content::RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (widget->IsRenderView()) {
      RenderViewHost* rvh = RenderViewHost::From(widget);
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

void GuestResourceProvider::StopUpdating() {
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

void GuestResourceProvider::Add(RenderViewHost* render_view_host) {
  GuestResource* resource = new GuestResource(render_view_host);
  resources_[render_view_host] = resource;
  task_manager_->AddResource(resource);
}

void GuestResourceProvider::Remove(RenderViewHost* render_view_host) {
  if (!updating_)
    return;

  GuestResourceMap::iterator iter = resources_.find(render_view_host);
  if (iter == resources_.end())
    return;

  GuestResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  resources_.erase(iter);
  delete resource;
}

void GuestResourceProvider::Observe(int type,
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

}  // namespace task_manager
