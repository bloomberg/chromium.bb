// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_DELEGATE_HELPER_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_DELEGATE_HELPER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "chrome/browser/dom_ui/web_ui_factory.h"
#include "chrome/common/window_container_type.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/window_open_disposition.h"

class BackgroundContents;
class Profile;
class RenderProcessHost;
class RenderViewHost;
class RenderViewHostDelegate;
class RenderWidgetHost;
class RenderWidgetHostView;
class SiteInstance;
class TabContents;

// Provides helper methods that provide common implementations of some
// RenderViewHostDelegate::View methods.
class RenderViewHostDelegateViewHelper {
 public:
  RenderViewHostDelegateViewHelper();
  virtual ~RenderViewHostDelegateViewHelper();

  // Creates a new renderer for window.open. This will either be a
  // BackgroundContents (if the window_container_type ==
  // WINDOW_CONTAINER_TYPE_BACKGROUND and permissions allow) or a TabContents.
  // If a TabContents is created, it is returned. Otherwise NULL is returned.
  virtual TabContents* CreateNewWindow(
      int route_id,
      Profile* profile,
      SiteInstance* site,
      WebUITypeID webui_type,
      RenderViewHostDelegate* opener,
      WindowContainerType window_container_type,
      const string16& frame_name);

  // Creates a new RenderWidgetHost and saves it for later retrieval by
  // GetCreatedWidget.
  virtual RenderWidgetHostView* CreateNewWidget(int route_id,
                                                WebKit::WebPopupType popup_type,
                                                RenderProcessHost* process);

  virtual RenderWidgetHostView* CreateNewFullscreenWidget(
      int route_id,
      WebKit::WebPopupType popup_type,
      RenderProcessHost* process);

  // Finds the new RenderWidgetHost and returns it. Note that this can only be
  // called once as this call also removes it from the internal map.
  virtual RenderWidgetHostView* GetCreatedWidget(int route_id);

  // Finds the new RenderViewHost/Delegate by route_id, initializes it for
  // for renderer-initiated creation, and returns the TabContents that needs
  // to be shown, if there is one (i.e. not a BackgroundContents). Note that
  // this can only be called once as this call also removes it from the internal
  // map.
  virtual TabContents* GetCreatedWindow(int route_id);

  // Removes |host| from the internal map of pending RenderWidgets.
  void RenderWidgetHostDestroyed(RenderWidgetHost* host);

 private:
  BackgroundContents* MaybeCreateBackgroundContents(
      int route_id,
      Profile* profile,
      SiteInstance* site,
      const GURL& opener_url,
      const string16& frame_name);

  // Tracks created RenderViewHost objects that have not been shown yet.
  // They are identified by the route ID passed to CreateNewWindow.
  typedef std::map<int, RenderViewHost*> PendingContents;
  PendingContents pending_contents_;

  // These maps hold on to the widgets that we created on behalf of the
  // renderer that haven't shown yet.
  typedef std::map<int, RenderWidgetHostView*> PendingWidgetViews;
  PendingWidgetViews pending_widget_views_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDelegateViewHelper);
};


// Provides helper methods that provide common implementations of some
// RenderViewHostDelegate methods.
class RenderViewHostDelegateHelper {
 public:
  static WebPreferences GetWebkitPrefs(Profile* profile, bool is_dom_ui);

  static void UpdateInspectorSetting(Profile* profile,
                                     const std::string& key,
                                     const std::string& value);
  static void ClearInspectorSettings(Profile* profile);

  static bool gpu_enabled() { return gpu_enabled_; }
  static void set_gpu_enabled(bool enabled) { gpu_enabled_ = enabled; }

 private:
  RenderViewHostDelegateHelper();

  // Master switch for enabling/disabling GPU acceleration for the current
  // browser session. It does not change the acceleration settings for
  // existing tabs, just the future ones.
  static bool gpu_enabled_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDelegateHelper);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_DELEGATE_HELPER_H_
