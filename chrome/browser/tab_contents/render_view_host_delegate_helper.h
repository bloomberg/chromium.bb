// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_DELEGATE_HELPER_H_
#define CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_DELEGATE_HELPER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "content/browser/webui/web_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/window_container_type.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/window_open_disposition.h"

class BackgroundContents;
class Profile;
class RenderViewHost;
class RenderViewHostDelegate;
class RenderWidgetHost;
class RenderWidgetHostView;
class SiteInstance;
class TabContents;
struct ViewHostMsg_CreateWindow_Params;

namespace content {
class BrowserContext;
class RenderProcessHost;
}

namespace gfx {
class Rect;
}

// Provides helper methods that provide common implementations of some
// RenderViewHostDelegate::View methods.
class RenderViewHostDelegateViewHelper : public content::NotificationObserver {
 public:
  RenderViewHostDelegateViewHelper();
  virtual ~RenderViewHostDelegateViewHelper();

  // Creates a new RenderWidgetHost and saves it for later retrieval by
  // GetCreatedWidget.
  RenderWidgetHostView* CreateNewWidget(int route_id,
                                        WebKit::WebPopupType popup_type,
                                        content::RenderProcessHost* process);

  RenderWidgetHostView* CreateNewFullscreenWidget(
      int route_id, content::RenderProcessHost* process);

  // Finds the new RenderWidgetHost and returns it. Note that this can only be
  // called once as this call also removes it from the internal map.
  RenderWidgetHostView* GetCreatedWidget(int route_id);

  // Finds the new RenderViewHost/Delegate by route_id, initializes it for
  // for renderer-initiated creation, and returns the TabContents that needs
  // to be shown, if there is one (i.e. not a BackgroundContents). Note that
  // this can only be called once as this call also removes it from the internal
  // map.
  TabContents* GetCreatedWindow(int route_id);

  // These methods are meant to be called from TabContentsView implementations.
  // They take care of notifying the TabContentsDelegate.
  TabContents* CreateNewWindowFromTabContents(
      TabContents* tab_contents,
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params);
  // Mirrors the RenderViewHostDelegate::View Show methods.
  TabContents* ShowCreatedWindow(TabContents* tab_contents,
                                 int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture);
  RenderWidgetHostView* ShowCreatedWidget(TabContents* tab_contents,
                                          int route_id,
                                          const gfx::Rect& initial_pos);
  RenderWidgetHostView* ShowCreatedFullscreenWidget(TabContents* tab_contents,
                                                    int route_id);

 private:
  // content::NotificationObserver implementation
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Creates a new renderer for window.open. This will either be a
  // BackgroundContents (if the window_container_type ==
  // WINDOW_CONTAINER_TYPE_BACKGROUND and permissions allow) or a TabContents.
  // If a TabContents is created, it is returned. Otherwise NULL is returned.
  TabContents* CreateNewWindow(
      int route_id,
      Profile* profile,
      SiteInstance* site,
      WebUI::TypeID webui_type,
      RenderViewHostDelegate* opener,
      WindowContainerType window_container_type,
      const string16& frame_name);

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

  // Registers and unregisters us for notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDelegateViewHelper);
};


// Provides helper methods that provide common implementations of some
// RenderViewHostDelegate methods.
class RenderViewHostDelegateHelper {
 public:
  static WebPreferences GetWebkitPrefs(RenderViewHost* rvh);

  static void UpdateInspectorSetting(content::BrowserContext* browser_context,
                                     const std::string& key,
                                     const std::string& value);
  static void ClearInspectorSettings(content::BrowserContext* browser_context);

 private:
  RenderViewHostDelegateHelper();

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostDelegateHelper);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_RENDER_VIEW_HOST_DELEGATE_HELPER_H_
