// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_map_impl.h"

#include <algorithm>
#include <cmath>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/page_zoom.h"
#include "net/base/net_util.h"

static const char* kHostZoomMapKeyName = "content_host_zoom_map";

namespace content {

HostZoomMap* HostZoomMap::GetForBrowserContext(BrowserContext* context) {
  HostZoomMapImpl* rv = static_cast<HostZoomMapImpl*>(
      context->GetUserData(kHostZoomMapKeyName));
  if (!rv) {
    rv = new HostZoomMapImpl();
    context->SetUserData(kHostZoomMapKeyName, rv);
  }
  return rv;
}

// Helper function for setting/getting zoom levels for WebContents without
// having to import HostZoomMapImpl everywhere.
double HostZoomMap::GetZoomLevel(const WebContents* web_contents) {
  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForBrowserContext(web_contents->GetBrowserContext()));
  return host_zoom_map->GetZoomLevelForWebContents(
      *static_cast<const WebContentsImpl*>(web_contents));
}

void HostZoomMap::SetZoomLevel(const WebContents* web_contents, double level) {
  HostZoomMapImpl* host_zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForBrowserContext(web_contents->GetBrowserContext()));
  host_zoom_map->SetZoomLevelForWebContents(
      *static_cast<const WebContentsImpl*>(web_contents), level);
}

HostZoomMapImpl::HostZoomMapImpl()
    : default_zoom_level_(0.0) {
  registrar_.Add(
      this, NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
      NotificationService::AllSources());
}

void HostZoomMapImpl::CopyFrom(HostZoomMap* copy_interface) {
  // This can only be called on the UI thread to avoid deadlocks, otherwise
  //   UI: a.CopyFrom(b);
  //   IO: b.CopyFrom(a);
  // can deadlock.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  HostZoomMapImpl* copy = static_cast<HostZoomMapImpl*>(copy_interface);
  base::AutoLock auto_lock(lock_);
  base::AutoLock copy_auto_lock(copy->lock_);
  host_zoom_levels_.
      insert(copy->host_zoom_levels_.begin(), copy->host_zoom_levels_.end());
  for (SchemeHostZoomLevels::const_iterator i(copy->
           scheme_host_zoom_levels_.begin());
       i != copy->scheme_host_zoom_levels_.end(); ++i) {
    scheme_host_zoom_levels_[i->first] = HostZoomLevels();
    scheme_host_zoom_levels_[i->first].
        insert(i->second.begin(), i->second.end());
  }
  default_zoom_level_ = copy->default_zoom_level_;
}

double HostZoomMapImpl::GetZoomLevelForHost(const std::string& host) const {
  base::AutoLock auto_lock(lock_);
  HostZoomLevels::const_iterator i(host_zoom_levels_.find(host));
  return (i == host_zoom_levels_.end()) ? default_zoom_level_ : i->second;
}

double HostZoomMapImpl::GetZoomLevelForHostAndScheme(
    const std::string& scheme,
    const std::string& host) const {
  {
    base::AutoLock auto_lock(lock_);
    SchemeHostZoomLevels::const_iterator scheme_iterator(
        scheme_host_zoom_levels_.find(scheme));
    if (scheme_iterator != scheme_host_zoom_levels_.end()) {
      HostZoomLevels::const_iterator i(scheme_iterator->second.find(host));
      if (i != scheme_iterator->second.end())
        return i->second;
    }
  }
  return GetZoomLevelForHost(host);
}

HostZoomMap::ZoomLevelVector HostZoomMapImpl::GetAllZoomLevels() const {
  HostZoomMap::ZoomLevelVector result;
  {
    base::AutoLock auto_lock(lock_);
    result.reserve(host_zoom_levels_.size() + scheme_host_zoom_levels_.size());
    for (HostZoomLevels::const_iterator i = host_zoom_levels_.begin();
         i != host_zoom_levels_.end();
         ++i) {
      ZoomLevelChange change = {HostZoomMap::ZOOM_CHANGED_FOR_HOST,
                                i->first,       // host
                                std::string(),  // scheme
                                i->second       // zoom level
      };
      result.push_back(change);
    }
    for (SchemeHostZoomLevels::const_iterator i =
             scheme_host_zoom_levels_.begin();
         i != scheme_host_zoom_levels_.end();
         ++i) {
      const std::string& scheme = i->first;
      const HostZoomLevels& host_zoom_levels = i->second;
      for (HostZoomLevels::const_iterator j = host_zoom_levels.begin();
           j != host_zoom_levels.end();
           ++j) {
        ZoomLevelChange change = {HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST,
                                  j->first,  // host
                                  scheme,    // scheme
                                  j->second  // zoom level
        };
        result.push_back(change);
      }
    }
  }
  return result;
}

