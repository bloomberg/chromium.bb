// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_READINESS_TRACKER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_READINESS_TRACKER_H_

#include "base/callback.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

// Determines how ready a page is for thumbnail capture based on
// navigation and loading state.
class ThumbnailReadinessTracker : public content::WebContentsObserver {
 public:
  enum class Readiness : int {
    // The WebContents is not ready for capturing.
    kNotReady = 0,
    // Thumbnails can be captured, but the page might change. Don't use
    // any captured frames as the final thumbnail.
    kReadyForInitialCapture,
    // A thumbnail can be captured that should be representative of the
    // page's final state. For good measure, the client should still
    // wait a few seconds to capture the final thumbnail since dynamic
    // elements might not be in final position yet.
    kReadyForFinalCapture,
  };

  using ReadinessChangeCallback = base::RepeatingCallback<void(Readiness)>;

  // |web_contents| should be a newly-created contents. If not, the
  // output readiness states will not be correct. |callback| will be
  // called with the new ready state whenever it changes.
  ThumbnailReadinessTracker(content::WebContents* web_contents,
                            ReadinessChangeCallback callback);
  ~ThumbnailReadinessTracker() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void WebContentsDestroyed() override;

 private:
  void UpdateReadiness(Readiness readiness);

  ReadinessChangeCallback callback_;
  Readiness last_readiness_ = Readiness::kNotReady;
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_READINESS_TRACKER_H_
