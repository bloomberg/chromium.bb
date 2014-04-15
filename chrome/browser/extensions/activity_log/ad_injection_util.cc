// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/ad_injection_util.h"

#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "components/rappor/rappor_service.h"

namespace extensions {
namespace ad_injection_util {

const char* kAdInjectingApis[] = {
  "HTMLIFrameElement.src",
  "HTMLEmbedElement.src",
  "HTMLAnchorElement.href",
};

const char* kExtensionAdInjectionMetricName = "Extensions.PossibleAdInjection";

void CheckActionForAdInjection(const Action* action,
                               rappor::RapporService* rappor_service) {
  DCHECK(action);
  DCHECK(rappor_service);
  // If the action has no url, or the url is empty, then return.
  if (!action->arg_url().is_valid() || action->arg_url().is_empty())
    return;
  std::string host = action->arg_url().host();
  if (host.empty())
    return;

  bool can_inject_ads = false;
  const std::string& api_name = action->api_name();
  for (size_t i = 0; i < arraysize(kAdInjectingApis); ++i) {
    if (api_name == kAdInjectingApis[i]) {
      can_inject_ads = true;
      break;
    }
  }

  if (!can_inject_ads)
    return;

  // Record the URL - an ad *may* have been injected.
  rappor_service->RecordSample(kExtensionAdInjectionMetricName,
                               rappor::ETLD_PLUS_ONE_RAPPOR_TYPE,
                               host);
}

}  // namespace ad_injection_util
}  // namespace extensions