void HostZoomMapImpl::SetZoomLevelForHost(const std::string& host,
                                          double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock auto_lock(lock_);

    if (ZoomValuesEqual(level, default_zoom_level_))
      host_zoom_levels_.erase(host);
    else
      host_zoom_levels_[host] = level;
  }

  // Notify renderers from this browser context.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* render_process_host = i.GetCurrentValue();
    if (HostZoomMap::GetForBrowserContext(
            render_process_host->GetBrowserContext()) == this) {
      render_process_host->Send(
          new ViewMsg_SetZoomLevelForCurrentURL(std::string(), host, level));
    }
  }
  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_FOR_HOST;
  change.host = host;
  change.zoom_level = level;

  zoom_level_changed_callbacks_.Notify(change);
}

void HostZoomMapImpl::SetZoomLevelForHostAndScheme(const std::string& scheme,
                                                   const std::string& host,
                                                   double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  {
    base::AutoLock auto_lock(lock_);
    scheme_host_zoom_levels_[scheme][host] = level;
  }

  // Notify renderers from this browser context.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* render_process_host = i.GetCurrentValue();
    if (HostZoomMap::GetForBrowserContext(
            render_process_host->GetBrowserContext()) == this) {
      render_process_host->Send(
          new ViewMsg_SetZoomLevelForCurrentURL(scheme, host, level));
    }
  }

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST;
  change.host = host;
  change.scheme = scheme;
  change.zoom_level = level;

  zoom_level_changed_callbacks_.Notify(change);
}

double HostZoomMapImpl::GetDefaultZoomLevel() const {
  return default_zoom_level_;
}

void HostZoomMapImpl::SetDefaultZoomLevel(double level) {
  default_zoom_level_ = level;
}

scoped_ptr<HostZoomMap::Subscription>
HostZoomMapImpl::AddZoomLevelChangedCallback(
    const ZoomLevelChangedCallback& callback) {
  return zoom_level_changed_callbacks_.Add(callback);
}

double HostZoomMapImpl::GetZoomLevelForWebContents(
    const WebContentsImpl& web_contents_impl) const {
  int render_process_id = web_contents_impl.GetRenderProcessHost()->GetID();
  int routing_id = web_contents_impl.GetRenderViewHost()->GetRoutingID();

  if (UsesTemporaryZoomLevel(render_process_id, routing_id))
    return GetTemporaryZoomLevel(render_process_id, routing_id);

  // Since zoom map is updated using the url as stored in the navigation
  // controller, we use that URL to get the zoom level.
  GURL url;
  NavigationEntry* entry =
      web_contents_impl.GetController().GetLastCommittedEntry();
  if (entry)
    url =  entry->GetURL();
  return GetZoomLevelForHostAndScheme(url.scheme(),
                                      net::GetHostOrSpecFromURL(url));
}

void HostZoomMapImpl::SetZoomLevelForWebContents(
    const WebContentsImpl& web_contents_impl,
    double level) {
  int render_process_id = web_contents_impl.GetRenderProcessHost()->GetID();
  int render_view_id = web_contents_impl.GetRenderViewHost()->GetRoutingID();
  if (UsesTemporaryZoomLevel(render_process_id, render_view_id)) {

    SetTemporaryZoomLevel(render_process_id, render_view_id, level);
  } else {
    SetZoomLevelForHost(
        net::GetHostOrSpecFromURL(web_contents_impl.GetLastCommittedURL()),
        level);
  }
}

void HostZoomMapImpl::SetZoomLevelForView(int render_process_id,
                                          int render_view_id,
                                          double level,
                                          const std::string& host) {
  if (UsesTemporaryZoomLevel(render_process_id, render_view_id))
    SetTemporaryZoomLevel(render_process_id, render_view_id, level);
  else
    SetZoomLevelForHost(host, level);
}

