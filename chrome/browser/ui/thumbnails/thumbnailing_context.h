// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAILING_CONTEXT_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAILING_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/thumbnails/thumbnail_utils.h"
#include "components/history/core/common/thumbnail_score.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace thumbnails {

// Holds the information needed for processing a thumbnail.
struct ThumbnailingContext : base::RefCountedThreadSafe<ThumbnailingContext> {
  ThumbnailingContext(const GURL& url, bool at_top, bool load_completed);

  GURL url;
  ClipResult clip_result;
  gfx::Size requested_copy_size;
  history::ThumbnailScore score;

 private:
  ~ThumbnailingContext();

  friend class base::RefCountedThreadSafe<ThumbnailingContext>;
};

}  // namespace thumbnails

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAILING_CONTEXT_H_
