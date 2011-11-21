// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
#define CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
#pragma once

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "base/memory/linked_ptr.h"
#include "base/timer.h"
#include "content/browser/renderer_host/backing_store.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class Profile;
class RenderWidgetHost;
class SkBitmap;
class TabContents;

namespace history {
class TopSites;
}

class ThumbnailGenerator : public content::NotificationObserver,
                           public TabContentsObserver {
 public:
  typedef base::Callback<void(const SkBitmap&)> ThumbnailReadyCallback;
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
  virtual ~ThumbnailGenerator();

  // Starts taking thumbnails of the given tab contents.
  void StartThumbnailing(TabContents* tab_contents);

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
                      const ThumbnailReadyCallback& callback,
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

  // Update the thumbnail of the given tab contents if necessary.
  void UpdateThumbnailIfNecessary(TabContents* tab_contents);

  // Returns true if we should update the thumbnail of the given URL.
  static bool ShouldUpdateThumbnail(Profile* profile,
                                    history::TopSites* top_sites,
                                    const GURL& url);

  // TabContentsObserver overrides.
  virtual void DidStartLoading() OVERRIDE;
  virtual void StopNavigation() OVERRIDE;

 private:
  virtual void WidgetDidReceivePaintAtSizeAck(
      RenderWidgetHost* widget,
      int tag,
      const gfx::Size& size);

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Indicates that the given widget has changed is visibility.
  void WidgetHidden(RenderWidgetHost* widget);

  // Called when the given tab contents are disconnected (either
  // through being closed, or because the renderer is no longer there).
  void TabContentsDisconnected(TabContents* contents);

  content::NotificationRegistrar registrar_;

  // Map of callback objects by sequence number.
  struct AsyncRequestInfo;
  typedef std::map<int,
                   linked_ptr<AsyncRequestInfo> > ThumbnailCallbackMap;
  ThumbnailCallbackMap callback_map_;

  bool load_interrupted_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailGenerator);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