bool HostZoomMapImpl::UsesTemporaryZoomLevel(int render_process_id,
                                             int render_view_id) const {
  TemporaryZoomLevel zoom_level(render_process_id, render_view_id);

  base::AutoLock auto_lock(lock_);
  TemporaryZoomLevels::const_iterator it = std::find(
      temporary_zoom_levels_.begin(), temporary_zoom_levels_.end(), zoom_level);
  return it != temporary_zoom_levels_.end();
}

void HostZoomMapImpl::SetUsesTemporaryZoomLevel(
    int render_process_id,
    int render_view_id,
    bool uses_temporary_zoom_level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TemporaryZoomLevel zoom_level(
      render_process_id, render_view_id, default_zoom_level_);

  base::AutoLock auto_lock(lock_);
  TemporaryZoomLevels::iterator it = std::find(
      temporary_zoom_levels_.begin(), temporary_zoom_levels_.end(), zoom_level);
  if (uses_temporary_zoom_level) {
    if (it == temporary_zoom_levels_.end())
      temporary_zoom_levels_.push_back(zoom_level);
  } else if (it != temporary_zoom_levels_.end()) {
      temporary_zoom_levels_.erase(it);
  }
}

double HostZoomMapImpl::GetTemporaryZoomLevel(int render_process_id,
                                              int render_view_id) const {
  base::AutoLock auto_lock(lock_);
  for (size_t i = 0; i < temporary_zoom_levels_.size(); ++i) {
    if (temporary_zoom_levels_[i].render_process_id == render_process_id &&
        temporary_zoom_levels_[i].render_view_id == render_view_id) {
      return temporary_zoom_levels_[i].zoom_level;
    }
  }

  return 0;
}

void HostZoomMapImpl::SetTemporaryZoomLevel(int render_process_id,
                                            int render_view_id,
                                            double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock auto_lock(lock_);
    size_t i;
    for (i = 0; i < temporary_zoom_levels_.size(); ++i) {
      if (temporary_zoom_levels_[i].render_process_id == render_process_id &&
          temporary_zoom_levels_[i].render_view_id == render_view_id) {
        if (level) {
          temporary_zoom_levels_[i].zoom_level = level;
        } else {
          temporary_zoom_levels_.erase(temporary_zoom_levels_.begin() + i);
        }
        break;
      }
    }

    if (level && i == temporary_zoom_levels_.size()) {
      TemporaryZoomLevel temp(render_process_id, render_view_id, level);
      temporary_zoom_levels_.push_back(temp);
    }
  }

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM;
  change.zoom_level = level;

  zoom_level_changed_callbacks_.Notify(change);
}

void HostZoomMapImpl::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW: {
      base::AutoLock auto_lock(lock_);
      int render_view_id = Source<RenderViewHost>(source)->GetRoutingID();
      int render_process_id =
          Source<RenderViewHost>(source)->GetProcess()->GetID();

      for (size_t i = 0; i < temporary_zoom_levels_.size(); ++i) {
        if (temporary_zoom_levels_[i].render_process_id == render_process_id &&
            temporary_zoom_levels_[i].render_view_id == render_view_id) {
          temporary_zoom_levels_.erase(temporary_zoom_levels_.begin() + i);
          break;
        }
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected preference observed.";
  }
}

HostZoomMapImpl::~HostZoomMapImpl() {
}

HostZoomMapImpl::TemporaryZoomLevel::TemporaryZoomLevel(int process_id,
                                                        int view_id,
                                                        double level)
    : render_process_id(process_id),
      render_view_id(view_id),
      zoom_level(level) {
}

HostZoomMapImpl::TemporaryZoomLevel::TemporaryZoomLevel(int process_id,
                                                        int view_id)
    : render_process_id(process_id),
      render_view_id(view_id),
      zoom_level(0.0) {
}

bool HostZoomMapImpl::TemporaryZoomLevel::operator==(
    const TemporaryZoomLevel& other) const {
  return other.render_process_id == render_process_id &&
         other.render_view_id == render_view_id;
}

}  // namespace content
