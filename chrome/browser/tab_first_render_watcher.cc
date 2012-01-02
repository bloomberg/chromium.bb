// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_first_render_watcher.h"

#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

TabFirstRenderWatcher::TabFirstRenderWatcher(WebContents* tab,
                                             Delegate* delegate)
    : state_(NONE),
      web_contents_(tab),
      delegate_(delegate) {
  registrar_.Add(this,
      content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
      content::Source<WebContents>(web_contents_));
  registrar_.Add(this,
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::Source<WebContents>(web_contents_));
}

void TabFirstRenderWatcher::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      RenderWidgetHost* rwh = content::Details<RenderWidgetHost>(details).ptr();
      registrar_.Add(this,
          content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT,
          content::Source<RenderWidgetHost>(rwh));
      delegate_->OnRenderHostCreated(
          content::Details<RenderViewHost>(details).ptr());
      break;
    }
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME:
      if (state_ == NONE) {
        state_ = LOADED;
        delegate_->OnTabMainFrameLoaded();
      }
      break;
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT:
      if (state_ == LOADED) {
        state_ = FIRST_PAINT;
        delegate_->OnTabMainFrameFirstRender();
      }
      break;
    default:
      NOTREACHED() << "unknown type" << type;
  }
}
