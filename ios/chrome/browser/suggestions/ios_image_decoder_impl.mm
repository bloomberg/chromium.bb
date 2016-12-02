// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/suggestions/ios_image_decoder_impl.h"

#include <UIKit/UIKit.h>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#import "ios/web/public/image_fetcher/webp_decoder.h"
#include "ios/web/public/web_thread.h"
#include "ui/gfx/image/image.h"

namespace {

class WebpDecoderDelegate : public webp_transcode::WebpDecoder::Delegate {
 public:
  WebpDecoderDelegate() = default;

  NSData* data() const { return decoded_image_; }

  // WebpDecoder::Delegate methods
  void OnFinishedDecoding(bool success) override {
    if (!success)
      decoded_image_.reset();
  }

  void SetImageFeatures(
      size_t total_size,
      webp_transcode::WebpDecoder::DecodedImageFormat format) override {
    decoded_image_.reset([[NSMutableData alloc] initWithCapacity:total_size]);
  }

  void OnDataDecoded(NSData* data) override {
    DCHECK(decoded_image_);
    [decoded_image_ appendData:data];
  }

 private:
  ~WebpDecoderDelegate() override {}
  base::scoped_nsobject<NSMutableData> decoded_image_;

  DISALLOW_COPY_AND_ASSIGN(WebpDecoderDelegate);
};

// Returns an NSData object containing the decoded image data of the given
// webp_image. Returns nil in case of failure.
base::scoped_nsobject<NSData> DecodeWebpImage(
    const base::scoped_nsobject<NSData>& webp_image) {
  scoped_refptr<WebpDecoderDelegate> delegate(new WebpDecoderDelegate);
  scoped_refptr<webp_transcode::WebpDecoder> decoder(
      new webp_transcode::WebpDecoder(delegate.get()));
  decoder->OnDataReceived(webp_image);
  DLOG_IF(ERROR, !delegate->data()) << "WebP image decoding failed.";
  return base::scoped_nsobject<NSData>([delegate->data() retain]);
}

// Returns true if the given image_data is a WebP image.
//
// Every WebP file contains a 12 byte file header in the beginning of the file.
// A WebP file header starts with the four ASCII characters "RIFF". The next
// four bytes contain the image size and the last four header bytes contain the
// four ASCII characters "WEBP".
//
// WebP file header:
//                                  1 1
// Byte Nr.     0 1 2 3 4 5 6 7 8 9 0 1
// Byte value [ R I F F ? ? ? ? W E B P  ]
//
// For more information see:
// https://developers.google.com/speed/webp/docs/riff_container#webp_file_header
bool IsWebpImage(const std::string& image_data) {
  if (image_data.length() < 12)
    return false;
  return image_data.compare(0, 4, "RIFF") == 0 &&
         image_data.compare(8, 4, "WEBP") == 0;
}

}  // namespace

namespace suggestions {

class IOSImageDecoderImpl : public image_fetcher::ImageDecoder {
 public:
  explicit IOSImageDecoderImpl(scoped_refptr<base::TaskRunner> task_runner);
  ~IOSImageDecoderImpl() override;

  void DecodeImage(
      const std::string& image_data,
      const image_fetcher::ImageDecodedCallback& callback) override;

 private:
  void CreateUIImageAndRunCallback(
      const image_fetcher::ImageDecodedCallback& callback,
      const base::scoped_nsobject<NSData>& image_data);

  // The task runner used to decode images if necessary.
  const scoped_refptr<base::TaskRunner> task_runner_;

  // The WeakPtrFactory is used to cancel callbacks if ImageFetcher is
  // destroyed during WebP decoding.
  base::WeakPtrFactory<IOSImageDecoderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOSImageDecoderImpl);
};

IOSImageDecoderImpl::IOSImageDecoderImpl(
    scoped_refptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)), weak_factory_(this) {
  DCHECK(task_runner_.get());
}

IOSImageDecoderImpl::~IOSImageDecoderImpl() {}

void IOSImageDecoderImpl::DecodeImage(
    const std::string& image_data,
    const image_fetcher::ImageDecodedCallback& callback) {
  // Convert the |image_data| std::string to an NSData buffer.
  base::scoped_nsobject<NSData> data(
      [[NSData dataWithBytesNoCopy:const_cast<char*>(image_data.c_str())
                            length:image_data.length()
                      freeWhenDone:NO] retain]);

  // The WebP image format is not supported by iOS natively. Therefore WebP
  // images need to be decoded explicitly,
  if (IsWebpImage(image_data)) {
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE, base::Bind(&DecodeWebpImage, data),
        base::Bind(&IOSImageDecoderImpl::CreateUIImageAndRunCallback,
                   weak_factory_.GetWeakPtr(), callback));
  } else {
    CreateUIImageAndRunCallback(callback, data);
  }
}

void IOSImageDecoderImpl::CreateUIImageAndRunCallback(
    const image_fetcher::ImageDecodedCallback& callback,
    const base::scoped_nsobject<NSData>& image_data) {
  // Decode the image data using UIImage.
  if (image_data) {
    // "Most likely" always returns 1x images.
    UIImage* ui_image = [UIImage imageWithData:image_data scale:1];
    if (ui_image) {
      // This constructor does not retain the image, but expects to take the
      // ownership, therefore, |ui_image| is retained here, but not released
      // afterwards.
      gfx::Image gfx_image([ui_image retain]);
      callback.Run(gfx_image);
      return;
    }
  }
  gfx::Image empty_image;
  callback.Run(empty_image);
}

std::unique_ptr<image_fetcher::ImageDecoder> CreateIOSImageDecoder(
    scoped_refptr<base::TaskRunner> task_runner) {
  return base::MakeUnique<IOSImageDecoderImpl>(std::move(task_runner));
}

}  // namespace suggestions
