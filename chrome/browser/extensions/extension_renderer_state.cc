// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_renderer_state.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::RenderProcessHost;
using content::RenderViewHost;
using content::WebContents;

//
// ExtensionRendererState::TabObserver
//

// This class listens for notifications about changes in renderer state on the
// UI thread, and notifies the ExtensionRendererState on the IO thread. It
// should only ever be accessed on the UI thread.
class ExtensionRendererState::TabObserver
    : public content::NotificationObserver {
 public:
  TabObserver();
  ~TabObserver();

 private:
  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  content::NotificationRegistrar registrar_;
};

ExtensionRendererState::TabObserver::TabObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_DELETED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_TAB_PARENTED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_RETARGETING,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

ExtensionRendererState::TabObserver::~TabObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ExtensionRendererState::TabObserver::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents);
      if (!session_tab_helper)
        return;
      RenderViewHost* host = content::Details<RenderViewHost>(details).ptr();
      // TODO(mpcomplete): How can we tell if window_id is bogus? It may not
      // have been set yet.
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &ExtensionRendererState::SetTabAndWindowId,
              base::Unretained(ExtensionRendererState::GetInstance()),
              host->GetProcess()->GetID(), host->GetRoutingID(),
              session_tab_helper->session_id().id(),
              session_tab_helper->window_id().id()));
      break;
    }
    case chrome::NOTIFICATION_TAB_PARENTED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents);
      if (!session_tab_helper)
        return;
      RenderViewHost* host = web_contents->GetRenderViewHost();
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &ExtensionRendererState::SetTabAndWindowId,
              base::Unretained(ExtensionRendererState::GetInstance()),
              host->GetProcess()->GetID(), host->GetRoutingID(),
              session_tab_helper->session_id().id(),
              session_tab_helper->window_id().id()));
      break;
    }
    case chrome::NOTIFICATION_RETARGETING: {
      RetargetingDetails* retargeting_details =
          content::Details<RetargetingDetails>(details).ptr();
      WebContents* web_contents = retargeting_details->target_web_contents;
      SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents);
      if (!session_tab_helper)
        return;
      RenderViewHost* host = web_contents->GetRenderViewHost();
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &ExtensionRendererState::SetTabAndWindowId,
              base::Unretained(ExtensionRendererState::GetInstance()),
              host->GetProcess()->GetID(), host->GetRoutingID(),
              session_tab_helper->session_id().id(),
              session_tab_helper->window_id().id()));
      break;
    }
    case content::NOTIFICATION_RENDER_VIEW_HOST_DELETED: {
      RenderViewHost* host = content::Source<RenderViewHost>(source).ptr();
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &ExtensionRendererState::ClearTabAndWindowId,
              base::Unretained(ExtensionRendererState::GetInstance()),
              host->GetProcess()->GetID(), host->GetRoutingID()));
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      RenderProcessHost* render_process_host =
          content::Source<RenderProcessHost>(source).ptr();
      if (!render_process_host->IsGuest())
        return;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &ExtensionRendererState::AddGuestProcess,
              base::Unretained(ExtensionRendererState::GetInstance()),
              render_process_host->GetID()));
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      RenderProcessHost* render_process_host =
          content::Source<RenderProcessHost>(source).ptr();
      if (!render_process_host->IsGuest())
        return;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &ExtensionRendererState::RemoveGuestProcess,
              base::Unretained(ExtensionRendererState::GetInstance()),
              render_process_host->GetID()));
      break;
    }
    default:
      NOTREACHED();
      return;
  }
}

//
// ExtensionRendererState
//

ExtensionRendererState::ExtensionRendererState() : observer_(NULL) {
}

ExtensionRendererState::~ExtensionRendererState() {
}

// static
ExtensionRendererState* ExtensionRendererState::GetInstance() {
  return Singleton<ExtensionRendererState>::get();
}

void ExtensionRendererState::Init() {
  observer_ = new TabObserver;
}

void ExtensionRendererState::Shutdown() {
  delete observer_;
}

void ExtensionRendererState::SetTabAndWindowId(
    int render_process_host_id, int routing_id, int tab_id, int window_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RenderId render_id(render_process_host_id, routing_id);
  map_[render_id] = TabAndWindowId(tab_id, window_id);
}

void ExtensionRendererState::ClearTabAndWindowId(
    int render_process_host_id, int routing_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RenderId render_id(render_process_host_id, routing_id);
  map_.erase(render_id);
}

bool ExtensionRendererState::GetTabAndWindowId(
    int render_process_host_id, int routing_id, int* tab_id, int* window_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RenderId render_id(render_process_host_id, routing_id);
  TabAndWindowIdMap::iterator iter = map_.find(render_id);
  if (iter != map_.end()) {
    *tab_id = iter->second.first;
    *window_id = iter->second.second;
    return true;
  }
  return false;
}

void ExtensionRendererState::AddGuestProcess(int render_process_host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  guest_set_.insert(render_process_host_id);
}

void ExtensionRendererState::RemoveGuestProcess(int render_process_host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  guest_set_.erase(render_process_host_id);
}

bool ExtensionRendererState::IsGuestProcess(int render_process_host_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return guest_set_.count(render_process_host_id) > 0;
}
