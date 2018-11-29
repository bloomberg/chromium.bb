// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_IDENTITY_SERVICE_CREATOR_H_
#define IOS_CHROME_BROWSER_SIGNIN_IDENTITY_SERVICE_CREATOR_H_

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "services/service_manager/public/mojom/service.mojom.h"

// Creates an instance of the Identity Service for |browser_state| and
// |request|.
std::unique_ptr<service_manager::Service> CreateIdentityService(
    ios::ChromeBrowserState* browser_state,
    service_manager::mojom::ServiceRequest request);

#endif  // IOS_CHROME_BROWSER_SIGNIN_IDENTITY_SERVICE_CREATOR_H_
