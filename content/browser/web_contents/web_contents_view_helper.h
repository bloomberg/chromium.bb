// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_HELPER_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_HELPER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "webkit/glue/window_open_disposition.h"

class WebContentsImpl;
struct ViewHostMsg_CreateWindow_Params;

namespace content {
class RenderWidgetHostView;
}

namespace gfx {
class Rect;
}

// Provides helper methods that provide common implementations of some
// WebContentsView methods.
class CONTENT_EXPORT WebContentsViewHelper
    : public content::NotificationObserver {
 public:
  WebContentsViewHelper();
  virtual ~WebContentsViewHelper();

  // Creates a new window; call |ShowCreatedWindow| below to show it.
  WebContentsImpl* CreateNewWindow(
      WebContentsImpl* opener,
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params);

  // Creates a new popup or fullscreen widget; call |ShowCreatedWidget| below to
  // show it. If |is_fullscreen| is true it is a fullscreen widget, if not then
  // a pop-up. |popup_type| is only meaningful for a pop-up.
  content::RenderWidgetHostView* CreateNewWidget(
      WebContentsImpl* web_contents,
      int route_id,
      bool is_fullscreen,
      WebKit::WebPopupType popup_type);

  // Shows a window created with |CreateNewWindow| above.
  WebContentsImpl* ShowCreatedWindow(WebContentsImpl* web_contents,
                                     int route_id,
                                     WindowOpenDisposition disposition,
                                     const gfx::Rect& initial_pos,
                                     bool user_gesture);

  // Shows a widget created with |CreateNewWidget| above. |initial_pos| is only
  // meaningful for non-fullscreen widgets.
  content::RenderWidgetHostView* ShowCreatedWidget(
      WebContentsImpl* web_contents,
      int route_id,
      bool is_fullscreen,
      const gfx::Rect& initial_pos);

 private:
  // content::NotificationObserver implementation
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Finds the new RenderWidgetHost and returns it. Note that this can only be
  // called once as this call also removes it from the internal map.
  content::RenderWidgetHostView* GetCreatedWidget(int route_id);

  // Finds the new WebContentsImpl by route_id, initializes it for
  // renderer-initiated creation, and returns it. Note that this can only be
  // called once as this call also removes it from the internal map.
  WebContentsImpl* GetCreatedWindow(int route_id);

  // Tracks created WebContentsImpl objects that have not been shown yet. They
  // are identified by the route ID passed to CreateNewWindow.
  typedef std::map<int, WebContentsImpl*> PendingContents;
  PendingContents pending_contents_;

  // These maps hold on to the widgets that we created on behalf of the renderer
  // that haven't shown yet.
  typedef std::map<int, content::RenderWidgetHostView*> PendingWidgetViews;
  PendingWidgetViews pending_widget_views_;

  // Registers and unregisters us for notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewHelper);
};

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_HELPER_H_
