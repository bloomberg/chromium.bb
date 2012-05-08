// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
#define CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
#pragma once

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;
class Profile;
class SkBitmap;

namespace content {
class RenderWidgetHost;
}

namespace gfx {
class Size;
}

namespace history {
class TopSites;
}

namespace skia {
class PlatformCanvas;
}

class ThumbnailGenerator : public content::NotificationObserver,
                           public content::WebContentsObserver {
 public:
  typedef base::Callback<void(const SkBitmap&)> ThumbnailReadyCallback;
  // The result of clipping. This can be used to determine if the
  // generated thumbnail is good or not.
  enum ClipResult {
    // The source image is smaller.
    kSourceIsSmaller,
    // Wider than tall by twice or more, clip horizontally.
    kTooWiderThanTall,
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
  void StartThumbnailing(content::WebContents* web_contents);

  // Enables or disables the function of taking thumbnails.
  // A disabled ThumbnailGenerator generates no thumbnails although it still
  // continues to receive the notifications from the web contents.
  void set_enabled(bool enabled) { enabled_ = enabled; }

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
                      const ThumbnailReadyCallback& callback,
                      gfx::Size page_size,
                      gfx::Size desired_size);

  // Start or stop monitoring notifications for |renderer| based on the value
  // of |monitor|.
  void MonitorRenderer(content::RenderWidgetHost* renderer, bool monitor);

  // Calculates how "boring" a thumbnail is. The boring score is the
  // 0,1 ranged percentage of pixels that are the most common
  // luma. Higher boring scores indicate that a higher percentage of a
  // bitmap are all the same brightness.
  static double CalculateBoringScore(const SkBitmap& bitmap);

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
  void UpdateThumbnailIfNecessary(content::WebContents* webb_contents);

  // Update the thumbnail of the given tab.
  void UpdateThumbnail(content::WebContents* web_contents,
                       const SkBitmap& bitmap,
                       const ThumbnailGenerator::ClipResult& clip_result);

  // Returns true if we should update the thumbnail of the given URL.
  static bool ShouldUpdateThumbnail(Profile* profile,
                                    history::TopSites* top_sites,
                                    const GURL& url);

  // content::WebContentsObserver overrides.
  virtual void DidStartLoading() OVERRIDE;
  virtual void StopNavigation() OVERRIDE;

 private:
  virtual void WidgetDidReceivePaintAtSizeAck(
      content::RenderWidgetHost* widget,
      int tag,
      const gfx::Size& size);

  // Asynchronously updates the thumbnail of the given tab. This must be called
  // on the UI thread.
  void AsyncUpdateThumbnail(content::WebContents* web_contents);

  // Called when the bitmap for generating a thumbnail is ready after the
  // AsyncUpdateThumbnail invocation. This runs on the UI thread.
  void AsyncUpdateThumbnailFinish(
      base::WeakPtr<content::WebContents> web_contents,
      skia::PlatformCanvas* temp_canvas,
      bool result);

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Indicates that the given widget has changed is visibility.
  void WidgetHidden(content::RenderWidgetHost* widget);

  // Called when the given web contents are disconnected (either
  // through being closed, or because the renderer is no longer there).
  void WebContentsDisconnected(content::WebContents* contents);

  bool enabled_;
  content::NotificationRegistrar registrar_;

  // Map of callback objects by sequence number.
  struct AsyncRequestInfo;
  typedef std::map<int,
                   linked_ptr<AsyncRequestInfo> > ThumbnailCallbackMap;
  ThumbnailCallbackMap callback_map_;

  bool load_interrupted_;

  base::WeakPtrFactory<ThumbnailGenerator> weak_factory_;
  scoped_ptr<base::WeakPtrFactory<content::WebContents> >
      web_contents_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailGenerator);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_THUMBNAIL_GENERATOR_H_
