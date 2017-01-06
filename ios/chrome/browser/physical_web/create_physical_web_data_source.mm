// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/physical_web/create_physical_web_data_source.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/physical_web/ios_chrome_physical_web_data_source.h"

std::unique_ptr<physical_web::PhysicalWebDataSource>
CreateIOSChromePhysicalWebDataSource(PrefService* prefService) {
  return base::MakeUnique<IOSChromePhysicalWebDataSource>(prefService);
}
