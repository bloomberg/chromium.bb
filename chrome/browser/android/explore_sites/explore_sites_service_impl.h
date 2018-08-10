// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/explore_sites/explore_sites_service.h"

namespace explore_sites {

class ExploreSitesServiceImpl : public ExploreSitesService {
 public:
  ExploreSitesServiceImpl();
  ~ExploreSitesServiceImpl() override;

  // KeyedService implementation:
  void Shutdown() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExploreSitesServiceImpl);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_IMPL_H_
