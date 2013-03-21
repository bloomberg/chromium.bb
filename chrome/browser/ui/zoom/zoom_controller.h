// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ZOOM_ZOOM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ZOOM_ZOOM_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_member.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class ZoomObserver;

namespace content {
class WebContents;
}

// Per-tab class to manage the Omnibox zoom icon.
class ZoomController : public content::WebContentsObserver,
                       public content::WebContentsUserData<ZoomController> {
 public:
  virtual ~ZoomController();

  int zoom_percent() const { return zoom_percent_; }

  // Convenience method to quickly check if the tab's at default zoom.
  bool IsAtDefaultZoom() const;

  // Returns which image should be loaded for the current zoom level.
  int GetResourceForZoomLevel() const;

  void set_observer(ZoomObserver* observer) { observer_ = observer; }

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  explicit ZoomController(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ZoomController>;
  friend class ZoomControllerTest;

  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

  // Updates the zoom icon and zoom percentage based on current values and
  // notifies the observer if changes have occurred. |host| may be empty,
  // meaning the change should apply to ~all sites. If it is not empty, the
  // change only affects sites with the given host.
  void UpdateState(const std::string& host);

  // The current zoom percentage.
  int zoom_percent_;

  // Used to access the default zoom level preference.
  DoublePrefMember default_zoom_level_;

  // Observer receiving notifications on state changes.
  ZoomObserver* observer_;

  content::BrowserContext* browser_context_;

  content::HostZoomMap::ZoomLevelChangedCallback zoom_callback_;

  DISALLOW_COPY_AND_ASSIGN(ZoomController);
};

#endif  // CHROME_BROWSER_UI_ZOOM_ZOOM_CONTROLLER_H_
