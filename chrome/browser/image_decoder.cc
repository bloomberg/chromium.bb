// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_decoder.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace {

// static, Leaky to allow access from any thread.
base::LazyInstance<ImageDecoder>::Leaky g_decoder = LAZY_INSTANCE_INITIALIZER;

// How long to wait after the last request has been received before ending
// batch mode.
const int kBatchModeTimeoutSeconds = 5;

}  // namespace

ImageDecoder::ImageDecoder()
    : image_request_id_counter_(0), last_request_(base::TimeTicks::Now()) {
  // A single ImageDecoder instance should live for the life of the program.
  // Explicitly add a reference so the object isn't deleted.
  AddRef();
}

ImageDecoder::~ImageDecoder() {
}

ImageDecoder::ImageRequest::ImageRequest(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {
}

ImageDecoder::ImageRequest::~ImageRequest() {
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
  DCHECK(image_request);
  DCHECK(image_request->task_runner());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ImageDecoder::DecodeImageInSandbox,
          g_decoder.Pointer(), image_request,
          std::vector<unsigned char>(image_data.begin(), image_data.end()),
          image_codec, shrink_to_fit));
}

// static
void ImageDecoder::Cancel(ImageRequest* image_request) {
  DCHECK(image_request);
  g_decoder.Pointer()->CancelImpl(image_request);
}

void ImageDecoder::DecodeImageInSandbox(
    ImageRequest* image_request,
    const std::vector<unsigned char>& image_data,
    ImageCodec image_codec,
    bool shrink_to_fit) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!utility_process_host_) {
    StartBatchMode();
  }

  last_request_ = base::TimeTicks::Now();
  base::AutoLock lock(map_lock_);
  image_request_id_map_.insert(
      std::make_pair(image_request_id_counter_, image_request));

  switch (image_codec) {
    case ROBUST_JPEG_CODEC:
      utility_process_host_->Send(new ChromeUtilityMsg_RobustJPEGDecodeImage(
          image_data, image_request_id_counter_));
      break;
    case DEFAULT_CODEC:
      utility_process_host_->Send(new ChromeUtilityMsg_DecodeImage(
          image_data, shrink_to_fit, image_request_id_counter_));
      break;
  }

  ++image_request_id_counter_;
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
      UtilityProcessHost::Create(this, base::MessageLoopProxy::current().get())
          ->AsWeakPtr();
  utility_process_host_->StartBatchMode();
  batch_mode_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kBatchModeTimeoutSeconds),
      this, &ImageDecoder::StopBatchMode);
}

void ImageDecoder::StopBatchMode() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if ((base::TimeTicks::Now() - last_request_)
      < base::TimeDelta::FromSeconds(kBatchModeTimeoutSeconds)) {
    return;
  }

  if (utility_process_host_) {
    utility_process_host_->EndBatchMode();
    utility_process_host_.reset();
  }
  batch_mode_timer_.Stop();
}

bool ImageDecoder::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageDecoder, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_DecodeImage_Succeeded,
                        OnDecodeImageSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_DecodeImage_Failed,
                        OnDecodeImageFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ImageDecoder::OnDecodeImageSucceeded(
    const SkBitmap& decoded_image,
    int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(map_lock_);
  auto it = image_request_id_map_.find(request_id);
  if (it != image_request_id_map_.end()) {
    ImageRequest* image_request = it->second;
    image_request->task_runner()->PostTask(
        FROM_HERE, base::Bind(&ImageRequest::OnImageDecoded,
                              base::Unretained(image_request), decoded_image));

    image_request_id_map_.erase(it);
  }
}

void ImageDecoder::OnDecodeImageFailed(int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(map_lock_);
  auto it = image_request_id_map_.find(request_id);
  if (it != image_request_id_map_.end()) {
    ImageRequest* image_request = it->second;
    image_request->task_runner()->PostTask(
        FROM_HERE, base::Bind(&ImageRequest::OnDecodeImageFailed,
                              base::Unretained(image_request)));

    image_request_id_map_.erase(it);
  }
}
