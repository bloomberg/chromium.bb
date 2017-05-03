// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/content_based_thumbnailing_algorithm.h"

#include <stddef.h>

#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/thumbnails/content_analysis.h"
#include "chrome/browser/thumbnails/simple_thumbnail_crop.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"

namespace {

const char kThumbnailHistogramName[] = "Thumbnail.RetargetMS";
const char kFailureHistogramName[] = "Thumbnail.FailedRetargetMS";
const float kScoreBoostFromSuccessfulRetargeting = 1.1f;

void CallbackInvocationAdapter(
    const thumbnails::ThumbnailingAlgorithm::ConsumerCallback& callback,
    scoped_refptr<thumbnails::ThumbnailingContext> context,
    const SkBitmap& source_bitmap) {
  callback.Run(*context.get(), source_bitmap);
}

}  // namespace

namespace thumbnails {

using content::BrowserThread;

ContentBasedThumbnailingAlgorithm::ContentBasedThumbnailingAlgorithm(
    const gfx::Size& target_size)
    : target_size_(target_size) {
  DCHECK(!target_size.IsEmpty());
}

ClipResult ContentBasedThumbnailingAlgorithm::GetCanvasCopyInfo(
    const gfx::Size& source_size,
    ui::ScaleFactor scale_factor,
    gfx::Rect* clipping_rect,
    gfx::Size* copy_size) const {
  DCHECK(!source_size.IsEmpty());
  gfx::Size copy_thumbnail_size =
      SimpleThumbnailCrop::GetCopySizeForThumbnail(scale_factor, target_size_);

  ClipResult clipping_method = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  *clipping_rect = GetClippingRect(source_size, copy_thumbnail_size, copy_size,
                                   &clipping_method);
  return clipping_method;
}

void ContentBasedThumbnailingAlgorithm::ProcessBitmap(
    scoped_refptr<ThumbnailingContext> context,
    const ConsumerCallback& callback,
    const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context.get());
  if (bitmap.isNull() || bitmap.empty())
    return;

  gfx::Size target_thumbnail_size =
      SimpleThumbnailCrop::ComputeTargetSizeAtMaximumScale(target_size_);

  SkBitmap source_bitmap =
      PrepareSourceBitmap(bitmap, target_thumbnail_size, context.get());

