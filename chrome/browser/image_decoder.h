// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_DECODER_H_
#define CHROME_BROWSER_IMAGE_DECODER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/common/image_decoder.mojom.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

class SkBitmap;

// This is a helper class for decoding images safely in a utility process. To
// use this, call ImageDecoder::Start(...) or
// ImageDecoder::StartWithOptions(...) on any thread.
//
// Internally, most of the work happens on the IO thread, and then
// the callback (ImageRequest::OnImageDecoded or
// ImageRequest::OnDecodeImageFailed) is posted back to the |task_runner_|
// associated with the ImageRequest.
// The Cancel() method runs on whichever thread called it. |map_lock_| is used
// to protect the data that is accessed from multiple threads.
class ImageDecoder : public content::UtilityProcessHostClient {
 public:
  // ImageRequest objects needs to be created and destroyed on the same
  // SequencedTaskRunner.
  class ImageRequest {
   public:
    // Called when image is decoded.
    virtual void OnImageDecoded(const SkBitmap& decoded_image) = 0;

    // Called when decoding image failed. ImageRequest can do some cleanup in
    // this handler.
    virtual void OnDecodeImageFailed() {}

    base::SequencedTaskRunner* task_runner() const {
      return task_runner_.get();
    }

   protected:
    // Creates an ImageRequest that runs on the thread creating it.
    ImageRequest();
    // Explicitly pass in |task_runner| if the current thread is part of a
    // thread pool.
    explicit ImageRequest(
        const scoped_refptr<base::SequencedTaskRunner>& task_runner);
    virtual ~ImageRequest();

   private:
    // The thread to post OnImageDecoded() or OnDecodeImageFailed() once the
    // the image has been decoded.
    const scoped_refptr<base::SequencedTaskRunner> task_runner_;

    base::SequenceChecker sequence_checker_;
  };

  enum ImageCodec {
    DEFAULT_CODEC = 0,  // Uses WebKit image decoding (via WebImage).
#if defined(OS_CHROMEOS)
    ROBUST_JPEG_CODEC,  // Restrict decoding to robust jpeg codec.
    ROBUST_PNG_CODEC,  // Restrict decoding to robust PNG codec.
#endif  // defined(OS_CHROMEOS)
  };

  // Calls StartWithOptions() with ImageCodec::DEFAULT_CODEC and
  // shrink_to_fit = false.
  static void Start(ImageRequest* image_request,
                    const std::string& image_data);

  // Starts asynchronous image decoding. Once finished, the callback will be
  // posted back to image_request's |task_runner_|.
  static void StartWithOptions(ImageRequest* image_request,
                               const std::string& image_data,
                               ImageCodec image_codec,
                               bool shrink_to_fit);

  // Removes all instances of |image_request| from |image_request_id_map_|,
  // ensuring callbacks are not made to the image_request after it is destroyed.
  static void Cancel(ImageRequest* image_request);

 private:
  friend struct base::DefaultLazyInstanceTraits<ImageDecoder>;

  using RequestMap = std::map<int, ImageRequest*>;

  ImageDecoder();
  // It's a reference counted object, so destructor is private.
  ~ImageDecoder() override;

  // Sends a request to the sandboxed process to decode the image. Starts
  // batch mode if necessary. If the utility process fails to start,
  // an OnDecodeImageFailed task is posted to image_request's |task_runner_|.
  void DecodeImageInSandbox(int request_id,
                            const std::vector<unsigned char>& image_data,
                            ImageCodec image_codec,
                            bool shrink_to_fit);

  void StartWithOptionsImpl(ImageRequest* image_request,
                            const std::string& image_data,
                            ImageCodec image_codec,
                            bool shrink_to_fit);
  void CancelImpl(ImageRequest* image_request);

  // Starts UtilityProcessHost in batch mode and starts |batch_mode_timer_|.
  // If the utility process fails to start, the method resets
  // |utility_process_host_| and returns.
  void StartBatchMode();

  // Stops batch mode if no requests have come in since
  // |kBatchModeTimeoutSeconds|.
  void StopBatchMode();

  // Fails all outstanding requests.
  void FailAllRequests();

  // Overidden from UtilityProcessHostClient.
  void OnProcessCrashed(int exit_code) override;
  void OnProcessLaunchFailed() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC message handlers.
  void OnDecodeImageSucceeded(const SkBitmap& decoded_image, int request_id);
  void OnDecodeImageFailed(int request_id);

  // For the ImageRequest identified by |request_id|, call its OnImageDecoded()
  // or OnDecodeImageFailed() method on its task runner thread.
  void RunOnImageDecoded(const SkBitmap& decoded_image, int request_id);
  void RunOnDecodeImageFailed(int request_id);

  // id to use for the next Start() request that comes in.
  int image_request_id_counter_;

  // Map of request id's to ImageRequests.
  RequestMap image_request_id_map_;

  // Protects |image_request_id_map_| and |image_request_id_counter_|.
  base::Lock map_lock_;

  // The UtilityProcessHost requests are sent to.
  base::WeakPtr<content::UtilityProcessHost> utility_process_host_;

  // Calls StopBatchMode() after |kBatchModeTimeoutSeconds| have elapsed,
  // unless a new decoding request resets the timer.
  std::unique_ptr<base::DelayTimer> batch_mode_timer_;

  // Mojo service connection. Must always be bound/reset and used on the IO
  // thread.
  mojom::ImageDecoderPtr decoder_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecoder);
};

#endif  // CHROME_BROWSER_IMAGE_DECODER_H_
