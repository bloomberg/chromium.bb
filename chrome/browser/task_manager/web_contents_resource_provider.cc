// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/web_contents_resource_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "chrome/browser/task_manager/web_contents_information.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

using content::WebContents;
using content::RenderViewHost;

namespace task_manager {

// A WebContentsObserver that tracks changes to a WebContents on behalf of
// a WebContentsResourceProvider.
class TaskManagerWebContentsObserver : public content::WebContentsObserver {
 public:
  TaskManagerWebContentsObserver(WebContents* web_contents,
                                 WebContentsResourceProvider* provider)
      : content::WebContentsObserver(web_contents), provider_(provider) {}

  // content::WebContentsObserver implementation.
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) OVERRIDE {
    provider_->RemoveFromTaskManager(web_contents());
    provider_->AddToTaskManager(web_contents());
  }

  virtual void RenderViewReady() OVERRIDE {
    provider_->RemoveFromTaskManager(web_contents());
    provider_->AddToTaskManager(web_contents());
  }

  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE {
    provider_->RemoveFromTaskManager(web_contents());
  }

  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE {
    provider_->RemoveFromTaskManager(web_contents);
    provider_->DeleteObserver(this);  // Deletes |this|.
  }

 private:
  WebContentsResourceProvider* provider_;
};

////////////////////////////////////////////////////////////////////////////////
// WebContentsResourceProvider class
////////////////////////////////////////////////////////////////////////////////

WebContentsResourceProvider::WebContentsResourceProvider(
    TaskManager* task_manager,
    scoped_ptr<WebContentsInformation> info)
    : updating_(false), task_manager_(task_manager), info_(info.Pass()) {}

WebContentsResourceProvider::~WebContentsResourceProvider() {}

RendererResource* WebContentsResourceProvider::GetResource(int origin_pid,
                                                           int child_id,
                                                           int route_id) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(child_id, route_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  // If an origin PID was specified then the request originated in a plugin
  // working on the WebContents's behalf, so ignore it.
  if (origin_pid)
    return NULL;

  std::map<WebContents*, RendererResource*>::iterator res_iter =
      resources_.find(web_contents);

  if (res_iter == resources_.end()) {
    // Can happen if the tab was closed while a network request was being
    // performed.
    return NULL;
  }
  return res_iter->second;
}

void WebContentsResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  WebContentsInformation::NewWebContentsCallback new_web_contents_callback =
      base::Bind(&WebContentsResourceProvider::OnWebContentsCreated, this);
  info_->GetAll(new_web_contents_callback);
  info_->StartObservingCreation(new_web_contents_callback);
}

void WebContentsResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  info_->StopObservingCreation();

  // Delete all observers; this dissassociates them from the WebContents too.
  STLDeleteElements(&web_contents_observers_);
  web_contents_observers_.clear();

  // Delete all resources. We don't need to remove them from the TaskManager,
  // because it's the TaskManager that's asking us to StopUpdating().
  STLDeleteValues(&resources_);
  resources_.clear();
}

void WebContentsResourceProvider::OnWebContentsCreated(
    WebContents* web_contents) {
  // Don't add dead tabs or tabs that haven't yet connected.
  if (!web_contents->GetRenderProcessHost()->GetHandle() ||
      !web_contents->WillNotifyDisconnection()) {
    return;
  }

  DCHECK(info_->CheckOwnership(web_contents));
  if (AddToTaskManager(web_contents)) {
    web_contents_observers_.insert(
        new TaskManagerWebContentsObserver(web_contents, this));
  }
}

bool WebContentsResourceProvider::AddToTaskManager(WebContents* web_contents) {
  if (!updating_)
    return false;

  if (resources_.count(web_contents)) {
    // The case may happen that we have added a WebContents as part of the
    // iteration performed during StartUpdating() call but the notification that
    // it has connected was not fired yet. So when the notification happens, we
    // are already observing this WebContents and just ignore it.
    return false;
  }

  // TODO(nick): If the RenderView is not live, then do we still want to install
  // the WebContentsObserver? Only some of the original ResourceProviders
  // had that check.
  scoped_ptr<RendererResource> resource = info_->MakeResource(web_contents);
  if (!resource)
    return false;

  task_manager_->AddResource(resource.get());
  resources_[web_contents] = resource.release();
  return true;
}

void WebContentsResourceProvider::RemoveFromTaskManager(
    WebContents* web_contents) {
  if (!updating_)
    return;

  std::map<WebContents*, RendererResource*>::iterator resource_iter =
      resources_.find(web_contents);

  if (resource_iter == resources_.end()) {
    return;
  }

  RendererResource* resource = resource_iter->second;
  task_manager_->RemoveResource(resource);

  // Remove the resource from the Task Manager.
  // And from the provider.
  resources_.erase(resource_iter);

  // Finally, delete the resource.
  delete resource;
}

void WebContentsResourceProvider::DeleteObserver(
    TaskManagerWebContentsObserver* observer) {
  if (!web_contents_observers_.erase(observer)) {
    NOTREACHED();
    return;
  }
  delete observer;  // Typically, this is our caller. Deletion is okay.
}

}  // namespace task_manager
