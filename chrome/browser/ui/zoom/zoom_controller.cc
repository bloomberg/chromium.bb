// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/zoom/zoom_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"

ZoomController::ZoomController(TabContentsWrapper* tab_contents)
    : content::WebContentsObserver(tab_contents->web_contents()),
      zoom_icon_state_(NONE),
      zoom_percent_(100),
      tab_contents_wrapper_(tab_contents),
      observer_(NULL) {
  default_zoom_level_.Init(prefs::kDefaultZoomLevel,
                           tab_contents->profile()->GetPrefs(), this);
  registrar_.Add(this, content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
                 content::NotificationService::AllBrowserContextsAndSources());

  UpdateState();
}

ZoomController::~ZoomController() {
  default_zoom_level_.Destroy();
  registrar_.RemoveAll();
}

void ZoomController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // If the main frame's content has changed, the new page may have a different
  // zoom level from the old one.
  UpdateState();
}

void ZoomController::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name = content::Details<std::string>(details).ptr();
      DCHECK(pref_name && *pref_name == prefs::kDefaultZoomLevel);
    }
    // Fall through.
    case content::NOTIFICATION_ZOOM_LEVEL_CHANGED:
      UpdateState();
      break;
    default:
      NOTREACHED();
  }
}

void ZoomController::UpdateState() {
  double current_zoom_level =
      tab_contents_wrapper_->web_contents()->GetZoomLevel();
  double default_zoom_level = default_zoom_level_.GetValue();

  ZoomIconState state;
  if (content::ZoomValuesEqual(current_zoom_level, default_zoom_level))
    state = NONE;
  else if (current_zoom_level > default_zoom_level)
    state = ZOOM_PLUS_ICON;
  else
    state = ZOOM_MINUS_ICON;

  bool dummy;
  int zoom_percent = tab_contents_wrapper_->web_contents()->
      GetZoomPercent(&dummy, &dummy);

  if (state != zoom_icon_state_) {
    zoom_icon_state_ = state;
    if (observer_)
      observer_->OnZoomIconChanged(tab_contents_wrapper_, state);
  }

  if (zoom_percent != zoom_percent_) {
    zoom_percent_ = zoom_percent;
    if (observer_)
      observer_->OnZoomChanged(tab_contents_wrapper_,
                               zoom_percent,
                               state != NONE);
  }
}
