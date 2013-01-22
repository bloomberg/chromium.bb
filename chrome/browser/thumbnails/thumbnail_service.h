// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_THUMBNAIL_SERVICE_H_
#define CHROME_BROWSER_THUMBNAILS_THUMBNAIL_SERVICE_H_

#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"
#include "chrome/common/thumbnail_score.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/image/image.h"

namespace base {
class RefCountedMemory;
}

namespace thumbnails {

class ThumbnailingAlgorithm;
struct ThumbnailingContext;

// An interface abstracting access to thumbnails. Intended as a temporary
// bridge facilitating switch from TopSites as the thumbnail source to a more
// robust way of handling these artefacts.
class ThumbnailService : public RefcountedProfileKeyedService {
 public:
  // Sets the given thumbnail for the given URL. Returns true if the thumbnail
  // was updated. False means either the URL wasn't known to us, or we felt
  // that our current thumbnail was superior to the given one.
  virtual bool SetPageThumbnail(const ThumbnailingContext& context,
                                const gfx::Image& thumbnail) = 0;

  // Returns the ThumbnailingAlgorithm used for processing thumbnails.
  // It is always a new instance, the caller owns it. It will encapsulate the
  // process of creating a thumbnail from tab contents. The lifetime of these
  // instances is limited to the act of processing a single tab image. They
  // are permitted to hold the state of such process.
  virtual ThumbnailingAlgorithm* GetThumbnailingAlgorithm() const = 0;

  // Gets a thumbnail for a given page. Returns true iff we have the thumbnail.
  // This may be invoked on any thread.
  // As this method may be invoked on any thread the ref count needs to be
  // incremented before this method returns, so this takes a scoped_refptr*.
  virtual bool GetPageThumbnail(
      const GURL& url,
      scoped_refptr<base::RefCountedMemory>* bytes) = 0;

  // Returns true if the page thumbnail should be updated.
  virtual bool ShouldAcquirePageThumbnail(const GURL& url) = 0;

 protected:
  virtual ~ThumbnailService() {}
};

}

#endif  // CHROME_BROWSER_THUMBNAILS_THUMBNAIL_SERVICE_H_
