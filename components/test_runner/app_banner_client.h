// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_APP_BANNER_CLIENT_H_
#define COMPONENTS_TEST_RUNNER_APP_BANNER_CLIENT_H_

#include "base/id_map.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerClient.h"

namespace content {

// Test app banner client that holds on to callbacks and allows the test runner
// to resolve them.
class AppBannerClient : public blink::WebAppBannerClient {
 public:
  AppBannerClient();
  virtual ~AppBannerClient();

  // blink::WebAppBannerClient:
  void registerBannerCallbacks(
      int requestId,
      blink::WebAppBannerCallbacks* callbacks) override;

  void ResolvePromise(int request_id, const std::string& platform);

 private:
  IDMap<blink::WebAppBannerCallbacks, IDMapOwnPointer> callbacks_map_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerClient);
};

}  // namespace content

#endif  // COMPONENTS_TEST_RUNNER_APP_BANNER_CLIENT_H_
