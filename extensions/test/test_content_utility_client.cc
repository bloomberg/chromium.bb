// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/test_content_utility_client.h"

namespace extensions {

TestContentUtilityClient::TestContentUtilityClient() {
}

TestContentUtilityClient::~TestContentUtilityClient() {
}

void TestContentUtilityClient::UtilityThreadStarted() {
  UtilityHandler::UtilityThreadStarted();
}

bool TestContentUtilityClient::OnMessageReceived(const IPC::Message& message) {
  return utility_handler_.OnMessageReceived(message);
}


}  // namespace extensions
