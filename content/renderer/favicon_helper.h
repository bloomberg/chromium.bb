// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FAVICON_HELPER_H_
#define CONTENT_RENDERER_FAVICON_HELPER_H_

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_view_observer.h"
#include "googleurl/src/gurl.h"

class SkBitmap;

namespace content {
struct FaviconURL;
}

namespace webkit_glue {
class MultiResolutionImageResourceFetcher;
}

namespace content {

struct FaviconURL;

// This class deals with favicon downloading.
// There is one FaviconHelper per RenderView, which is owned by the RenderView.
class FaviconHelper : public RenderViewObserver {
 public:
  explicit FaviconHelper(RenderView* render_view);

  // Sund a message that the favicon has changed.
  void DidChangeIcon(WebKit::WebFrame* frame,
                     WebKit::WebIconURL::Type icon_type);

 private:
  virtual ~FaviconHelper();

  // Message handler.
  void OnDownloadFavicon(int id,
                         const GURL& image_url,
                         bool is_favicon,
                         int image_size);

  // Requests to download a favicon image. When done, the RenderView
  // is notified by way of DidDownloadFavicon. Returns true if the
  // request was successfully started, false otherwise. id is used to
  // uniquely identify the request and passed back to the
  // DidDownloadFavicon method. If the image is a favicon, cookies are
  // not sent and not accepted during download.
  // If the image has multiple frames, the
  // frame whose size is image_size is returned. If the image doesn't
  // have a frame at the specified size, the first is returned.
  bool DownloadFavicon(int id,
                       const GURL& image_url,
                       bool is_favicon,
                       int image_size);

  // This callback is triggered when DownloadFavicon completes, either
  // succesfully or with a failure. See DownloadFavicon for more
  // details.
  void DidDownloadFavicon(
      int requested_size,
      webkit_glue::MultiResolutionImageResourceFetcher* fetcher,
      const std::vector<SkBitmap>& images);

  // Decodes a data: URL image or returns an empty image in case of failure.
  SkBitmap ImageFromDataUrl(const GURL&) const;

  // Send a message to update the favicon URL for a page.
  void SendUpdateFaviconURL(int32 routing_id,
                            int32 page_id,
                            const std::vector<FaviconURL>& urls);

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;

  typedef ScopedVector<webkit_glue::MultiResolutionImageResourceFetcher>
    ImageResourceFetcherList;

  // ImageResourceFetchers schedule via DownloadImage.
  ImageResourceFetcherList image_fetchers_;

  DISALLOW_COPY_AND_ASSIGN(FaviconHelper);
};

} // namespace content

#endif  // CONTENT_RENDERER_FAVICON_HELPER_H_
