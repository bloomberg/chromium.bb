// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ZOOM_ZOOM_EVENT_MANAGER_H_
#define CHROME_BROWSER_UI_ZOOM_ZOOM_EVENT_MANAGER_H_

#include "base/callback_list.h"
#include "base/supports_user_data.h"
#include "content/public/browser/host_zoom_map.h"

namespace content {
class BrowserContext;
}  // namespace content

// This class serves as a target for event notifications from all ZoomController
// objects. Classes that need to know about browser-specific zoom events (e.g.
// manual-mode zoom) should subscribe here.
class ZoomEventManager : public base::SupportsUserData::Data {
 public:
  ZoomEventManager();
  virtual ~ZoomEventManager();

  // Returns the ZoomEventManager for the specified BrowserContext. This
  // function creates the ZoomEventManager if it hasn't been created already.
  static ZoomEventManager* GetForBrowserContext(
      content::BrowserContext* context);

  // Called by ZoomControllers when changes are made to zoom levels in manual
  // mode in order that browser listeners can be notified.
  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

  // Add and remove zoom level changed callbacks.
  scoped_ptr<content::HostZoomMap::Subscription> AddZoomLevelChangedCallback(
      const content::HostZoomMap::ZoomLevelChangedCallback& callback);

 private:
  base::CallbackList<void(const content::HostZoomMap::ZoomLevelChange&)>
      zoom_level_changed_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ZoomEventManager);
};

#endif  // CHROME_BROWSER_UI_ZOOM_ZOOM_EVENT_MANAGER_H_
