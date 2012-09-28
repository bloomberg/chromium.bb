// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/zoom/zoom_controller.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"

int ZoomController::kUserDataKey;

ZoomController::ZoomController(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      zoom_percent_(100),
      observer_(NULL) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  default_zoom_level_.Init(prefs::kDefaultZoomLevel, profile->GetPrefs(), this);

  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetForBrowserContext(profile);
  registrar_.Add(this, content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
                 content::Source<content::HostZoomMap>(zoom_map));

  UpdateState(std::string());
}

ZoomController::~ZoomController() {
  default_zoom_level_.Destroy();
  registrar_.RemoveAll();
}

bool ZoomController::IsAtDefaultZoom() const {
  return content::ZoomValuesEqual(web_contents()->GetZoomLevel(),
                                  default_zoom_level_.GetValue());
}

int ZoomController::GetResourceForZoomLevel() const {
  DCHECK(!IsAtDefaultZoom());
  double zoom = web_contents()->GetZoomLevel();
  return zoom > default_zoom_level_.GetValue() ? IDR_ZOOM_PLUS : IDR_ZOOM_MINUS;
}

void ZoomController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // If the main frame's content has changed, the new page may have a different
  // zoom level from the old one.
  UpdateState(std::string());
}

void ZoomController::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name = content::Details<std::string>(details).ptr();
      DCHECK(pref_name && *pref_name == prefs::kDefaultZoomLevel);
      UpdateState(std::string());
      break;
    }
    case content::NOTIFICATION_ZOOM_LEVEL_CHANGED:
      UpdateState(*content::Details<std::string>(details).ptr());
      break;
    default:
      NOTREACHED();
  }
}

void ZoomController::UpdateState(const std::string& host) {
  if (!host.empty() &&
      host != net::GetHostOrSpecFromURL(web_contents()->GetURL())) {
    return;
  }

  bool dummy;
  zoom_percent_ = web_contents()->GetZoomPercent(&dummy, &dummy);

  if (observer_)
    observer_->OnZoomChanged(web_contents(), !host.empty());
}
