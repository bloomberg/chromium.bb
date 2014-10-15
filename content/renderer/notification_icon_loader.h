// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOTIFICATION_ICON_LOADER_H_
#define CONTENT_RENDERER_NOTIFICATION_ICON_LOADER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"

class SkBitmap;

namespace blink {
class WebURL;
struct WebURLError;
class WebURLLoader;
}

namespace content {

// Downloads the icon associated with a notification and decodes the received
// image. This must be completed before notifications are shown to the user.
// Icon downloaders must not be re-used for multiple notifications or icons.
class NotificationIconLoader : public blink::WebURLLoaderClient {
  typedef base::Callback<void(int, const SkBitmap&)> DownloadCompletedCallback;

 public:
  NotificationIconLoader(int notification_id,
                 const DownloadCompletedCallback& callback);

  virtual ~NotificationIconLoader();

  // Downloads the notification's image and invokes the callback with the
  // decoded SkBitmap when the download has succeeded, or with an empty SkBitmap
  // in case the download has failed.
  void Start(const blink::WebURL& icon_url);

  // Cancels the current image download. The callback will not be invoked.
  void Cancel();

  // blink::WebURLLoaderClient implementation.
  virtual void didReceiveData(blink::WebURLLoader* loader,
                              const char* data,
                              int data_length,
                              int encoded_data_length);
  virtual void didFinishLoading(blink::WebURLLoader* loader,
                                double finish_time,
                                int64_t total_encoded_data_length);
  virtual void didFail(blink::WebURLLoader* loader,
                       const blink::WebURLError& error);

  int notification_id() const {
    return notification_id_;
  }

 private:
  int notification_id_;
  DownloadCompletedCallback callback_;

  scoped_ptr<blink::WebURLLoader> loader_;
  bool completed_;

  std::vector<uint8_t> buffer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationIconLoader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NOTIFICATION_ICON_LOADER_H_
