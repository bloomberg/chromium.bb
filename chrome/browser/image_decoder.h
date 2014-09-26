// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_DECODER_H_
#define CHROME_BROWSER_IMAGE_DECODER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/utility_process_host_client.h"

class SkBitmap;

// Decodes an image in a sandboxed process.
class ImageDecoder : public content::UtilityProcessHostClient {
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

  enum ImageCodec {
    DEFAULT_CODEC = 0,  // Uses WebKit image decoding (via WebImage).
    ROBUST_JPEG_CODEC,  // Restrict decoding to robust jpeg codec.
  };

  ImageDecoder(Delegate* delegate,
               const std::string& image_data,
               ImageCodec image_codec);

  // Starts asynchronous image decoding. Once finished, the callback will be
  // posted back to |task_runner|.
  void Start(scoped_refptr<base::SequencedTaskRunner> task_runner);

  const std::vector<unsigned char>& get_image_data() const {
    return image_data_;
  }

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }
  void set_shrink_to_fit(bool shrink_to_fit) { shrink_to_fit_ = shrink_to_fit; }

 private:
  // It's a reference counted object, so destructor is private.
  virtual ~ImageDecoder();

  // Overidden from UtilityProcessHostClient:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC message handlers.
  void OnDecodeImageSucceeded(const SkBitmap& decoded_image);
  void OnDecodeImageFailed();

  // Launches sandboxed process that will decode the image.
  void DecodeImageInSandbox(const std::vector<unsigned char>& image_data);

  Delegate* delegate_;
  std::vector<unsigned char> image_data_;
  const ImageCodec image_codec_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool shrink_to_fit_; // if needed for IPC msg size limit

  DISALLOW_COPY_AND_ASSIGN(ImageDecoder);
};

#endif  // CHROME_BROWSER_IMAGE_DECODER_H_
