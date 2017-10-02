// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_USER_MEDIA_CLIENT_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_USER_MEDIA_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/web/WebMediaDeviceChangeObserver.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"

namespace test_runner {

class WebTestDelegate;

class MockWebUserMediaClient : public blink::WebUserMediaClient {
 public:
  explicit MockWebUserMediaClient(WebTestDelegate* delegate);
  ~MockWebUserMediaClient() override;

  void RequestUserMedia(const blink::WebUserMediaRequest&) override;
  void CancelUserMediaRequest(const blink::WebUserMediaRequest&) override;
  void RequestMediaDevices(const blink::WebMediaDevicesRequest&) override;
  void SetMediaDeviceChangeObserver(
      const blink::WebMediaDeviceChangeObserver&) override;
  void ApplyConstraints(const blink::WebApplyConstraintsRequest&) override;

 private:
  WebTestDelegate* delegate_;
  blink::WebMediaDeviceChangeObserver media_device_change_observer_;
  bool should_enumerate_extra_device_;

  base::WeakPtrFactory<MockWebUserMediaClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockWebUserMediaClient);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_USER_MEDIA_CLIENT_H_
