// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/image_decoder.h"

#include "chrome/browser/browser_process.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

ImageDecoder::ImageDecoder(Delegate* delegate,
                           const std::string& image_data)
    : delegate_(delegate),
      image_data_(image_data.begin(), image_data.end()),
      target_thread_id_(BrowserThread::UI) {
}

void ImageDecoder::Start() {
  if (!BrowserThread::GetCurrentThreadIdentifier(&target_thread_id_)) {
    NOTREACHED();
    return;
  }
  BrowserThread::PostTask(
     BrowserThread::IO, FROM_HERE,
     NewRunnableMethod(
         this, &ImageDecoder::DecodeImageInSandbox,
         g_browser_process->resource_dispatcher_host(),
         image_data_));
}

void ImageDecoder::OnDecodeImageSucceeded(const SkBitmap& decoded_image) {
  DCHECK(BrowserThread::CurrentlyOn(target_thread_id_));
  if (delegate_)
    delegate_->OnImageDecoded(this, decoded_image);
}

void ImageDecoder::OnDecodeImageFailed() {
  DCHECK(BrowserThread::CurrentlyOn(target_thread_id_));
  if (delegate_)
    delegate_->OnDecodeImageFailed(this);
}

void ImageDecoder::DecodeImageInSandbox(
    ResourceDispatcherHost* rdh,
    const std::vector<unsigned char>& image_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UtilityProcessHost* utility_process_host =
      new UtilityProcessHost(rdh,
                             this,
                             target_thread_id_);
  utility_process_host->StartImageDecoding(image_data);
}

}  // namespace chromeos

