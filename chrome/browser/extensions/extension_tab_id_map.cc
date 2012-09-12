// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_id_map.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
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
using content::RenderViewHost;
using content::WebContents;

//
// ExtensionTabIdMap::TabObserver
//

// This class listens for notifications about new and closed tabs on the UI
// thread, and notifies the ExtensionTabIdMap on the IO thread. It should only
// ever be accessed on the UI thread.
class ExtensionTabIdMap::TabObserver : public content::NotificationObserver {
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

ExtensionTabIdMap::TabObserver::TabObserver() {
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
}

ExtensionTabIdMap::TabObserver::~TabObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ExtensionTabIdMap::TabObserver::Observe(
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
              &ExtensionTabIdMap::SetTabAndWindowId,
              base::Unretained(ExtensionTabIdMap::GetInstance()),
              host->GetProcess()->GetID(), host->GetRoutingID(),
              session_tab_helper->session_id().id(),
              session_tab_helper->window_id().id()));
      break;
    }
    case chrome::NOTIFICATION_TAB_PARENTED: {
      TabContents* tab = content::Source<TabContents>(source).ptr();
      WebContents* web_contents = tab->web_contents();
      SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents);
      if (!session_tab_helper)
        return;
      RenderViewHost* host = web_contents->GetRenderViewHost();
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &ExtensionTabIdMap::SetTabAndWindowId,
              base::Unretained(ExtensionTabIdMap::GetInstance()),
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
              &ExtensionTabIdMap::SetTabAndWindowId,
              base::Unretained(ExtensionTabIdMap::GetInstance()),
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
              &ExtensionTabIdMap::ClearTabAndWindowId,
              base::Unretained(ExtensionTabIdMap::GetInstance()),
              host->GetProcess()->GetID(), host->GetRoutingID()));
      break;
    }
    default:
      NOTREACHED();
      return;
  }
}

//
// ExtensionTabIdMap
//

ExtensionTabIdMap::ExtensionTabIdMap() : observer_(NULL) {
}

ExtensionTabIdMap::~ExtensionTabIdMap() {
}

// static
ExtensionTabIdMap* ExtensionTabIdMap::GetInstance() {
  return Singleton<ExtensionTabIdMap>::get();
}

void ExtensionTabIdMap::Init() {
  observer_ = new TabObserver;
}

void ExtensionTabIdMap::Shutdown() {
  delete observer_;
}

void ExtensionTabIdMap::SetTabAndWindowId(
    int render_process_host_id, int routing_id, int tab_id, int window_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RenderId render_id(render_process_host_id, routing_id);
  map_[render_id] = TabAndWindowId(tab_id, window_id);
}

void ExtensionTabIdMap::ClearTabAndWindowId(
    int render_process_host_id, int routing_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RenderId render_id(render_process_host_id, routing_id);
  map_.erase(render_id);
}

bool ExtensionTabIdMap::GetTabAndWindowId(
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
