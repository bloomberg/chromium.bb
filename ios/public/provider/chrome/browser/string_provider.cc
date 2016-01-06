// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/string_provider.h"

namespace ios {

StringProvider::StringProvider() {}

StringProvider::~StringProvider() {}

std::string StringProvider::GetOmniboxCopyUrlString() {
  return std::string();
}

base::string16 StringProvider::GetSpoofingInterstitialTitle() {
  return base::string16();
}

base::string16 StringProvider::GetSpoofingInterstitialHeadline() {
  return base::string16();
}

base::string16 StringProvider::GetSpoofingInterstitialMessage() {
  return base::string16();
}

base::string16 StringProvider::GetSpoofingInterstitialDetails() {
  return base::string16();
}

base::string16 StringProvider::GetSpoofingInterstitialFailure() {
  return base::string16();
}

int StringProvider::GetUnsafePortTitleID() {
  return 0;
}

int StringProvider::GetUnsafePortHeadlineID() {
  return 0;
}

int StringProvider::GetUnsafePortMessageID() {
  return 0;
}

int StringProvider::GetUnsafePortDetailsID() {
  return 0;
}

base::string16 StringProvider::GetDoneString() {
  return base::string16();
}

base::string16 StringProvider::GetOKString() {
  return base::string16();
}

base::string16 StringProvider::GetProductName() {
  return base::string16();
}

}  // namespace ios
