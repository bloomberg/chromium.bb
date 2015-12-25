// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_IMAGE_LOADER_H_
#define CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_IMAGE_LOADER_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"

class GURL;
class SkBitmap;

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebURL;
struct WebURLError;
class WebURLLoader;
}

namespace content {

struct NotificationImageLoaderDeleter;

// Downloads the image associated with a notification and decodes the received
// image. This must be completed before notifications are shown to the user.
// Image downloaders must not be re-used for multiple notifications.
//
// All methods, except for the constructor, are expected to be used on the
// renderer main thread.
class NotificationImageLoader
    : public blink::WebURLLoaderClient,
      public base::RefCountedThreadSafe<NotificationImageLoader,
                                        NotificationImageLoaderDeleter> {
  using ImageLoadCompletedCallback = base::Callback<void(int, const SkBitmap&)>;

 public:
  NotificationImageLoader(
      const ImageLoadCompletedCallback& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& worker_task_runner);

  // Asynchronously starts loading |image_url| using a Blink WebURLLoader. Must
  // only be called on the main thread.
  void StartOnMainThread(int notification_id, const GURL& image_url);

  // blink::WebURLLoaderClient implementation.
  void didReceiveData(blink::WebURLLoader* loader,
                      const char* data,
                      int data_length,
                      int encoded_data_length) override;
  void didFinishLoading(blink::WebURLLoader* loader,
                        double finish_time,
                        int64_t total_encoded_data_length) override;
  void didFail(blink::WebURLLoader* loader,
               const blink::WebURLError& error) override;

 private:
  friend class base::DeleteHelper<NotificationImageLoader>;
  friend class base::RefCountedThreadSafe<NotificationImageLoader,
                                          NotificationImageLoaderDeleter>;
  friend struct NotificationImageLoaderDeleter;

  ~NotificationImageLoader() override;

  // Invokes the callback on the thread this image loader was started for. When
  // the thread id is zero (the main document), it will be executed immediately.
  // For all other threads a task will be posted to the appropriate task runner.
  void RunCallbackOnWorkerThread();

  // Returns a Skia bitmap, empty if buffer_ was empty or could not be decoded
  // as an image, or a valid bitmap otherwise.
  SkBitmap GetDecodedImage() const;

  // Ensures that we delete the image loader on the main thread.
  void DeleteOnCorrectThread() const;

  ImageLoadCompletedCallback callback_;

  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  int notification_id_;
  bool completed_;

  scoped_ptr<blink::WebURLLoader> url_loader_;

  std::vector<uint8_t> buffer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationImageLoader);
};

struct NotificationImageLoaderDeleter {
  static void Destruct(const NotificationImageLoader* context) {
    context->DeleteOnCorrectThread();
  }
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_IMAGE_LOADER_H_
