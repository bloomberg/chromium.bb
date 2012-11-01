// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
#define CHROME_BROWSER_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_

#include "base/basictypes.h"
#include "chrome/common/thumbnail_score.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;
class Profile;
class SkBitmap;

namespace content {
class RenderViewHost;
class RenderWidgetHost;
}

namespace skia {
class PlatformBitmap;
}

class ThumbnailTabHelper
    : public content::NotificationObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ThumbnailTabHelper> {
 public:
  // The result of clipping. This can be used to determine if the
  // generated thumbnail is good or not.
  enum ClipResult {
    // Clipping is not done yet.
    kUnprocessed,
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

  // Holds the information needed for processing a thumbnail.
  struct ThumbnailingContext : base::RefCountedThreadSafe<ThumbnailingContext> {
    ThumbnailingContext(content::WebContents* web_contents,
                        bool load_interrupted);

    content::BrowserContext* browser_context;
    GURL url;
    ClipResult clip_result;
    ThumbnailScore score;

   private:
    ~ThumbnailingContext();
    friend class base::RefCountedThreadSafe<ThumbnailingContext>;
  };

  virtual ~ThumbnailTabHelper();

  // Enables or disables the function of taking thumbnails.
  // A disabled ThumbnailTabHelper generates no thumbnails although it still
  // continues to receive the notifications from the web contents.
  void set_enabled(bool enabled) { enabled_ = enabled; }

  // Calculates how "boring" a thumbnail is. The boring score is the
  // 0,1 ranged percentage of pixels that are the most common
  // luma. Higher boring scores indicate that a higher percentage of a
  // bitmap are all the same brightness.
  // Statically exposed for use by tests only.
  static double CalculateBoringScore(const SkBitmap& bitmap);

  // Gets the clipped bitmap from |bitmap| per the aspect ratio of the
  // desired width and the desired height. For instance, if the input
  // bitmap is vertically long (ex. 400x900) and the desired size is
  // square (ex. 100x100), the clipped bitmap will be the top half of the
  // input bitmap (400x400).
  // Statically exposed for use by tests only.
  static SkBitmap GetClippedBitmap(const SkBitmap& bitmap,
                                   int desired_width,
                                   int desired_height,
                                   ClipResult* clip_result);

 private:
  explicit ThumbnailTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<ThumbnailTabHelper>;

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // content::WebContentsObserver overrides.
  virtual void DidStartLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void StopNavigation() OVERRIDE;

  // Update the thumbnail of the given tab contents if necessary.
  void UpdateThumbnailIfNecessary(content::WebContents* web_contents);

  // Update the thumbnail of the given tab.
  static void UpdateThumbnail(ThumbnailingContext* context,
                              const SkBitmap& bitmap);

  // Asynchronously updates the thumbnail of the given tab. This must be called
  // on the UI thread.
  void AsyncUpdateThumbnail(content::WebContents* web_contents);

  // Called when the bitmap for generating a thumbnail is ready after the
  // AsyncUpdateThumbnail invocation. This runs on the UI thread.
  static void UpdateThumbnailWithBitmap(ThumbnailingContext* context,
                                        const SkBitmap& bitmap);

  // Called when the canvas for generating a thumbnail is ready after the
  // AsyncUpdateThumbnail invocation. This runs on the UI thread.
  static void UpdateThumbnailWithCanvas(
      ThumbnailingContext* context,
      skia::PlatformBitmap* temp_bitmap,
      bool result);

  // Called when a render view host was created for a WebContents.
  void RenderViewHostCreated(content::RenderViewHost* renderer);

  // Indicates that the given widget has changed is visibility.
  void WidgetHidden(content::RenderWidgetHost* widget);

  // Called when the given render view host was deleted.
  void RenderViewHostDeleted(content::RenderViewHost* renderer);

  bool enabled_;

  content::NotificationRegistrar registrar_;

  bool load_interrupted_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailTabHelper);
};

#endif  // CHROME_BROWSER_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
