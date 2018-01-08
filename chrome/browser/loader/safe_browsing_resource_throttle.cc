// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/safe_browsing_resource_throttle.h"

#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"

content::ResourceThrottle* MaybeCreateSafeBrowsingResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    safe_browsing::SafeBrowsingService* sb_service) {
  if (!sb_service->database_manager()->IsSupported())
    return nullptr;

  return new SafeBrowsingParallelResourceThrottle(request, resource_type,
                                                  sb_service);
}

SafeBrowsingParallelResourceThrottle::SafeBrowsingParallelResourceThrottle(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    safe_browsing::SafeBrowsingService* sb_service)
    : safe_browsing::BaseParallelResourceThrottle(
          request,
          resource_type,
          new safe_browsing::UrlCheckerDelegateImpl(
              sb_service->database_manager(),
              sb_service->ui_manager())) {}

SafeBrowsingParallelResourceThrottle::~SafeBrowsingParallelResourceThrottle() =
    default;

const char* SafeBrowsingParallelResourceThrottle::GetNameForLogging() const {
  return "SafeBrowsingParallelResourceThrottle";
}
