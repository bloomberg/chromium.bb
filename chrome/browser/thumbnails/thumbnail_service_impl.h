// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_THUMBNAIL_SERVICE_IMPL_H_
#define CHROME_BROWSER_THUMBNAILS_THUMBNAIL_SERVICE_IMPL_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"

namespace base {
class RefCountedMemory;
}

namespace thumbnails {

// An implementation of ThumbnailService which delegates storage and most of
// logic to an underlying TopSites instances.
class ThumbnailServiceImpl : public ThumbnailService {
 public:
  explicit ThumbnailServiceImpl(Profile* profile);

  // Implementation of ThumbnailService.
  virtual bool SetPageThumbnail(const ThumbnailingContext& context,
                                const gfx::Image& thumbnail) OVERRIDE;
  virtual ThumbnailingAlgorithm* GetThumbnailingAlgorithm() const OVERRIDE;
  virtual bool GetPageThumbnail(
      const GURL& url,
      scoped_refptr<base::RefCountedMemory>* bytes) OVERRIDE;
  virtual bool ShouldAcquirePageThumbnail(const GURL& url) OVERRIDE;

  // Implementation of RefcountedProfileKeyedService.
  virtual void ShutdownOnUIThread() OVERRIDE;

 private:
  virtual ~ThumbnailServiceImpl();

  scoped_refptr<history::TopSites> top_sites_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailServiceImpl);
};

}

#endif  // CHROME_BROWSER_THUMBNAILS_THUMBNAIL_SERVICE_IMPL_H_
