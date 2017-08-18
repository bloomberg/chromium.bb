// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_UNITTEST_BASE_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_UNITTEST_BASE_CHROMEOS_H_

#include <memory>

#include "base/macros.h"
#include "extensions/browser/api_unittest.h"

namespace extensions {

class ExtensionsAPIClient;

// Creates a FeedbackPrivateDelegate that can generate a SystemLogsSource for
// testing.
class FeedbackPrivateApiUnittestBase : public ApiUnitTest {
 public:
  FeedbackPrivateApiUnittestBase();
  ~FeedbackPrivateApiUnittestBase() override;

  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<ExtensionsAPIClient> extensions_api_client_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackPrivateApiUnittestBase);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_UNITTEST_BASE_CHROMEOS_H_
