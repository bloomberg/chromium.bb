// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
#define CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
#pragma once

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/linked_ptr.h"
#include "base/timer.h"
#include "chrome/browser/renderer_host/backing_store.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class GURL;
class Profile;
class RenderWidgetHost;
class SkBitmap;
class TabContents;

namespace history {
class TopSites;
}

class ThumbnailGenerator : NotificationObserver {
 public:
  typedef Callback1<const SkBitmap&>::Type ThumbnailReadyCallback;
  // The result of clipping. This can be used to determine if the
  // generated thumbnail is good or not.
  enum ClipResult {
    // The source image is smaller.
    kSourceIsSmaller,
    // Wider than tall, clip horizontally.
    kWiderThanTall,
    // Taller than wide, clip vertically.
    kTallerThanWide,
    // The source and destination aspect ratios are identical.
    kNotClipped,
  };

  // Bitmasks of options for generating a thumbnail.
  enum ThumbnailOptions {
    // No options.
    kNoOptions = 0,
    // Request a clipped thumbnail with the aspect ratio preserved.
    kClippedThumbnail = 1 << 0,
  };

  // This class will do nothing until you call StartThumbnailing.
  ThumbnailGenerator();
  ~ThumbnailGenerator();

  // Ensures that we're properly hooked in to generated thumbnails. This can
  // be called repeatedly and with wild abandon to no ill effect.
  void StartThumbnailing();

  // This registers a callback that can receive the resulting SkBitmap
  // from the renderer when it is done rendering it.  This differs
  // from GetThumbnailForRenderer in that it may be asynchronous, and
  // because it will also fetch the bitmap even if the tab is hidden.
  // In addition, if the renderer has to be invoked, the scaling of
  // the thumbnail happens on the rendering thread.
  //
  // Takes ownership of the callback object.
  //
  // If |prefer_backing_store| is set, then the function will try and
  // use the backing store for the page if it exists.  |page_size| is
  // the size to render the page, and |desired_size| is the size to
  // scale the resulting rendered page to (which is done efficiently
  // if done in the rendering thread).  If |prefer_backing_store| is
  // set, and the backing store is used, then the resulting image will
  // be less then twice the size of the |desired_size| in both
  // dimensions, but might not be the exact size requested.
  void AskForSnapshot(RenderWidgetHost* renderer,
                      bool prefer_backing_store,
                      ThumbnailReadyCallback* callback,
                      gfx::Size page_size,
                      gfx::Size desired_size);

  // This returns a thumbnail of a fixed, small size for the given
  // renderer.
  SkBitmap GetThumbnailForRenderer(RenderWidgetHost* renderer) const;

  // This returns a thumbnail of a fixed, small size for the given
  // renderer. |options| is a bitmask of ThumbnailOptions. If
  // |clip_result| is non-NULL, the result of clipping will be written.
  SkBitmap GetThumbnailForRendererWithOptions(RenderWidgetHost* renderer,
                                              int options,
                                              ClipResult* clip_result) const;

  // Start or stop monitoring notifications for |renderer| based on the value
  // of |monitor|.
  void MonitorRenderer(RenderWidgetHost* renderer, bool monitor);

#ifdef UNIT_TEST
  // When true, the class will not use a timeout to do the expiration. This
  // will cause expiration to happen on the next run of the message loop.
  // Unit tests case use this to test expiration by choosing when the message
  // loop runs.
  void set_no_timeout(bool no_timeout) { no_timeout_ = no_timeout; }
#endif

  // Calculates how "boring" a thumbnail is. The boring score is the
  // 0,1 ranged percentage of pixels that are the most common
  // luma. Higher boring scores indicate that a higher percentage of a
  // bitmap are all the same brightness.
  static double CalculateBoringScore(SkBitmap* bitmap);

  // Gets the clipped bitmap from |bitmap| per the aspect ratio of the
  // desired width and the desired height. For instance, if the input
  // bitmap is vertically long (ex. 400x900) and the desired size is
  // square (ex. 100x100), the clipped bitmap will be the top half of the
  // input bitmap (400x400).
  static SkBitmap GetClippedBitmap(const SkBitmap& bitmap,
                                   int desired_width,
                                   int desired_height,
                                   ClipResult* clip_result);

  // Update the thumbnail of the given URL if necessary.
  static void UpdateThumbnailIfNecessary(TabContents* tab_contents,
                                         const GURL& url);

  // Returns true if we should update the thumbnail of the given URL.
  static bool ShouldUpdateThumbnail(Profile* profile,
                                    history::TopSites* top_sites,
                                    const GURL& url);

 private:
  // RenderWidgetHostPaintingObserver implementation.
  virtual void WidgetWillDestroyBackingStore(RenderWidgetHost* widget,
                                             BackingStore* backing_store);
  virtual void WidgetDidUpdateBackingStore(RenderWidgetHost* widget);

  virtual void WidgetDidReceivePaintAtSizeAck(
      RenderWidgetHost* widget,
      int tag,
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

  // Map of callback objects by sequence number.
  struct AsyncRequestInfo;
  typedef std::map<int,
                   linked_ptr<AsyncRequestInfo> > ThumbnailCallbackMap;
  ThumbnailCallbackMap callback_map_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailGenerator);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
