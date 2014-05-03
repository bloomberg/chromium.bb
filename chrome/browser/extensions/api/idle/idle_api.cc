// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/idle/idle_api.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/extensions/api/idle/idle_api_constants.h"
#include "chrome/browser/extensions/api/idle/idle_manager.h"
#include "chrome/browser/extensions/api/idle/idle_manager_factory.h"

namespace {

const int kMinThreshold = 15;  // In seconds.  Set >1 sec for security concerns.
const int kMaxThreshold = 4*60*60;  // Four hours, in seconds. Not set
                                    // arbitrarily high for security concerns.

int ClampThreshold(int threshold) {
  if (threshold < kMinThreshold) {
    threshold = kMinThreshold;
  } else if (threshold > kMaxThreshold) {
    threshold = kMaxThreshold;
  }

  return threshold;
}

}  // namespace

namespace extensions {

bool IdleQueryStateFunction::RunAsync() {
  int threshold;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &threshold));
  threshold = ClampThreshold(threshold);

  IdleManagerFactory::GetForProfile(GetProfile())->QueryState(
      threshold, base::Bind(&IdleQueryStateFunction::IdleStateCallback, this));

  // Don't send the response, it'll be sent by our callback
  return true;
}

void IdleQueryStateFunction::IdleStateCallback(IdleState state) {
  SetResult(IdleManager::CreateIdleValue(state));
  SendResponse(true);
}

bool IdleSetDetectionIntervalFunction::RunSync() {
  int threshold;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &threshold));
  threshold = ClampThreshold(threshold);

  IdleManagerFactory::GetForProfile(GetProfile())
      ->SetThreshold(extension_id(), threshold);

  return true;
}

}  // namespace extensions
