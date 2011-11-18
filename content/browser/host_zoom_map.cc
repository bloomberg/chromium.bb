// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "content/browser/host_zoom_map.h"

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using content::BrowserThread;
using WebKit::WebView;

HostZoomMap::HostZoomMap()
    : default_zoom_level_(0.0),
      original_(this) {
  Init();
}

HostZoomMap::HostZoomMap(HostZoomMap* original)
    : default_zoom_level_(0.0),
      original_(original) {
  DCHECK(original);
  Init();
  base::AutoLock auto_lock(original->lock_);
  for (HostZoomLevels::const_iterator i(original->host_zoom_levels_.begin());
       i != original->host_zoom_levels_.end(); ++i) {
    host_zoom_levels_[i->first] = i->second;
  }
}

void HostZoomMap::Init() {
  registrar_.Add(
      this, content::NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
      content::NotificationService::AllSources());
}

double HostZoomMap::GetZoomLevel(const std::string& host) const {
  base::AutoLock auto_lock(lock_);
  HostZoomLevels::const_iterator i(host_zoom_levels_.find(host));
  return (i == host_zoom_levels_.end()) ? default_zoom_level_ : i->second;
}

void HostZoomMap::SetZoomLevel(std::string host, double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock auto_lock(lock_);
    if (level == default_zoom_level_)
      host_zoom_levels_.erase(host);
    else
      host_zoom_levels_[host] = level;
  }

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
      content::Source<HostZoomMap>(this),
      content::Details<const std::string>(&host));
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

  std::string host;
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
      content::Source<HostZoomMap>(this),
      content::Details<const std::string>(&host));
}

void HostZoomMap::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW: {
      base::AutoLock auto_lock(lock_);
      int render_view_id =
          content::Source<RenderViewHost>(source)->routing_id();
      int render_process_id =
          content::Source<RenderViewHost>(source)->process()->GetID();

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
