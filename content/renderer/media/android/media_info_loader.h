// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_INFO_LOADER_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_INFO_LOADER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/renderer/media/active_loader.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/web/WebMediaPlayer.h"
#include "url/gurl.h"

namespace WebKit {
class WebFrame;
class WebURLLoader;
class WebURLRequest;
}

namespace content {

// This class provides additional information about a media URL. Currently it
// can be used to determine if a media URL has a single security origin and
// whether the URL passes a CORS access check.
class CONTENT_EXPORT MediaInfoLoader : private WebKit::WebURLLoaderClient {
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

  // Start loading information about the given media URL.
  // |url| - URL for the media resource to be loaded.
  // |cors_mode| - HTML media element's crossorigin attribute.
  // |ready_cb| - Called when media info has finished or failed loading.
  typedef base::Callback<void(Status)> ReadyCB;
  MediaInfoLoader(
      const GURL& url,
      WebKit::WebMediaPlayer::CORSMode cors_mode,
      const ReadyCB& ready_cb);
  virtual ~MediaInfoLoader();

  // Start loading media info.
  void Start(WebKit::WebFrame* frame);

  // Returns true if the media resource has a single origin, false otherwise.
  // Only valid to call after the loader becomes ready.
  bool HasSingleOrigin() const;

  // Returns true if the media resource passed a CORS access control check.
  // Only valid to call after the loader becomes ready.
  bool DidPassCORSAccessCheck() const;

 private:
  friend class MediaInfoLoaderTest;

  // WebKit::WebURLLoaderClient implementation.
  virtual void willSendRequest(
      WebKit::WebURLLoader* loader,
      WebKit::WebURLRequest& newRequest,
      const WebKit::WebURLResponse& redirectResponse);
  virtual void didSendData(
      WebKit::WebURLLoader* loader,
      unsigned long long bytesSent,
      unsigned long long totalBytesToBeSent);
  virtual void didReceiveResponse(
      WebKit::WebURLLoader* loader,
      const WebKit::WebURLResponse& response);
  virtual void didDownloadData(
      WebKit::WebURLLoader* loader,
      int data_length);
  virtual void didReceiveData(
      WebKit::WebURLLoader* loader,
      const char* data,
      int data_length,
      int encoded_data_length);
  virtual void didReceiveCachedMetadata(
      WebKit::WebURLLoader* loader,
      const char* data, int dataLength);
  virtual void didFinishLoading(
      WebKit::WebURLLoader* loader,
      double finishTime);
  virtual void didFail(
      WebKit::WebURLLoader* loader,
      const WebKit::WebURLError&);

  void DidBecomeReady(Status status);

  // Injected WebURLLoader instance for testing purposes.
  scoped_ptr<WebKit::WebURLLoader> test_loader_;

  // Keeps track of an active WebURLLoader and associated state.
  scoped_ptr<ActiveLoader> active_loader_;

  bool loader_failed_;
  GURL url_;
  WebKit::WebMediaPlayer::CORSMode cors_mode_;
  bool single_origin_;

  ReadyCB ready_cb_;
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(MediaInfoLoader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_INFO_LOADER_H_
