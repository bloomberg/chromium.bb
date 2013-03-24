// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_RENDER_WIDGET_SNAPSHOT_TAKER_H_
#define CHROME_BROWSER_THUMBNAILS_RENDER_WIDGET_SNAPSHOT_TAKER_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class SkBitmap;

namespace content {
class RenderWidgetHost;
}

namespace gfx {
class Size;
}

class RenderWidgetSnapshotTaker : public content::NotificationObserver {
 public:
  typedef base::Callback<void(const SkBitmap&)> SnapshotReadyCallback;

  RenderWidgetSnapshotTaker();
  virtual ~RenderWidgetSnapshotTaker();

  // This registers a callback that can receive the resulting SkBitmap
  // from the renderer when it is done rendering it.  This is asynchronous,
  // and it will also fetch the bitmap even if the tab is hidden.
  // In addition, if the renderer has to be invoked, the scaling of
  // the thumbnail happens on the rendering thread.
  //
  // Takes ownership of the callback object.
  //
  // |page_size| is the size to render the page, and |desired_size| is
  // the size to scale the resulting rendered page to (which is done
  // efficiently if done in the rendering thread). The resulting image
  // will be less then twice the size of the |desired_size| in both
  // dimensions, but might not be the exact size requested.
  void AskForSnapshot(content::RenderWidgetHost* renderer,
                      const SnapshotReadyCallback& callback,
                      gfx::Size page_size,
                      gfx::Size desired_size);

  // Cancel any pending snapshots requested via AskForSnapshot.
  void CancelSnapshot(content::RenderWidgetHost* renderer);

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetSnapshotTakerTest,
                           WidgetDidReceivePaintAtSizeAck);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetSnapshotTakerTest,
                           WidgetDidReceivePaintAtSizeAckFail);

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Start or stop monitoring notifications for |renderer| based on the value
  // of |monitor|.
  void MonitorRenderer(content::RenderWidgetHost* renderer, bool monitor);

  void WidgetDidReceivePaintAtSizeAck(content::RenderWidgetHost* widget,
                                      int tag,
                                      const gfx::Size& size);

  content::NotificationRegistrar registrar_;

  // Map of callback objects by sequence number.
  struct AsyncRequestInfo;
  typedef std::map<int,
                   linked_ptr<AsyncRequestInfo> > SnapshotCallbackMap;
  SnapshotCallbackMap callback_map_;

  // Count of how many outstanding snapshot requests there are for each
  // RenderWidgetHost.
  std::map<content::RenderWidgetHost*, int> host_monitor_counts_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetSnapshotTaker);
};

#endif  // CHROME_BROWSER_THUMBNAILS_RENDER_WIDGET_SNAPSHOT_TAKER_H_
