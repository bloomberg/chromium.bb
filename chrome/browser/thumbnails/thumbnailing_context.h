// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_THUMBNAILING_CONTEXT_H_
#define CHROME_BROWSER_THUMBNAILS_THUMBNAILING_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_utils.h"
#include "components/history/core/common/thumbnail_score.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

namespace thumbnails {

// Holds the information needed for processing a thumbnail.
struct ThumbnailingContext : base::RefCountedThreadSafe<ThumbnailingContext> {
  ThumbnailingContext(content::WebContents* web_contents,
                      ThumbnailService* receiving_service,
                      bool load_interrupted);

  // Create an instance for use with unit tests.
  static ThumbnailingContext* CreateThumbnailingContextForTest() {
    return new ThumbnailingContext();
  }

  scoped_refptr<ThumbnailService> service;
  GURL url;
  ClipResult clip_result;
  gfx::Size requested_copy_size;
  ThumbnailScore score;

 private:
  ThumbnailingContext();
  ~ThumbnailingContext();

  friend class base::RefCountedThreadSafe<ThumbnailingContext>;
};

}  // namespace thumbnails

#endif  // CHROME_BROWSER_THUMBNAILS_THUMBNAILING_CONTEXT_H_
