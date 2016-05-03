// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_decoder.h"

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/image_decoder.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/common/service_registry.h"
#include "skia/public/type_converters.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace {

// static, Leaky to allow access from any thread.
base::LazyInstance<ImageDecoder>::Leaky g_decoder = LAZY_INSTANCE_INITIALIZER;

// How long to wait after the last request has been received before ending
// batch mode.
const int kBatchModeTimeoutSeconds = 5;

void OnDecodeImageDone(
    base::Callback<void(int)> fail_callback,
    base::Callback<void(const SkBitmap&, int)> success_callback,
    int request_id, skia::mojom::BitmapPtr image) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (image) {
    SkBitmap bitmap = image.To<SkBitmap>();
    if (!bitmap.empty()) {
      success_callback.Run(bitmap, request_id);
      return;
    }
  }
  fail_callback.Run(request_id);
}

}  // namespace

ImageDecoder::ImageDecoder()
    : image_request_id_counter_(0) {
  // A single ImageDecoder instance should live for the life of the program.
  // Explicitly add a reference so the object isn't deleted.
  AddRef();
}

ImageDecoder::~ImageDecoder() {
}

ImageDecoder::ImageRequest::ImageRequest()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

ImageDecoder::ImageRequest::ImageRequest(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

ImageDecoder::ImageRequest::~ImageRequest() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  ImageDecoder::Cancel(this);
}

// static
void ImageDecoder::Start(ImageRequest* image_request,
                         const std::string& image_data) {
  StartWithOptions(image_request, image_data, DEFAULT_CODEC, false);
}

// static
void ImageDecoder::StartWithOptions(ImageRequest* image_request,
                                    const std::string& image_data,
                                    ImageCodec image_codec,
                                    bool shrink_to_fit) {
  g_decoder.Pointer()->StartWithOptionsImpl(image_request, image_data,
                                            image_codec, shrink_to_fit);
}

void ImageDecoder::StartWithOptionsImpl(ImageRequest* image_request,
                                        const std::string& image_data,
                                        ImageCodec image_codec,
                                        bool shrink_to_fit) {
  DCHECK(image_request);
  DCHECK(image_request->task_runner());

  int request_id;
  {
    base::AutoLock lock(map_lock_);
    request_id = image_request_id_counter_++;
    image_request_id_map_.insert(std::make_pair(request_id, image_request));
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ImageDecoder::DecodeImageInSandbox,
          g_decoder.Pointer(), request_id,
          std::vector<unsigned char>(image_data.begin(), image_data.end()),
          image_codec, shrink_to_fit));
}

// static
void ImageDecoder::Cancel(ImageRequest* image_request) {
  DCHECK(image_request);
  g_decoder.Pointer()->CancelImpl(image_request);
}

void ImageDecoder::DecodeImageInSandbox(
    int request_id,
    const std::vector<unsigned char>& image_data,
    ImageCodec image_codec,
    bool shrink_to_fit) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(map_lock_);
  const auto it = image_request_id_map_.find(request_id);
  if (it == image_request_id_map_.end())
    return;

  ImageRequest* image_request = it->second;
  if (!utility_process_host_) {
    StartBatchMode();
  }
  if (!utility_process_host_) {
    // Utility process failed to start; notify delegate and return.
    // Without this check, we were seeing crashes on startup. Further
    // investigation is needed to determine why the utility process
    // is failing to start. See crbug.com/472272
    image_request->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ImageDecoder::RunOnDecodeImageFailed, this, request_id));
    return;
  }

  if (!batch_mode_timer_) {
    // Created here so it will call StopBatchMode() on the right thread.
    batch_mode_timer_.reset(new base::DelayTimer(
        FROM_HERE, base::TimeDelta::FromSeconds(kBatchModeTimeoutSeconds), this,
        &ImageDecoder::StopBatchMode));
  }
  batch_mode_timer_->Reset();

  mojom::ImageCodec mojo_codec = mojom::ImageCodec::DEFAULT;
#if defined(OS_CHROMEOS)
  if (image_codec == ROBUST_JPEG_CODEC)
    mojo_codec = mojom::ImageCodec::ROBUST_JPEG;
  if (image_codec == ROBUST_PNG_CODEC)
    mojo_codec = mojom::ImageCodec::ROBUST_PNG;
