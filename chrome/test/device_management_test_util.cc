// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/device_management_test_util.h"

#include <string>

#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

namespace policy {

namespace {

static const char kFakeGaiaAuthToken[] = "FakeGAIAToken";

}  // namespace

void SimulateSuccessfulLogin() {
  const std::string service(GaiaConstants::kDeviceManagementService);
  const std::string auth_token(kFakeGaiaAuthToken);
  TokenService::TokenAvailableDetails details(service, auth_token);
  NotificationService::current()->Notify(
      NotificationType::TOKEN_AVAILABLE,
      Source<TokenService>(NULL),
      Details<const TokenService::TokenAvailableDetails>(&details));
}

}  // namespace policy
