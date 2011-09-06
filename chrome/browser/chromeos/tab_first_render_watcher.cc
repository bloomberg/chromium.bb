// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tab_first_render_watcher.h"

#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

namespace chromeos {

TabFirstRenderWatcher::TabFirstRenderWatcher(TabContents* tab,
                                             Delegate* delegate)
    : state_(NONE),
      tab_contents_(tab),
      delegate_(delegate) {
  registrar_.Add(this,
      content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB,
      Source<TabContents>(tab_contents_));
  registrar_.Add(this,
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      Source<TabContents>(tab_contents_));
}

void TabFirstRenderWatcher::Observe(int type,
    const NotificationSource& source, const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDER_VIEW_HOST_CREATED_FOR_TAB: {
      RenderWidgetHost* rwh = Details<RenderWidgetHost>(details).ptr();
      registrar_.Add(this,
          content::NOTIFICATION_RENDER_WIDGET_HOST_DID_PAINT,
          Source<RenderWidgetHost>(rwh));
      delegate_->OnRenderHostCreated(Details<RenderViewHost>(details).ptr());
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

}  // namespace chromeos
