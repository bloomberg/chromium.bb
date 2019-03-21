// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_WEB_CONTENTS_OBSERVER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

// Base class for thumbnail tab helper; processes specific web contents events
// into a filtered-down set of navigation and loading events for ease of
// processing.
class ThumbnailWebContentsObserver : public content::WebContentsObserver {
 public:
  ~ThumbnailWebContentsObserver() override;

 protected:
  enum class FrameContext { kMainFrame, kChildFrame };

  explicit ThumbnailWebContentsObserver(content::WebContents* contents);

  // Called when navigation in the top-level browser window starts.
  virtual void TopLevelNavigationStarted(const GURL& url) = 0;
  // Called when navigation in the top-level browser window completes.
  virtual void TopLevelNavigationEnded(const GURL& url) = 0;
  // Called when the page/tab's visibility changes.
  // If |view_is_valid| is false, no attempt should be made to read from the
  // contents pane.
  virtual void VisibilityChanged(bool visible) = 0;
  // Called when a page begins to load.
  virtual void PageLoadStarted(FrameContext frame_context) = 0;
  // Called when a page finishes loading.
  virtual void PageLoadFinished(FrameContext frame_context) = 0;
  // Called when the page is resized or otherwise updated.
  virtual void PageUpdated(FrameContext frame_context) = 0;

  void OnVisibilityChanged(content::Visibility visibility) override;

  // Track when navigation happens so that we know when a thumbnail is no longer
  // valid (thumbnail will still be valid for the old URL until the new
  // navigation has committed and the new page render occurs).
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Track the progress of loading. Thumbnails should be captured during the
  // loading process, since some pages take a long time to load, but there is no
  // point to capturing a thumbnail of a page that has not rendered anything
  // yet.
  void DidStartLoading() override;
  void DidStopLoading() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;
  void NavigationStopped() override;
  void MainFrameWasResized(bool width_changed) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;

 private:
  static FrameContext ContextFromRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  DISALLOW_COPY_AND_ASSIGN(ThumbnailWebContentsObserver);
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_WEB_CONTENTS_OBSERVER_H_