#endif  // defined(OS_CHROMEOS)
  decoder_->DecodeImage(
      mojo::Array<uint8_t>::From(image_data),
      mojo_codec,
      shrink_to_fit,
      base::Bind(&OnDecodeImageDone,
                 base::Bind(&ImageDecoder::OnDecodeImageFailed, this),
                 base::Bind(&ImageDecoder::OnDecodeImageSucceeded, this),
                 request_id));
}

void ImageDecoder::CancelImpl(ImageRequest* image_request) {
  base::AutoLock lock(map_lock_);
  for (auto it = image_request_id_map_.begin();
       it != image_request_id_map_.end();) {
    if (it->second == image_request) {
      image_request_id_map_.erase(it++);
    } else {
      ++it;
    }
  }
}

void ImageDecoder::StartBatchMode() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  utility_process_host_ =
      UtilityProcessHost::Create(
          this, base::ThreadTaskRunnerHandle::Get().get())->AsWeakPtr();
  utility_process_host_->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_IMAGE_DECODER_NAME));
  if (!utility_process_host_->Start()) {
    delete utility_process_host_.get();
    return;
  }
  content::ServiceRegistry* service_registry =
      utility_process_host_->GetServiceRegistry();
  service_registry->ConnectToRemoteService(mojo::GetProxy(&decoder_));
}

void ImageDecoder::StopBatchMode() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  {
    // Check for outstanding requests and wait for them to finish.
    base::AutoLock lock(map_lock_);
    if (!image_request_id_map_.empty()) {
      batch_mode_timer_->Reset();
      return;
    }
  }

  if (utility_process_host_) {
    // With Mojo, the utility process needs to be explicitly shut down by
    // deleting the host.
    delete utility_process_host_.get();
    decoder_.reset();
    utility_process_host_.reset();
  }
}

void ImageDecoder::FailAllRequests() {
  RequestMap requests;
  {
    base::AutoLock lock(map_lock_);
    requests = image_request_id_map_;
  }

  // Since |OnProcessCrashed| and |OnProcessLaunchFailed| are run asynchronously
  // from the actual event, it's possible for a new utility process to have been
  // created and sent requests by the time these functions are run. This results
  // in failing requests that are unaffected by the crash. Although not ideal,
  // this is valid and simpler than tracking which request is sent to which
  // utility process, and whether the request has been sent at all.
  for (const auto& request : requests)
    OnDecodeImageFailed(request.first);
}

void ImageDecoder::OnProcessCrashed(int exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FailAllRequests();
}

void ImageDecoder::OnProcessLaunchFailed(int error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FailAllRequests();
}

bool ImageDecoder::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void ImageDecoder::OnDecodeImageSucceeded(
    const SkBitmap& decoded_image,
    int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(map_lock_);
  auto it = image_request_id_map_.find(request_id);
  if (it == image_request_id_map_.end())
    return;

  ImageRequest* image_request = it->second;
  image_request->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ImageDecoder::RunOnImageDecoded,
                 this,
                 decoded_image,
                 request_id));
}

void ImageDecoder::OnDecodeImageFailed(int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(map_lock_);
  auto it = image_request_id_map_.find(request_id);
  if (it == image_request_id_map_.end())
    return;

  ImageRequest* image_request = it->second;
  image_request->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ImageDecoder::RunOnDecodeImageFailed, this, request_id));
}

void ImageDecoder::RunOnImageDecoded(const SkBitmap& decoded_image,
                                     int request_id) {
  ImageRequest* image_request;
  {
    base::AutoLock lock(map_lock_);
    auto it = image_request_id_map_.find(request_id);
    if (it == image_request_id_map_.end())
      return;
    image_request = it->second;
    image_request_id_map_.erase(it);
  }

  DCHECK(image_request->task_runner()->RunsTasksOnCurrentThread());
  image_request->OnImageDecoded(decoded_image);
}

void ImageDecoder::RunOnDecodeImageFailed(int request_id) {
  ImageRequest* image_request;
  {
    base::AutoLock lock(map_lock_);
    auto it = image_request_id_map_.find(request_id);
    if (it == image_request_id_map_.end())
      return;
    image_request = it->second;
    image_request_id_map_.erase(it);
  }

  DCHECK(image_request->task_runner()->RunsTasksOnCurrentThread());
  image_request->OnDecodeImageFailed();
}
