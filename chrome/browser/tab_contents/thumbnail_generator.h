// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
#define CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/linked_ptr.h"
#include "base/lock.h"
#include "base/timer.h"
#include "chrome/browser/renderer_host/render_widget_host_painting_observer.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class RenderViewHost;
class RenderWidgetHost;
class SkBitmap;
class TabContents;

// This class MUST be destroyed after the RenderWidgetHosts, since it installs
// a painting observer that is not removed.
class ThumbnailGenerator : public RenderWidgetHostPaintingObserver,
                           public NotificationObserver {
 public:
  typedef Callback1<const SkBitmap&>::Type ThumbnailReadyCallback;
  // This class will do nothing until you call StartThumbnailing.
  ThumbnailGenerator();
  ~ThumbnailGenerator();

  // Ensures that we're properly hooked in to generated thumbnails. This can
  // be called repeatedly and with wild abandon to no ill effect.
  void StartThumbnailing();

  // This registers a callback that can receive the resulting SkBitmap
  // from the renderer when it is done rendering it.  This differs
  // from GetThumbnailForRenderer in that it is asynchronous, and
  // because it will also fetch the bitmap even if the tab is hidden.
  // In addition, if the renderer has to be invoked, the scaling of
  // the thumbnail happens on the rendering thread.  Takes ownership
  // of the callback object.
  void AskForThumbnail(RenderWidgetHost* renderer,
                       ThumbnailReadyCallback* callback,
                       gfx::Size size);

  SkBitmap GetThumbnailForRenderer(RenderWidgetHost* renderer) const;

#ifdef UNIT_TEST
  // When true, the class will not use a timeout to do the expiration. This
  // will cause expiration to happen on the next run of the message loop.
  // Unit tests case use this to test expiration by choosing when the message
  // loop runs.
  void set_no_timeout(bool no_timeout) { no_timeout_ = no_timeout; }
#endif

 private:
  // RenderWidgetHostPaintingObserver implementation.
  virtual void WidgetWillDestroyBackingStore(RenderWidgetHost* widget,
                                             BackingStore* backing_store);
  virtual void WidgetDidUpdateBackingStore(RenderWidgetHost* widget);

  virtual void WidgetDidReceivePaintAtSizeAck(
      RenderWidgetHost* widget,
      const TransportDIB::Handle& dib_handle,
      const gfx::Size& size);

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Indicates that the given widget has changed is visibility.
  void WidgetShown(RenderWidgetHost* widget);
  void WidgetHidden(RenderWidgetHost* widget);

  // Called when the given widget is destroyed.
  void WidgetDestroyed(RenderWidgetHost* widget);

  // Called when the given tab contents are disconnected (either
  // through being closed, or because the renderer is no longer there).
  void TabContentsDisconnected(TabContents* contents);

  // Timer function called on a delay after a tab has been shown. It will
  // invalidate the thumbnail for hosts with expired thumbnails in shown_hosts_.
  void ShownDelayHandler();

  // Removes the given host from the shown_hosts_ list, if it is there.
  void EraseHostFromShownList(RenderWidgetHost* host);

  NotificationRegistrar registrar_;

  base::OneShotTimer<ThumbnailGenerator> timer_;

  // A list of all RWHs that have been shown and need to have their thumbnail
  // expired at some time in the future with the "slop" time has elapsed. This
  // list will normally have 0 or 1 items in it.
  std::vector<RenderWidgetHost*> shown_hosts_;

  // See the setter above.
  bool no_timeout_;

  // Map of callback objects by TransportDIB::Handle.
  struct AsyncRequestInfo;
  typedef std::map<TransportDIB::Handle,
                   linked_ptr<AsyncRequestInfo> > ThumbnailCallbackMap;
  ThumbnailCallbackMap callback_map_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailGenerator);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
