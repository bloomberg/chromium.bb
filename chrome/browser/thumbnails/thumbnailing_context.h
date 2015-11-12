// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_THUMBNAILING_CONTEXT_H_
#define CHROME_BROWSER_THUMBNAILS_THUMBNAILING_CONTEXT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "components/history/core/common/thumbnail_score.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

namespace thumbnails {

// The result of clipping. This can be used to determine if the
// generated thumbnail is good or not.
enum ClipResult {
  // Clipping is not done yet.
  CLIP_RESULT_UNPROCESSED,
  // The source image is smaller.
  CLIP_RESULT_SOURCE_IS_SMALLER,
  // Wider than tall by twice or more, clip horizontally.
  CLIP_RESULT_MUCH_WIDER_THAN_TALL,
  // Wider than tall, clip horizontally.
  CLIP_RESULT_WIDER_THAN_TALL,
  // Taller than wide, clip vertically.
  CLIP_RESULT_TALLER_THAN_WIDE,
  // The source and destination aspect ratios are identical.
  CLIP_RESULT_NOT_CLIPPED,
  // The source and destination are identical.
  CLIP_RESULT_SOURCE_SAME_AS_TARGET,
};

// Holds the information needed for processing a thumbnail.
class ThumbnailingContext
    : public base::RefCountedThreadSafe<ThumbnailingContext>,
      public content::WebContentsObserver {
 public:
  ThumbnailingContext(content::WebContents* web_contents,
                      ThumbnailService* receiving_service,
                      bool load_interrupted);

  // Create an instance for use with unit tests.
  static ThumbnailingContext* CreateThumbnailingContextForTest() {
    return new ThumbnailingContext();
  }

  const scoped_refptr<ThumbnailService>& service() const;

  const GURL& GetURL() const;

  ClipResult clip_result() const;
  void set_clip_result(ClipResult result);

  gfx::Size requested_copy_size();
  void set_requested_copy_size(const gfx::Size& requested_size);

  ThumbnailScore score() const;
  void SetBoringScore(double score);
  void SetGoodClipping(bool is_good_clipping);

 private:
  ThumbnailingContext();
  ~ThumbnailingContext() override;

  friend class base::RefCountedThreadSafe<ThumbnailingContext>;

  scoped_refptr<ThumbnailService> service_;
  ClipResult clip_result_;
  gfx::Size requested_copy_size_;
  ThumbnailScore score_;
};

}  // namespace thumbnails

#endif  // CHROME_BROWSER_THUMBNAILS_THUMBNAILING_CONTEXT_H_
