// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_TEST_FAKE_STRING_PROVIDER_H_
#define IOS_PUBLIC_TEST_FAKE_STRING_PROVIDER_H_

#include "base/compiler_specific.h"
#include "ios/public/provider/chrome/browser/string_provider.h"

namespace ios {

// An impementation of StringProvider for use in tests.
class FakeStringProvider : public StringProvider {
 public:
  FakeStringProvider();
  ~FakeStringProvider() override;

  // StringProvider implementation
  std::string GetOmniboxCopyUrlString() override;
  int GetAppleFlagsTableTitleID() override;
  int GetAppleFlagsNoExperimentsAvailableID() override;
  base::string16 GetSpoofingInterstitialTitle() override;
  base::string16 GetSpoofingInterstitialHeadline() override;
  base::string16 GetSpoofingInterstitialMessage() override;
  base::string16 GetSpoofingInterstitialDetails() override;
  base::string16 GetSpoofingInterstitialFailure() override;
  int GetUnsafePortTitleID() override;
  int GetUnsafePortHeadlineID() override;
  int GetUnsafePortMessageID() override;
  int GetUnsafePortDetailsID() override;
  base::string16 GetDoneString() override;
  base::string16 GetOKString() override;
  base::string16 GetProductName() override;
};

}  // namespace ios

#endif  // IOS_PUBLIC_TEST_FAKE_STRING_PROVIDER_H_
