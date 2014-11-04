// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/test/stub_whispernet_client.h"

namespace copresence {

WhispernetClient::TokensCallback StubWhispernetClient::GetTokensCallback() {
  return TokensCallback();
}

WhispernetClient::SamplesCallback StubWhispernetClient::GetSamplesCallback() {
  return SamplesCallback();
}

WhispernetClient::SuccessCallback
StubWhispernetClient::GetDetectBroadcastCallback() {
  return SuccessCallback();
}

WhispernetClient::SuccessCallback
StubWhispernetClient::GetInitializedCallback() {
  return SuccessCallback();
}

}  // namespace copresence
