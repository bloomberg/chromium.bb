// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_INFO_LOADER_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_INFO_LOADER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "media/blink/active_loader.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoaderClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "url/gurl.h"

namespace blink {
class WebAssociatedURLLoader;
class WebLocalFrame;
class WebURLRequest;
}

namespace content {

// This class provides additional information about a media URL. Currently it
// can be used to determine if a media URL has a single security origin and
// whether the URL passes a CORS access check.
class CONTENT_EXPORT MediaInfoLoader
    : private blink::WebAssociatedURLLoaderClient {
 public:
  // Status codes for start operations on MediaInfoLoader.
  enum Status {
    // The operation failed, which may have been due to:
    //   - Page navigation
    //   - Server replied 4xx/5xx
    //   - The response was invalid
    //   - Connection was terminated
    //
    // At this point you should delete the loader.
    kFailed,

    // Everything went as planned.
    kOk,
  };

  // Callback when MediaInfoLoader finishes loading the url. Args: whether URL
  // is successfully loaded, the final URL destination following all the
  // redirect, the first party URL for the final destination, and whether
  // credentials needs to be sent to the final destination.
  typedef base::Callback<void(Status, const GURL&, const GURL&, bool)> ReadyCB;

  // Start loading information about the given media URL.
  // |url| - URL for the media resource to be loaded.
  // |cors_mode| - HTML media element's crossorigin attribute.
  // |ready_cb| - Called when media info has finished or failed loading.
  MediaInfoLoader(
      const GURL& url,
      blink::WebMediaPlayer::CORSMode cors_mode,
      const ReadyCB& ready_cb);
  ~MediaInfoLoader() override;

  // Start loading media info.
  void Start(blink::WebLocalFrame* frame);

  // Returns true if the media resource has a single origin, false otherwise.
  // Only valid to call after the loader becomes ready.
  bool HasSingleOrigin() const;

  // Returns true if the media resource passed a CORS access control check.
  // Only valid to call after the loader becomes ready.
  bool DidPassCORSAccessCheck() const;

 private:
  friend class MediaInfoLoaderTest;

  // blink::WebAssociatedURLLoaderClient implementation.
  bool WillFollowRedirect(
      const blink::WebURLRequest& newRequest,
      const blink::WebURLResponse& redirectResponse) override;
  void DidSendData(unsigned long long bytesSent,
                   unsigned long long totalBytesToBeSent) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidDownloadData(int data_length) override;
  void DidReceiveData(const char* data, int data_length) override;
  void DidReceiveCachedMetadata(const char* data, int dataLength) override;
  void DidFinishLoading(double finishTime) override;
  void DidFail(const blink::WebURLError&) override;

  void DidBecomeReady(Status status);

  // Injected WebAssociatedURLLoader instance for testing purposes.
  std::unique_ptr<blink::WebAssociatedURLLoader> test_loader_;

  // Keeps track of an active WebAssociatedURLLoader and associated state.
  std::unique_ptr<media::ActiveLoader> active_loader_;

  bool loader_failed_;
  GURL url_;
  GURL first_party_url_;
  bool allow_stored_credentials_;
  blink::WebMediaPlayer::CORSMode cors_mode_;
  bool single_origin_;

  ReadyCB ready_cb_;
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(MediaInfoLoader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_INFO_LOADER_H_
