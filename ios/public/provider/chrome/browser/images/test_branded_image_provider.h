// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_IMAGES_TEST_BRANDED_IMAGE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_IMAGES_TEST_BRANDED_IMAGE_PROVIDER_H_

#include "ios/public/provider/chrome/browser/images/branded_image_provider.h"

class TestBrandedImageProvider : public BrandedImageProvider {
 public:
  TestBrandedImageProvider();
  ~TestBrandedImageProvider() override;

  void set_toolbar_search_button_image(UIImage* toolbar_search_button_image) {
    toolbar_search_button_image_ = toolbar_search_button_image;
  }

  // BrandedImageProvider implementation.
  UIImage* GetToolbarSearchButtonImage() override;

 private:
  UIImage* toolbar_search_button_image_;
  DISALLOW_COPY_AND_ASSIGN(TestBrandedImageProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_IMAGES_TEST_BRANDED_IMAGE_PROVIDER_H_
