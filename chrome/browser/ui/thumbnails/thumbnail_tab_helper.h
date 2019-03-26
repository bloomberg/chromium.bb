// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/thumbnails/thumbnail_page_event_adapter.h"
#include "chrome/browser/ui/thumbnails/thumbnail_page_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class ThumbnailTabHelper
    : public thumbnails::ThumbnailPageObserver,
      public content::WebContentsUserData<ThumbnailTabHelper> {
 public:
  enum class ThumbnailState {
    kNoThumbnail,     // no thumbnail is available
    kLoadInProgress,  // thumbnail available but is of a page that is loading
    kFinishedLoading  // thumbnail should represent the finished page
  };

  ~ThumbnailTabHelper() override;

  ThumbnailState thumbnail_state() const { return thumbnail_state_; }
  ThumbnailImage thumbnail() const { return thumbnail_; }

 protected:
  // ThumbnailWebContentsObserver:
  void TopLevelNavigationStarted(const GURL& url) override;
  void TopLevelNavigationEnded(const GURL& url) override;
  void PageLoadStarted(thumbnails::FrameContext frame_context) override;
  void PageLoadFinished(thumbnails::FrameContext frame_context) override;
  void PageUpdated(thumbnails::FrameContext frame_context) override;
  void VisibilityChanged(bool visible) override;

  content::WebContents* web_contents() const { return adapter_.web_contents(); }

 private:
  enum class CaptureSchedule { kImmediate, kDelayed };

  // Loading is treated as a state machine for each new URL, and the state for
  // each new URL the current tab loads can only advance as events are received
  // (so it is not possible to go from kLoadStarted to kNavigationStarted).
  enum class LoadingState : int32_t {
    kNone = 0,
    kNavigationStarted = 1,
    kNavigationFinished = 2,
    kLoadStarted = 3,
    kLoadFinished = 4
  };

  struct CaptureInfo {
    GURL url;
    ThumbnailState target_state;
  };

  explicit ThumbnailTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<ThumbnailTabHelper>;

  void ScheduleThumbnailCapture(CaptureSchedule schedule);
  void StartThumbnailCapture(CaptureSchedule schedule);
  void ProcessCapturedThumbnail(const CaptureInfo& capture_info,
                                base::TimeTicks start_time,
                                const SkBitmap& bitmap);
  void StoreThumbnail(const CaptureInfo& capture_info,
                      base::TimeTicks start_time,
                      ThumbnailImage thumbnail);
  void NotifyTabPreviewChanged();

  // For tabs in the process of loading, schedules another capture if none is
  // currently queued.
  void MaybeScheduleAnotherCapture(const CaptureInfo& capture_info,
                                   base::TimeTicks finish_time);

  // Returns whether a capture can happen at the current time.
  bool CanCaptureThumbnail(CaptureSchedule schedule) const;

  ThumbnailState GetThumbnailState() const;
  void TransitionLoadingState(LoadingState state, const GURL& url);
  void ClearThumbnail();

  ThumbnailImage thumbnail_;
  ThumbnailState thumbnail_state_ = ThumbnailState::kNoThumbnail;
  GURL thumbnail_url_;

  // Caches whether or not the web contents view is visible. See notes in
  // VisibilityChanged() for more information.
  bool view_is_visible_;  // set in constructor
  LoadingState loading_state_ = LoadingState::kNone;
  GURL current_url_;

  // The time that the most recently-scheduled capture is/was scheduled for.
  // Can be in the past. Used to prevent captures from bunching up or being
  // scheduled in the wrong order.
  base::TimeTicks last_scheduled_capture_time_;

  thumbnails::ThumbnailPageEventAdapter adapter_;
  ScopedObserver<thumbnails::ThumbnailPageEventAdapter,
                 thumbnails::ThumbnailPageObserver>
      scoped_observer_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<ThumbnailTabHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ThumbnailTabHelper);
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_TAB_HELPER_H_
