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

ImageDecoder::ImageDecoder(Delegate* delegate,
                           const std::string& image_data,
                           ImageCodec image_codec)
    : delegate_(delegate),
      image_data_(image_data.begin(), image_data.end()),
      image_codec_(image_codec),
      task_runner_(NULL) {
}

ImageDecoder::~ImageDecoder() {}

void ImageDecoder::Start(scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = task_runner;
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
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  if (delegate_)
    delegate_->OnImageDecoded(this, decoded_image);
}

void ImageDecoder::OnDecodeImageFailed() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  if (delegate_)
    delegate_->OnDecodeImageFailed(this);
}

void ImageDecoder::DecodeImageInSandbox(
    const std::vector<unsigned char>& image_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UtilityProcessHost* utility_process_host;
  utility_process_host = UtilityProcessHost::Create(this, task_runner_.get());
  if (image_codec_ == ROBUST_JPEG_CODEC) {
    utility_process_host->Send(
        new ChromeUtilityMsg_RobustJPEGDecodeImage(image_data));
  } else {
    utility_process_host->Send(new ChromeUtilityMsg_DecodeImage(image_data));
  }
}
