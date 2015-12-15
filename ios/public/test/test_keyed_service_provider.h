// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_TEST_TEST_KEYED_SERVICE_PROVIDER_H_
#define IOS_PUBLIC_TEST_TEST_KEYED_SERVICE_PROVIDER_H_

#include "base/macros.h"
#include "ios/public/provider/chrome/browser/keyed_service_provider.h"

namespace ios {

class TestKeyedServiceProvider : public KeyedServiceProvider {
 public:
  TestKeyedServiceProvider();
  ~TestKeyedServiceProvider() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestKeyedServiceProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_TEST_TEST_KEYED_SERVICE_PROVIDER_H_
