// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
#define CHROME_BROWSER_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/page_transition_types.h"

namespace content {
class NavigationHandle;
class RenderViewHost;
class RenderWidgetHost;
}

class ThumbnailTabHelper
    : public content::NotificationObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ThumbnailTabHelper> {
 public:
  ~ThumbnailTabHelper() override;

 private:
  explicit ThumbnailTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<ThumbnailTabHelper>;

  // content::NotificationObserver overrides.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::WebContentsObserver overrides.
  void RenderViewDeleted(content::RenderViewHost* render_view_host) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStartLoading() override;
  void NavigationStopped() override;

  // Update the thumbnail of the given tab contents if necessary.
  void UpdateThumbnailIfNecessary();

  // Initiate asynchronous generation of a thumbnail from the web contents.
  void AsyncProcessThumbnail(
      scoped_refptr<thumbnails::ThumbnailService> thumbnail_service);

  // Create a thumbnail from the web contents bitmap.
  void ProcessCapturedBitmap(
      const SkBitmap& bitmap,
      content::ReadbackResponse response);

  // Pass the thumbnail to the thumbnail service.
  void UpdateThumbnail(const SkBitmap& thumbnail);

  // Clean up after thumbnail generation has ended.
  void CleanUpFromThumbnailGeneration();

  // Called when a render view host was created for a WebContents.
  void RenderViewHostCreated(content::RenderViewHost* renderer);

  // Indicates that the given widget has changed is visibility.
  void WidgetHidden(content::RenderWidgetHost* widget);

  content::NotificationRegistrar registrar_;

  ui::PageTransition page_transition_;
  bool load_interrupted_;

  scoped_refptr<thumbnails::ThumbnailingContext> thumbnailing_context_;

  base::TimeTicks copy_from_surface_start_time_;

  base::WeakPtrFactory<ThumbnailTabHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailTabHelper);
};

#endif  // CHROME_BROWSER_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
