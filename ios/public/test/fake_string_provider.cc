// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/test/fake_string_provider.h"

namespace ios {

FakeStringProvider::FakeStringProvider() {
}

FakeStringProvider::~FakeStringProvider() {
}

std::string FakeStringProvider::GetOmniboxCopyUrlString() {
  return "Copy URL";
}

int FakeStringProvider::GetAppleFlagsTableTitleID() {
  return 0;
}

int FakeStringProvider::GetAppleFlagsNoExperimentsAvailableID() {
  return 0;
}

base::string16 FakeStringProvider::GetSpoofingInterstitialTitle() {
  return base::string16();
}

base::string16 FakeStringProvider::GetSpoofingInterstitialHeadline() {
  return base::string16();
}

base::string16 FakeStringProvider::GetSpoofingInterstitialMessage() {
  return base::string16();
}

base::string16 FakeStringProvider::GetSpoofingInterstitialDetails() {
  return base::string16();
}

base::string16 FakeStringProvider::GetSpoofingInterstitialFailure() {
  return base::string16();
}

int FakeStringProvider::GetUnsafePortTitleID() {
  return 0;
}

int FakeStringProvider::GetUnsafePortHeadlineID() {
  return 0;
}

int FakeStringProvider::GetUnsafePortMessageID() {
  return 0;
}

int FakeStringProvider::GetUnsafePortDetailsID() {
  return 0;
}

base::string16 FakeStringProvider::GetDoneString() {
  return base::string16();
}

base::string16 FakeStringProvider::GetOKString() {
  return base::string16();
}

base::string16 FakeStringProvider::GetProductName() {
  return base::string16();
}

}  // namespace ios
