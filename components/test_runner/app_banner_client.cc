// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/app_banner_client.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerPromptResult.h"

namespace test_runner {

AppBannerClient::AppBannerClient() {
}

AppBannerClient::~AppBannerClient() {
}

void AppBannerClient::registerBannerCallbacks(
    int requestId,
    blink::WebAppBannerCallbacks* callbacks) {
  callbacks_map_.AddWithID(callbacks, requestId);
}

void AppBannerClient::ResolvePromise(int request_id,
                                     const std::string& resolve_platform) {
  blink::WebAppBannerCallbacks* callbacks = callbacks_map_.Lookup(request_id);
  if (!callbacks)
    return;

  scoped_ptr<blink::WebAppBannerPromptResult> result(
      new blink::WebAppBannerPromptResult(
          blink::WebString::fromUTF8(resolve_platform),
          blink::WebAppBannerPromptResult::Outcome::Accepted));

  // If no platform has been set, treat it as dismissal.
  if (resolve_platform.empty())
    result->outcome = blink::WebAppBannerPromptResult::Outcome::Dismissed;

  callbacks->onSuccess(result.release());
  callbacks_map_.Remove(request_id);
}

}  // namespace test_runner
