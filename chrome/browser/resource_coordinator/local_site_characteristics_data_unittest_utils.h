// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_UNITTEST_UTILS_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_UNITTEST_UTILS_H_

#include "base/macros.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace resource_coordinator {
namespace testing {

class MockLocalSiteCharacteristicsDataImplOnDestroyDelegate
    : public internal::LocalSiteCharacteristicsDataImpl::OnDestroyDelegate {
 public:
  MockLocalSiteCharacteristicsDataImplOnDestroyDelegate();
  ~MockLocalSiteCharacteristicsDataImplOnDestroyDelegate();

  MOCK_METHOD1(OnLocalSiteCharacteristicsDataImplDestroyed,
               void(internal::LocalSiteCharacteristicsDataImpl*));

 private:
  DISALLOW_COPY_AND_ASSIGN(
      MockLocalSiteCharacteristicsDataImplOnDestroyDelegate);
};

}  // namespace testing
}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_UNITTEST_UTILS_H_
