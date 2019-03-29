// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/scrollbar_size.h"

namespace {

// Determine if two URLs are similar enough that we should not blank the preview
// image when transitioning between them. This prevents webapps which do all of
// their work through internal page transitions (and which can transition after
// the page is loaded) from blanking randomly.
bool AreSimilarURLs(const GURL& url1, const GURL& url2) {
  const GURL origin1 = url1.GetOrigin();

  // For non-standard URLs, compare using normal logic.
  if (origin1.is_empty())
    return url1.EqualsIgnoringRef(url2);

  // TODO(dfried): make this logic a little smarter; maybe compare the first
  // element of the path as well?
  return origin1 == url2.GetOrigin();
}

}  // namespace

ThumbnailTabHelper::ThumbnailTabHelper(content::WebContents* contents)
    : view_is_visible_(contents->GetVisibility() ==
                       content::Visibility::VISIBLE),
      adapter_(contents),
      scoped_observer_(this) {
  scoped_observer_.Add(&adapter_);
}

ThumbnailTabHelper::~ThumbnailTabHelper() = default;

void ThumbnailTabHelper::TopLevelNavigationStarted(const GURL& url) {
  TransitionLoadingState(LoadingState::kNavigationStarted, url);
}

void ThumbnailTabHelper::TopLevelNavigationEnded(const GURL& url) {
  TransitionLoadingState(LoadingState::kNavigationFinished, url);
}

void ThumbnailTabHelper::PageLoadStarted(
    thumbnails::FrameContext frame_context) {
  if (frame_context == thumbnails::FrameContext::kMainFrame) {
    TransitionLoadingState(LoadingState::kLoadStarted,
                           web_contents()->GetVisibleURL());
  }
  ScheduleThumbnailCapture(CaptureSchedule::kDelayed);
}

void ThumbnailTabHelper::PageLoadFinished(
    thumbnails::FrameContext frame_context) {
  if (frame_context == thumbnails::FrameContext::kMainFrame) {
    TransitionLoadingState(LoadingState::kLoadFinished,
                           web_contents()->GetVisibleURL());
  }
  ScheduleThumbnailCapture(CaptureSchedule::kDelayed);
}

void ThumbnailTabHelper::PageUpdated(thumbnails::FrameContext frame_context) {
  ScheduleThumbnailCapture(CaptureSchedule::kDelayed);
}

void ThumbnailTabHelper::VisibilityChanged(bool visible) {
  // When the visibility of the current tab changes (most importantly, when the
  // user is switching away from the current tab) we want to capture a snapshot
  // of the tab to capture e.g. its scroll position, so that the preview will
  // look like the tab did when the user last switched to/from it.
  const bool was_visible = view_is_visible_;
  view_is_visible_ = visible;
  if (was_visible != visible) {
    // Because it can take a moment for tabs to re-render, use a delay when
    // returning to a tab, but capture immediately when switching away.
    const CaptureSchedule schedule =
        visible ? CaptureSchedule::kDelayed : CaptureSchedule::kImmediate;
    ScheduleThumbnailCapture(schedule);
  }
}

void ThumbnailTabHelper::ScheduleThumbnailCapture(CaptureSchedule schedule) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!CanCaptureThumbnail(schedule))
    return;

  // We will do the capture either now or at some point in the future.
  constexpr base::TimeDelta kDelayTime = base::TimeDelta::FromMilliseconds(500);
  base::TimeDelta delay;
  if (schedule == CaptureSchedule::kDelayed)
    delay += kDelayTime;

  // The time until the next scheduled capture, or the time since the most
  // recent (durations in the past are negative).
  const base::TimeDelta until_scheduled =
      last_scheduled_capture_time_ - base::TimeTicks::Now();

  // If we would schedule a non-immediate capture too close to an existing
  // capture, push it out or discard it altogether.
  constexpr base::TimeDelta kMinTimeBetweenCaptures =
      base::TimeDelta::FromMilliseconds(2000);
  if (schedule != CaptureSchedule::kImmediate &&
      delay - until_scheduled < kMinTimeBetweenCaptures) {
    if (until_scheduled > delay)
      return;
    delay = until_scheduled + kMinTimeBetweenCaptures;
  }

  last_scheduled_capture_time_ = base::TimeTicks::Now() + delay;

  if (delay.is_zero()) {
    StartThumbnailCapture(schedule);
    return;
  }

  base::PostDelayedTaskWithTraits(
      FROM_HERE,
      {content::BrowserThread::UI, base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailTabHelper::StartThumbnailCapture,
                     weak_factory_.GetWeakPtr(), schedule),
      delay);
}

void ThumbnailTabHelper::StartThumbnailCapture(CaptureSchedule schedule) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!CanCaptureThumbnail(schedule))
    return;

  CaptureInfo capture_info{web_contents()->GetVisibleURL(),
                           GetThumbnailState()};

  // Don't try to capture thumbnails if we're in the navigation phase.
  if (capture_info.target_state == ThumbnailState::kNoThumbnail)
    return;

  // If there's no currently-visible URL, don't capture.
  if (capture_info.url.is_empty())
    return;

  base::TimeTicks start_time = base::TimeTicks::Now();

  content::RenderWidgetHostView* const source_view =
      web_contents()->GetRenderViewHost()->GetWidget()->GetView();

  // If there's no view or the view isn't available right now, put off
  // capturing.
  if (!source_view || !source_view->IsSurfaceAvailableForCopy()) {
    ScheduleThumbnailCapture(CaptureSchedule::kDelayed);
    return;
  }

  // Note: this is the size in pixels on-screen, not the size in DIPs.
  gfx::Size source_size = source_view->GetViewBounds().size();
  // Clip the pixels that will commonly hold a scrollbar, which looks bad in
  // thumbnails.
  const float scale_factor = source_view->GetDeviceScaleFactor();
  const int scrollbar_size = gfx::scrollbar_size() * scale_factor;
  source_size.Enlarge(-scrollbar_size, -scrollbar_size);

  if (source_size.IsEmpty())
    return;

  const gfx::Size desired_size = TabStyle::GetPreviewImageSize();
  thumbnails::CanvasCopyInfo copy_info =
      thumbnails::GetCanvasCopyInfo(source_size, scale_factor, desired_size);

  source_view->CopyFromSurface(
      copy_info.copy_rect, copy_info.target_size,
      base::BindOnce(&ThumbnailTabHelper::ProcessCapturedThumbnail,
                     weak_factory_.GetWeakPtr(), capture_info, start_time));
}

