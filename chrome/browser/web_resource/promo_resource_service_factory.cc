// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/promo_resource_service_factory.h"

PromoResourceServiceFactory* PromoResourceServiceFactory::GetInstance() {
  return Singleton<PromoResourceServiceFactory>::get();
}

PromoResourceServiceFactory::PromoResourceServiceFactory() {
  promo_resource_service_ = new PromoResourceService();
}

void PromoResourceServiceFactory::StartPromoResourceService() {
  promo_resource_service_->StartAfterDelay();
}

PromoResourceServiceFactory::~PromoResourceServiceFactory() {}

