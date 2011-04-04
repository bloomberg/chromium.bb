// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_FACTORY_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/web_resource/promo_resource_service.h"

// Singleton class that provides and wraps the refcounted PromoResourceService.
class PromoResourceServiceFactory {
 public:
  // Static singleton getter.
  static PromoResourceServiceFactory* GetInstance();

  // Start the PromoResourceService; separated from constructor so it can be
  // skipped in unit testing.
  void StartPromoResourceService();

 private:
  friend struct DefaultSingletonTraits<PromoResourceServiceFactory>;
  friend class PromoResourceServiceTest;

  PromoResourceServiceFactory();
  ~PromoResourceServiceFactory();

  // Exposed for unit testing.
  PromoResourceService* promo_resource_service() {
    return promo_resource_service_;
  }

  scoped_refptr<PromoResourceService> promo_resource_service_;

  DISALLOW_COPY_AND_ASSIGN(PromoResourceServiceFactory);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_FACTORY_H_

