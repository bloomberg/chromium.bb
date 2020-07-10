// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_NET_REQUEST_DNR_TEST_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_NET_REQUEST_DNR_TEST_BASE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
class ChromeTestExtensionLoader;

namespace declarative_net_request {

// Base test fixture for the Declarative Net Request API.
class DNRTestBase : public ExtensionServiceTestBase,
                    public testing::WithParamInterface<ExtensionLoadType> {
 public:
  DNRTestBase();

  // ExtensionServiceTestBase override.
  void SetUp() override;

 protected:
  // Returns an extension loader for the current ExtensionLoadType.
  std::unique_ptr<ChromeTestExtensionLoader> CreateExtensionLoader();

 private:
  ScopedCurrentChannel channel_;

  DISALLOW_COPY_AND_ASSIGN(DNRTestBase);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_NET_REQUEST_DNR_TEST_BASE_H_