  // If the source is same (or smaller) than the target, just return it as
  // the final result. Otherwise, send the shrinking task to the blocking
  // thread pool.
  if (source_bitmap.width() <= target_thumbnail_size.width() ||
      source_bitmap.height() <= target_thumbnail_size.height()) {
    context->score.boring_score =
        color_utils::CalculateBoringScore(source_bitmap);
    context->score.good_clipping =
        (context->clip_result == CLIP_RESULT_WIDER_THAN_TALL ||
         context->clip_result == CLIP_RESULT_TALLER_THAN_WIDE ||
         context->clip_result == CLIP_RESULT_NOT_CLIPPED ||
         context->clip_result == CLIP_RESULT_SOURCE_SAME_AS_TARGET);

    callback.Run(*context.get(), source_bitmap);
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE,
      {base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::Bind(&CreateRetargetedThumbnail, source_bitmap,
                 target_thumbnail_size, context, callback));
}

// static
SkBitmap ContentBasedThumbnailingAlgorithm::PrepareSourceBitmap(
    const SkBitmap& received_bitmap,
    const gfx::Size& thumbnail_size,
    ThumbnailingContext* context) {
  gfx::Size resize_target;
  SkBitmap clipped_bitmap;
  if (context->clip_result == CLIP_RESULT_UNPROCESSED) {
    // This case will require extracting a fragment from the retrieved bitmap.
    int scrollbar_size = gfx::scrollbar_size();
    gfx::Size scrollbarless(
        std::max(1, received_bitmap.width() - scrollbar_size),
        std::max(1, received_bitmap.height() - scrollbar_size));

    gfx::Rect clipping_rect = GetClippingRect(
        scrollbarless,
        thumbnail_size,
        &resize_target,
        &context->clip_result);

    received_bitmap.extractSubset(&clipped_bitmap,
                                  gfx::RectToSkIRect(clipping_rect));
  } else {
    // This means that the source bitmap has been requested and at least
    // clipped. Upstream code in same cases seems opportunistic and it may
    // not perform actual resizing if copying with resize is not supported.
    // In this case we will resize to the orignally requested copy size.
    resize_target = context->requested_copy_size;
    clipped_bitmap = received_bitmap;
  }

  SkBitmap result_bitmap = SkBitmapOperations::DownsampleByTwoUntilSize(
      clipped_bitmap, resize_target.width(), resize_target.height());

  return result_bitmap;
}

// static
void ContentBasedThumbnailingAlgorithm::CreateRetargetedThumbnail(
    const SkBitmap& source_bitmap,
    const gfx::Size& thumbnail_size,
    scoped_refptr<ThumbnailingContext> context,
    const ConsumerCallback& callback) {
  base::TimeTicks begin_compute_thumbnail = base::TimeTicks::Now();
  float kernel_sigma =
      context->clip_result == CLIP_RESULT_SOURCE_SAME_AS_TARGET ? 5.0f : 2.5f;
  SkBitmap thumbnail = thumbnailing_utils::CreateRetargetedThumbnailImage(
      source_bitmap, thumbnail_size, kernel_sigma);
  bool processing_failed = thumbnail.empty();
  if (processing_failed) {
    // Log and apply the method very much like in SimpleThumbnailCrop (except
    // that some clipping and copying is not required).
    LOG(WARNING) << "CreateRetargetedThumbnailImage failed. "
                 << "The thumbnail for " << context->url
                 << " will be created the old-fashioned way.";

    ClipResult clip_result;
    gfx::Rect clipping_rect = SimpleThumbnailCrop::GetClippingRect(
        gfx::Size(source_bitmap.width(), source_bitmap.height()),
        thumbnail_size,
        &clip_result);
    source_bitmap.extractSubset(&thumbnail, gfx::RectToSkIRect(clipping_rect));
    thumbnail = SkBitmapOperations::DownsampleByTwoUntilSize(
        thumbnail, thumbnail_size.width(), thumbnail_size.height());
  }

  if (processing_failed) {
    LOCAL_HISTOGRAM_TIMES(kFailureHistogramName,
                          base::TimeTicks::Now() - begin_compute_thumbnail);
  } else {
    LOCAL_HISTOGRAM_TIMES(kThumbnailHistogramName,
                          base::TimeTicks::Now() - begin_compute_thumbnail);
  }
  context->score.boring_score =
      color_utils::CalculateBoringScore(source_bitmap);
  if (!processing_failed)
    context->score.boring_score *= kScoreBoostFromSuccessfulRetargeting;
  context->score.good_clipping =
      (context->clip_result == CLIP_RESULT_WIDER_THAN_TALL ||
       context->clip_result == CLIP_RESULT_TALLER_THAN_WIDE ||
       context->clip_result == CLIP_RESULT_NOT_CLIPPED ||
       context->clip_result == CLIP_RESULT_SOURCE_SAME_AS_TARGET);
  // Post the result (the bitmap) back to the callback.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CallbackInvocationAdapter, callback, context, thumbnail));
}

ContentBasedThumbnailingAlgorithm::~ContentBasedThumbnailingAlgorithm() {
}

//  static
gfx::Rect ContentBasedThumbnailingAlgorithm::GetClippingRect(
    const gfx::Size& source_size,
    const gfx::Size& thumbnail_size,
    gfx::Size* target_size,
    ClipResult* clip_result) {
  // Compute and return the clipping rectagle of the source image and the
  // size of the target bitmap which will be used for the further processing.
  // This function in 'general case' is trivial (don't clip, halve the source)
  // but it is needed for handling edge cases (source smaller than the target
  // thumbnail size).
  DCHECK(target_size);
  DCHECK(clip_result);
  gfx::Rect clipping_rect;
  if (source_size.width() < thumbnail_size.width() ||
      source_size.height() < thumbnail_size.height()) {
    clipping_rect = gfx::Rect(thumbnail_size);
    *target_size = thumbnail_size;
    *clip_result = CLIP_RESULT_SOURCE_IS_SMALLER;
  } else if (source_size.width() < thumbnail_size.width() * 4 ||
             source_size.height() < thumbnail_size.height() * 4) {
    clipping_rect = gfx::Rect(source_size);
    *target_size = source_size;
    *clip_result = CLIP_RESULT_SOURCE_SAME_AS_TARGET;
  } else {
    clipping_rect = gfx::Rect(source_size);
    target_size->SetSize(source_size.width() / 2, source_size.height() / 2);
    *clip_result = CLIP_RESULT_NOT_CLIPPED;
  }

  return clipping_rect;
}

}  // namespace thumbnails
