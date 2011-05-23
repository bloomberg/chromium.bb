// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "content/browser/host_zoom_map.h"

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebView;

HostZoomMap::HostZoomMap() : default_zoom_level_(0.0) {
  registrar_.Add(
      this, NotificationType::RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
      NotificationService::AllSources());
}

double HostZoomMap::GetZoomLevel(const GURL& url) const {
  std::string host(net::GetHostOrSpecFromURL(url));
  base::AutoLock auto_lock(lock_);
  HostZoomLevels::const_iterator i(host_zoom_levels_.find(host));
  return (i == host_zoom_levels_.end()) ? default_zoom_level_ : i->second;
}

void HostZoomMap::SetZoomLevel(const GURL& url, double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::string host(net::GetHostOrSpecFromURL(url));

  {
    base::AutoLock auto_lock(lock_);
    if (level == default_zoom_level_)
      host_zoom_levels_.erase(host);
    else
      host_zoom_levels_[host] = level;
  }

  NotificationService::current()->Notify(NotificationType::ZOOM_LEVEL_CHANGED,
                                         Source<HostZoomMap>(this),
                                         Details<const std::string>(&host));
}

double HostZoomMap::GetTemporaryZoomLevel(int render_process_id,
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

void HostZoomMap::SetTemporaryZoomLevel(int render_process_id,
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
      TemporaryZoomLevel temp;
      temp.render_process_id = render_process_id;
      temp.render_view_id = render_view_id;
      temp.zoom_level = level;
      temporary_zoom_levels_.push_back(temp);
    }
  }

  NotificationService::current()->Notify(NotificationType::ZOOM_LEVEL_CHANGED,
                                         Source<HostZoomMap>(this),
                                         NotificationService::NoDetails());
}

void HostZoomMap::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (type.value) {
    case NotificationType::RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW: {
      base::AutoLock auto_lock(lock_);
      int render_view_id = Source<RenderViewHost>(source)->routing_id();
      int render_process_id = Source<RenderViewHost>(source)->process()->id();

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

HostZoomMap::~HostZoomMap() {
}
