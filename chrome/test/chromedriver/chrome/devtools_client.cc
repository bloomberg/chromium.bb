// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {

Status AlwaysTrue(bool* is_condition_true) {
  *is_condition_true = true;
  return Status(kOk);
}

}  // namespace

Status DevToolsClient::HandleReceivedEvents() {
  return HandleEventsUntil(base::Bind(&AlwaysTrue));
}
