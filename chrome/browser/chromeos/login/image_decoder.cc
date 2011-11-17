// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/image_decoder.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

ImageDecoder::ImageDecoder(Delegate* delegate,
                           const std::string& image_data)
    : delegate_(delegate),
      image_data_(image_data.begin(), image_data.end()),
      target_thread_id_(BrowserThread::UI) {
}

ImageDecoder::~ImageDecoder() {}

void ImageDecoder::Start() {
  if (!BrowserThread::GetCurrentThreadIdentifier(&target_thread_id_)) {
    NOTREACHED();
    return;
  }
  BrowserThread::PostTask(
     BrowserThread::IO, FROM_HERE,
     base::Bind(&ImageDecoder::DecodeImageInSandbox, this, image_data_));
}

bool ImageDecoder::OnMessageReceived(const IPC::Message& message) {
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
    const std::vector<unsigned char>& image_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UtilityProcessHost* utility_process_host =
      new UtilityProcessHost(this,
                             target_thread_id_);
  utility_process_host->Send(new ChromeUtilityMsg_DecodeImage(image_data));
}

}  // namespace chromeos