void ThumbnailTabHelper::ProcessCapturedThumbnail(
    const CaptureInfo& capture_info,
    base::TimeTicks start_time,
    const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(!capture_info.url.is_empty());
  DCHECK(capture_info.target_state != ThumbnailState::kNoThumbnail);

  const base::TimeTicks finish_time = base::TimeTicks::Now();
  const base::TimeDelta copy_from_surface_time = finish_time - start_time;
  UMA_HISTOGRAM_TIMES("Thumbnails.CopyFromSurfaceTime", copy_from_surface_time);

  if (bitmap.drawsNothing()) {
    // TODO(dfried): Log capture failed.
    MaybeScheduleAnotherCapture(capture_info, finish_time);
    return;
  }

  // TODO(dfried): Log capture succeeded.
  ThumbnailImage::FromSkBitmapAsync(
      bitmap,
      base::BindOnce(&ThumbnailTabHelper::StoreThumbnail,
                     weak_factory_.GetWeakPtr(), capture_info, finish_time));
}

void ThumbnailTabHelper::StoreThumbnail(const CaptureInfo& capture_info,
                                        base::TimeTicks start_time,
                                        ThumbnailImage thumbnail) {
  DCHECK(thumbnail.HasData());
  const base::TimeTicks finish_time = base::TimeTicks::Now();
  const base::TimeDelta process_time = finish_time - start_time;
  UMA_HISTOGRAM_TIMES("Thumbnails.ProcessBitmapTime", process_time);
  thumbnail_state_ = capture_info.target_state;
  thumbnail_url_ = capture_info.url;
  thumbnail_ = thumbnail;

  NotifyTabPreviewChanged();
  MaybeScheduleAnotherCapture(capture_info, finish_time);
}

void ThumbnailTabHelper::NotifyTabPreviewChanged() {
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
}

void ThumbnailTabHelper::MaybeScheduleAnotherCapture(
    const CaptureInfo& capture_info,
    base::TimeTicks finish_time) {
  // If the page is still loading, schedule another capture a short time later.
  if (capture_info.target_state != ThumbnailState::kFinishedLoading &&
      finish_time > last_scheduled_capture_time_) {
    ScheduleThumbnailCapture(CaptureSchedule::kDelayed);
  }
}

bool ThumbnailTabHelper::CanCaptureThumbnail(CaptureSchedule schedule) const {
  if (adapter_.is_unloading())
    return false;

  // Destroying a WebContents may trigger it to be hidden, prompting a snapshot
  // which would be unwise to attempt <http://crbug.com/130097>. If the
  // WebContents is in the middle of destruction, do not risk it.
  if (!web_contents() || web_contents()->IsBeingDestroyed())
    return false;

  // Windows which are not being shown often have no or invalid data, so don't
  // try to capture them. The exception is snapshots that happen on visibility
  // transitions, which have the |kImmediate| schedule.
  switch (schedule) {
    case CaptureSchedule::kImmediate:
      return true;
    default:
      return view_is_visible_;
  }
}

ThumbnailTabHelper::ThumbnailState ThumbnailTabHelper::GetThumbnailState()
    const {
  switch (loading_state_) {
    case LoadingState::kNavigationFinished:
    case LoadingState::kLoadStarted:
      return ThumbnailState::kLoadInProgress;
    case LoadingState::kLoadFinished:
      return ThumbnailState::kFinishedLoading;
    default:
      return ThumbnailState::kNoThumbnail;
  }
}

void ThumbnailTabHelper::TransitionLoadingState(LoadingState state,
                                                const GURL& url) {
  // Because the loading process is unpredictable, and because there are a large
  // number of events which could be interpreted as navigation of the main frame
  // or loading, only move the loading progress forward.
  const bool is_similar_url = AreSimilarURLs(url, current_url_);
  switch (state) {
    case LoadingState::kNavigationStarted:
    case LoadingState::kNavigationFinished:
      if (!is_similar_url) {
        current_url_ = url;
        ClearThumbnail();
        loading_state_ = state;
      } else {
        loading_state_ = std::max(loading_state_, state);
      }
      break;
    case LoadingState::kLoadStarted:
    case LoadingState::kLoadFinished:
      if (!is_similar_url &&
          (loading_state_ == LoadingState::kNavigationStarted ||
           loading_state_ == LoadingState::kNavigationFinished)) {
        // This probably refers to an old page, so ignore it.
        return;
      }
      current_url_ = url;
      loading_state_ = std::max(loading_state_, state);
      break;
    case LoadingState::kNone:
      NOTREACHED();
      break;
  }
}

void ThumbnailTabHelper::ClearThumbnail() {
  if (!thumbnail_.HasData())
    return;
  thumbnail_state_ = ThumbnailState::kNoThumbnail;
  thumbnail_ = ThumbnailImage();
  NotifyTabPreviewChanged();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ThumbnailTabHelper)
