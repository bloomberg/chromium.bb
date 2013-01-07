// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/zoom/zoom_controller.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ZoomController);

ZoomController::ZoomController(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      zoom_percent_(100),
      observer_(NULL) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  default_zoom_level_.Init(prefs::kDefaultZoomLevel, profile->GetPrefs(),
                           base::Bind(&ZoomController::UpdateState,
                                      base::Unretained(this),
                                      std::string()));

  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetForBrowserContext(profile);
  registrar_.Add(this, content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
                 content::Source<content::HostZoomMap>(zoom_map));

  UpdateState(std::string());
}

ZoomController::~ZoomController() {}

bool ZoomController::IsAtDefaultZoom() const {
  if (!web_contents())
    return true;

  return content::ZoomValuesEqual(web_contents()->GetZoomLevel(),
                                  default_zoom_level_.GetValue());
}

int ZoomController::GetResourceForZoomLevel() const {
  if (!web_contents())
    return IDR_ZOOM_MINUS;

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
  DCHECK_EQ(content::NOTIFICATION_ZOOM_LEVEL_CHANGED, type);
  UpdateState(*content::Details<std::string>(details).ptr());
}

void ZoomController::UpdateState(const std::string& host) {
  // TODO(dbeam): I'm not totally sure why this is happening, and there's been a
  // bit of effort to understand with no tangible results yet. It's possible
  // that WebContents is NULL as it's being destroyed or some other random
  // reason that I haven't found yet. Applying band-aid. http://crbug.com/144879
  if (!web_contents())
    return;

  // If |host| is empty, all observers should be updated.
  if (!host.empty()) {
    // Use the active navigation entry's URL instead of the WebContents' so
    // virtual URLs work (e.g. chrome://settings). http://crbug.com/153950
    content::NavigationEntry* active_entry =
        web_contents()->GetController().GetActiveEntry();
    if (!active_entry ||
        host != net::GetHostOrSpecFromURL(active_entry->GetURL())) {
      return;
    }
  }

  bool dummy;
  zoom_percent_ = web_contents()->GetZoomPercent(&dummy, &dummy);

  if (observer_)
    observer_->OnZoomChanged(web_contents(), !host.empty());
}
