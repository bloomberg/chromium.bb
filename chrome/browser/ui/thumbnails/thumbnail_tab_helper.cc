// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"

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

ThumbnailTabHelper::ThumbnailTabHelper(content::WebContents* contents)
    : ThumbnailWebContentsObserver(contents),
      view_is_visible_(contents->GetVisibility() ==
                       content::Visibility::VISIBLE) {}

ThumbnailTabHelper::~ThumbnailTabHelper() = default;

void ThumbnailTabHelper::TopLevelNavigationStarted(const GURL& url) {
  UpdateCurrentUrl(url);
}

void ThumbnailTabHelper::TopLevelNavigationEnded(const GURL& url) {
  UpdateCurrentUrl(url);
}

void ThumbnailTabHelper::UpdateCurrentUrl(const GURL& url) {
  current_url_ = url;
  if (current_url_ != thumbnail_url_ &&
      thumbnail_state_ != ThumbnailState::kNoThumbnail) {
    thumbnail_state_ = ThumbnailState::kNoThumbnail;
    thumbnail_ = ThumbnailImage();
    NotifyTabPreviewChanged();
  }
}

void ThumbnailTabHelper::PageLoadStarted(FrameContext frame_context) {
  if (frame_context == FrameContext::kMainFrame)
    is_loading_ = true;
  ScheduleThumbnailCapture(CaptureSchedule::kDelayed);
}

void ThumbnailTabHelper::PageLoadFinished(FrameContext frame_context) {
  if (frame_context == FrameContext::kMainFrame)
    is_loading_ = false;
  ScheduleThumbnailCapture(CaptureSchedule::kAttemptImmediate);
}

void ThumbnailTabHelper::PageUpdated(FrameContext frame_context) {
  ScheduleThumbnailCapture(CaptureSchedule::kAttemptImmediate);
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

  if (schedule != CaptureSchedule::kImmediate && !view_is_visible_)
    return;

  constexpr base::TimeDelta kDelayTime = base::TimeDelta::FromMilliseconds(250);
  constexpr base::TimeDelta kMinTimeBetweenCaptures =
      base::TimeDelta::FromMilliseconds(500);

  // We will do the capture either now or at some point in the future.
  base::TimeDelta delay;
  if (schedule == CaptureSchedule::kDelayed)
    delay += kDelayTime;

  // The time until the next scheduled capture, or the time since the most
  // recent (durations in the past are negative).
  const base::TimeDelta until_scheduled =
      last_scheduled_capture_time_ - base::TimeTicks::Now();

  // If we would schedule a non-immediate capture too close to an existing
  // capture, push it out or discard it altogether.
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
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&ThumbnailTabHelper::StartThumbnailCapture,
                     weak_factory_.GetWeakPtr(), schedule),
      delay);
}

void ThumbnailTabHelper::StartThumbnailCapture(CaptureSchedule schedule) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CaptureInfo capture_info{web_contents()->GetVisibleURL(),
                           is_loading_ ? ThumbnailState::kLoadInProgress
                                       : ThumbnailState::kFinishedLoading};
  DCHECK(!capture_info.url.is_empty());

  if (!view_is_visible_ && schedule != CaptureSchedule::kImmediate)
    return;

  // Destroying a WebContents may trigger it to be hidden, prompting a snapshot
  // which would be unwise to attempt <http://crbug.com/130097>. If the
  // WebContents is in the middle of destruction, do not risk it.
  if (!web_contents() || web_contents()->IsBeingDestroyed())
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(ThumbnailTabHelper)
