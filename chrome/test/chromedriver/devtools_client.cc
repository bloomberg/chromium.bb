// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/devtools_client.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/test/chromedriver/status.h"

namespace {

bool ReturnTrue() {
  return true;
}

}  // namespace

Status DevToolsClient::HandleReceivedEvents() {
  return HandleEventsUntil(base::Bind(&ReturnTrue));
}
