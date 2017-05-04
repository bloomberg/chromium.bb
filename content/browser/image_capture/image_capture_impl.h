// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IMAGE_CAPTURE_IMAGE_CAPTURE_IMPL_H_
#define CONTENT_BROWSER_IMAGE_CAPTURE_IMAGE_CAPTURE_IMPL_H_

#include "media/capture/mojo/image_capture.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace content {

class ImageCaptureImpl : public media::mojom::ImageCapture {
 public:
  ImageCaptureImpl();
  ~ImageCaptureImpl() override;

  static void Create(const service_manager::BindSourceInfo& source_info,
                     media::mojom::ImageCaptureRequest request);

  void GetCapabilities(const std::string& source_id,
                       const GetCapabilitiesCallback& callback) override;

  void SetOptions(const std::string& source_id,
                  media::mojom::PhotoSettingsPtr settings,
                  const SetOptionsCallback& callback) override;

  void TakePhoto(const std::string& source_id,
                 const TakePhotoCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageCaptureImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_IMAGE_CAPTURE_IMAGE_CAPTURE_IMPL_H_
