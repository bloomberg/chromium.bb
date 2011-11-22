// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_DECODER_H_
#define CHROME_BROWSER_IMAGE_DECODER_H_
#pragma once

#include <string>
#include <vector>

#include "content/browser/utility_process_host.h"

class SkBitmap;

// Decodes an image in a sandboxed process.
class ImageDecoder : public UtilityProcessHost::Client {
 public:
  class Delegate {
   public:
    // Called when image is decoded.
    // |decoder| is used to identify the image in case of decoding several
    // images simultaneously.
    virtual void OnImageDecoded(const ImageDecoder* decoder,
                                const SkBitmap& decoded_image) = 0;

    // Called when decoding image failed. Delegate can do some cleanup in
    // this handler.
    virtual void OnDecodeImageFailed(const ImageDecoder* decoder) {}

   protected:
    virtual ~Delegate() {}
  };

  ImageDecoder(Delegate* delegate,
               const std::string& image_data);

  // Starts image decoding.
  void Start();

 private:
  // It's a reference counted object, so destructor is private.
  virtual ~ImageDecoder();

  // Overidden from UtilityProcessHost::Client:
  virtual bool OnMessageReceived(const IPC::Message& message);

  // IPC message handlers.
  void OnDecodeImageSucceeded(const SkBitmap& decoded_image);
  void OnDecodeImageFailed();

  // Launches sandboxed process that will decode the image.
  void DecodeImageInSandbox(const std::vector<unsigned char>& image_data);

  Delegate* delegate_;
  std::vector<unsigned char> image_data_;
  content::BrowserThread::ID target_thread_id_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecoder);
};

#endif  // CHROME_BROWSER_IMAGE_DECODER_H_
