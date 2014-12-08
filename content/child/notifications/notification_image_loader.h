// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_IMAGE_LOADER_H_
#define CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_IMAGE_LOADER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"

class SkBitmap;

namespace blink {
class WebURL;
struct WebURLError;
class WebURLLoader;
}

namespace content {

class NotificationImageLoader;

// Callback to be invoked when an image load has been completed.
using NotificationImageLoadedCallback =
    base::Callback<void(scoped_refptr<NotificationImageLoader>)>;

// Downloads the image associated with a notification and decodes the received
// image. This must be completed before notifications are shown to the user.
// Image downloaders must not be re-used for multiple notifications. The image
// loader must be started from the main thread, but will invoke the callback on
// the thread identified in the StartOnMainThread call.
class NotificationImageLoader
    : public blink::WebURLLoaderClient,
      public base::RefCountedThreadSafe<NotificationImageLoader> {
 public:
  explicit NotificationImageLoader(
      const NotificationImageLoadedCallback& callback);

  // Asynchronously starts loading |image_url|.
  // Must be called on the main thread. |worker_thread_id| identifies the id
  // of the thread on which the callback should be executed upon completion.
  void StartOnMainThread(const blink::WebURL& image_url, int worker_thread_id);

  // Returns the SkBitmap resulting from decoding the loaded buffer.
  SkBitmap GetDecodedImage() const;

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

 private:
  friend class base::RefCountedThreadSafe<NotificationImageLoader>;
  virtual ~NotificationImageLoader();

  // Invokes the callback on the thread this image loader was started for. When
  // the thread id is zero (the main document), it will be executed immediately.
  // For all other threads a task will be posted to the appropriate task runner.
  void RunCallbackOnWorkerThread();

  NotificationImageLoadedCallback callback_;

  scoped_ptr<blink::WebURLLoader> url_loader_;
  int worker_thread_id_;
  bool completed_;

  std::vector<uint8_t> buffer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationImageLoader);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_IMAGE_LOADER_H_
