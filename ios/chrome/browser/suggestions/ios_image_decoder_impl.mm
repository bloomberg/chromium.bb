// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/suggestions/ios_image_decoder_impl.h"

#import <UIKit/UIKit.h>

#include "base/callback.h"
#import "base/mac/bind_objc_block.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#import "components/image_fetcher/ios/webp_decoder.h"
#include "ios/web/public/web_thread.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace suggestions {

class IOSImageDecoderImpl : public image_fetcher::ImageDecoder {
 public:
  explicit IOSImageDecoderImpl(scoped_refptr<base::TaskRunner> task_runner);
  ~IOSImageDecoderImpl() override;

  // Note, that |desired_image_frame_size| is not supported
  // (http://crbug/697596).
  void DecodeImage(
      const std::string& image_data,
      const gfx::Size& desired_image_frame_size,
      const image_fetcher::ImageDecodedCallback& callback) override;

 private:
  void CreateUIImageAndRunCallback(
      const image_fetcher::ImageDecodedCallback& callback,
      NSData* image_data);

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
    const gfx::Size& desired_image_frame_size,
    const image_fetcher::ImageDecodedCallback& callback) {
  // Convert the |image_data| std::string to an NSData buffer.
  // The data is copied as it may have to outlive the caller in
  // PostTaskAndReplyWithResult.
  NSData* data =
      [NSData dataWithBytes:image_data.data() length:image_data.size()];

  // The WebP image format is not supported by iOS natively. Therefore WebP
  // images need to be decoded explicitly,
  if (webp_transcode::WebpDecoder::IsWebpImage(image_data)) {
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE, base::BindBlockArc(^NSData*() {
          return webp_transcode::WebpDecoder::DecodeWebpImage(data);
        }),
        base::Bind(&IOSImageDecoderImpl::CreateUIImageAndRunCallback,
                   weak_factory_.GetWeakPtr(), callback));
  } else {
    CreateUIImageAndRunCallback(callback, data);
  }
}

void IOSImageDecoderImpl::CreateUIImageAndRunCallback(
    const image_fetcher::ImageDecodedCallback& callback,
    NSData* image_data) {
  // Decode the image data using UIImage.
  if (image_data) {
    // "Most likely" always returns 1x images.
    UIImage* ui_image = [UIImage imageWithData:image_data scale:1];
    if (ui_image) {
      // This constructor does not retain the image, but expects to take the
      // ownership, therefore, |ui_image| is retained here, but not released
      // afterwards.
      gfx::Image gfx_image(ui_image, base::scoped_policy::RETAIN);
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
