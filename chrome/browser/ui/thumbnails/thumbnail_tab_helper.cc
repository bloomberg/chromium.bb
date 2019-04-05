// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_tab_helper.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_utils.h"
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

void ThumbnailTabHelper::PageLoadStarted() {
  TransitionLoadingState(LoadingState::kLoadStarted,
                         web_contents()->GetVisibleURL());
}

void ThumbnailTabHelper::PagePainted() {
  page_painted_ = true;
}

void ThumbnailTabHelper::PageLoadFinished() {
  TransitionLoadingState(LoadingState::kLoadFinished,
                         web_contents()->GetVisibleURL());
}

void ThumbnailTabHelper::VisibilityChanged(bool visible) {
  // When the user switches away from a tab, we want to take a snapshot to
  // capture e.g. its scroll position, so that the preview will look like the
  // tab did when the user last visited it.
  const bool was_visible = view_is_visible_;
  view_is_visible_ = visible;
  if (was_visible && !visible) {
    StartThumbnailCapture();
  }
}

void ThumbnailTabHelper::StartThumbnailCapture() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Pages that are unloading do not need to be captured.
  if (adapter_.is_unloading())
    return;

  // Don't capture if in the process of navigating or haven't started yet.
  if (loading_state_ == LoadingState::kNone ||
      loading_state_ == LoadingState::kNavigationStarted) {
    return;
  }

  // Don't capture if there is no current page.
  if (web_contents()->GetVisibleURL().is_empty())
    return;

  // Don't capture pages that have not been loading and visible long enough to
  // actually paint.
  if (!page_painted_)
    return;

  content::RenderWidgetHostView* const source_view =
      web_contents()->GetRenderViewHost()->GetWidget()->GetView();

  // If there's no view or the view isn't available right now, don't bother.
  if (!source_view || !source_view->IsSurfaceAvailableForCopy())
    return;

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
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ThumbnailTabHelper::ProcessCapturedThumbnail(
    base::TimeTicks start_time,
    const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::TimeTicks finish_time = base::TimeTicks::Now();
  const base::TimeDelta copy_from_surface_time = finish_time - start_time;
  UMA_HISTOGRAM_TIMES("Thumbnails.CopyFromSurfaceTime", copy_from_surface_time);

  if (bitmap.drawsNothing())
    return;

  // TODO(dfried): Log capture succeeded.
  ThumbnailImage::FromSkBitmapAsync(
      bitmap, base::BindOnce(&ThumbnailTabHelper::StoreThumbnail,
                             weak_factory_.GetWeakPtr(), finish_time));
}

void ThumbnailTabHelper::StoreThumbnail(base::TimeTicks start_time,
                                        ThumbnailImage thumbnail) {
  DCHECK(thumbnail.HasData());
  const base::TimeTicks finish_time = base::TimeTicks::Now();
  const base::TimeDelta process_time = finish_time - start_time;
  UMA_HISTOGRAM_TIMES("Thumbnails.ProcessBitmapTime", process_time);
  thumbnail_ = thumbnail;

  NotifyTabPreviewChanged();
}

void ThumbnailTabHelper::NotifyTabPreviewChanged() {
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
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
        page_painted_ = false;
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
  thumbnail_ = ThumbnailImage();
  NotifyTabPreviewChanged();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ThumbnailTabHelper)
