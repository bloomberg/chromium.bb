// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_TEST_FAKE_SEARCH_PROVIDER_H_
#define IOS_PUBLIC_TEST_FAKE_SEARCH_PROVIDER_H_

#include "ios/public/provider/chrome/browser/search_provider.h"

namespace ios {

class FakeSearchProvider : public ios::SearchProvider {
 public:
  FakeSearchProvider();
  ~FakeSearchProvider() override;

 private:
  // ios::SearchProvider implementation.
  bool IsQueryExtractionEnabled() override;
  std::string InstantExtendedEnabledParam(bool for_search) override;
  std::string ForceInstantResultsParam(bool for_prerender) override;
  int OmniboxStartMargin() override;
  std::string GoogleImageSearchSource() override;

  DISALLOW_COPY_AND_ASSIGN(FakeSearchProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_TEST_FAKE_SEARCH_PROVIDER_H_
