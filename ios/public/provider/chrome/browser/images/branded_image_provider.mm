// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"

#import <Foundation/Foundation.h>

BrandedImageProvider::BrandedImageProvider() {}

BrandedImageProvider::~BrandedImageProvider() {}

UIImage* BrandedImageProvider::GetAccountsListActivityControlsImage() {
  return nil;
}

UIImage* BrandedImageProvider::GetClearBrowsingDataAccountActivityImage() {
  return nil;
}

UIImage* BrandedImageProvider::GetClearBrowsingDataSiteDataImage() {
  return nil;
}

UIImage* BrandedImageProvider::GetContextualSearchHeaderImage() {
  return nil;
}

UIImage* BrandedImageProvider::GetSigninConfirmationSyncSettingsImage() {
  return nil;
}

UIImage* BrandedImageProvider::GetSigninConfirmationPersonalizeServicesImage() {
  return nil;
}

bool BrandedImageProvider::GetToolbarVoiceSearchButtonImageId(int* image_id) {
  return false;
}

UIImage* BrandedImageProvider::GetWhatsNewIconImage(WhatsNewIcon type) {
  return nil;
}
