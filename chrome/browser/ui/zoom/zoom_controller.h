// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ZOOM_ZOOM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ZOOM_ZOOM_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_member.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class ZoomObserver;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}  // namespace extensions

// Per-tab class to manage zoom changes and the Omnibox zoom icon.
class ZoomController : public content::WebContentsObserver,
                       public content::WebContentsUserData<ZoomController> {
 public:
  // Defines how zoom changes are handled.
  enum ZoomMode {
    // Results in default zoom behavior, i.e. zoom changes are handled
    // automatically and on a per-origin basis, meaning that other tabs
    // navigated to the same origin will also zoom.
    ZOOM_MODE_DEFAULT,
    // Results in zoom changes being handled automatically, but on a per-tab
    // basis. Tabs in this zoom mode will not be affected by zoom changes in
    // other tabs, and vice versa.
    ZOOM_MODE_ISOLATED,
    // Overrides the automatic handling of zoom changes. The |onZoomChange|
    // event will still be dispatched, but the page will not actually be zoomed.
    // These zoom changes can be handled manually by listening for the
    // |onZoomChange| event. Zooming in this mode is also on a per-tab basis.
    ZOOM_MODE_MANUAL,
    // Disables all zooming in this tab. The tab will revert to default (100%)
    // zoom, and all attempted zoom changes will be ignored.
    ZOOM_MODE_DISABLED,
  };

  struct ZoomChangedEventData {
    ZoomChangedEventData(content::WebContents* web_contents,
                         double old_zoom_level,
                         double new_zoom_level,
                         ZoomController::ZoomMode zoom_mode,
                         bool can_show_bubble)
        : web_contents(web_contents),
          old_zoom_level(old_zoom_level),
          new_zoom_level(new_zoom_level),
          zoom_mode(zoom_mode),
          can_show_bubble(can_show_bubble) {}
    content::WebContents* web_contents;
    double old_zoom_level;
    double new_zoom_level;
    ZoomController::ZoomMode zoom_mode;
    bool can_show_bubble;
  };

  virtual ~ZoomController();

  ZoomMode zoom_mode() const { return zoom_mode_; }

  // Convenience method to get default zoom level. Implemented here for
  // inlining.
  double GetDefaultZoomLevel() const {
    // TODO(wjmaclean) Make this refer to the webcontents-specific HostZoomMap
    // when that becomes available.
    return content::HostZoomMap::GetDefaultForBrowserContext(browser_context_)->
        GetDefaultZoomLevel();
  }

  // Convenience method to quickly check if the tab's at default zoom.
  bool IsAtDefaultZoom() const;

  // Returns which image should be loaded for the current zoom level.
  int GetResourceForZoomLevel() const;

  const extensions::Extension* last_extension() const {
    return last_extension_.get();
  }

  void AddObserver(ZoomObserver* observer);
  void RemoveObserver(ZoomObserver* observer);

  // Used to set whether the zoom notification bubble can be shown when the
  // zoom level is changed for this controller. Default behavior is to show
  // the bubble.
  void SetShowsNotificationBubble(bool can_show_bubble) {
    can_show_bubble_ = can_show_bubble;
  }

  // Gets the current zoom level by querying HostZoomMap (if not in manual zoom
  // mode) or from the ZoomController local value otherwise.
  double GetZoomLevel() const;
  // Calls GetZoomLevel() then converts the returned value to a percentage
  // zoom factor.
  // Virtual for testing.
  virtual int GetZoomPercent() const;

  // Sets the zoom level through HostZoomMap.
  // Returns true on success.
  bool SetZoomLevel(double zoom_level);

  // Sets the zoom level via HostZoomMap (or stores it locally if in manual zoom
  // mode), and attributes the zoom to |extension|. Returns true on success.
  bool SetZoomLevelByExtension(
      double zoom_level,
      const scoped_refptr<const extensions::Extension>& extension);

  // Sets the zoom mode, which defines zoom behavior (see enum ZoomMode).
  void SetZoomMode(ZoomMode zoom_mode);

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

 protected:
  // Protected for testing.
  explicit ZoomController(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<ZoomController>;
  friend class ZoomControllerTest;

  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);

  // Updates the zoom icon and zoom percentage based on current values and
  // notifies the observer if changes have occurred. |host| may be empty,
  // meaning the change should apply to ~all sites. If it is not empty, the
  // change only affects sites with the given host.
  void UpdateState(const std::string& host);

  // True if changes to zoom level can trigger the zoom notification bubble.
  bool can_show_bubble_;

  // The current zoom mode.
  ZoomMode zoom_mode_;

  // Current zoom level.
  double zoom_level_;

  scoped_ptr<ZoomChangedEventData> event_data_;

  // Keeps track of the extension (if any) that initiated the last zoom change
  // that took effect.
  scoped_refptr<const extensions::Extension> last_extension_;

  // Observer receiving notifications on state changes.
  ObserverList<ZoomObserver> observers_;

  content::BrowserContext* browser_context_;

  scoped_ptr<content::HostZoomMap::Subscription> zoom_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ZoomController);
};

#endif  // CHROME_BROWSER_UI_ZOOM_ZOOM_CONTROLLER_H_
