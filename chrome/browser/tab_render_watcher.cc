// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_render_watcher.h"

#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::RenderWidgetHost;
using content::WebContents;

TabRenderWatcher::TabRenderWatcher(WebContents* tab, Delegate* delegate)
    : loaded_(false),
      web_contents_(tab),
      delegate_(delegate) {
  registrar_.Add(this,
      content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
      content::Source<WebContents>(web_contents_));
  registrar_.Add(this,
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::Source<WebContents>(web_contents_));
}

void TabRenderWatcher::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      RenderWidgetHost* rwh = content::Details<RenderWidgetHost>(details).ptr();
      registrar_.Add(this,
          content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
          content::Source<RenderWidgetHost>(rwh));
      delegate_->OnRenderHostCreated(
          content::Details<content::RenderViewHost>(details).ptr());
      break;
    }
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME:
      if (!loaded_) {
        loaded_ = true;
        delegate_->OnTabMainFrameLoaded();
      }
      break;
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE:
      if (loaded_)
        delegate_->OnTabMainFrameRender();
      break;
    default:
      NOTREACHED() << "Unknown notification " << type;
  }
}
