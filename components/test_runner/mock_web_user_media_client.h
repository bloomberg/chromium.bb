// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_WEB_USER_MEDIA_CLIENT_H_
#define COMPONENTS_TEST_RUNNER_MOCK_WEB_USER_MEDIA_CLIENT_H_

#include "components/test_runner/web_task.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"

namespace test_runner {

class WebTestDelegate;

class MockWebUserMediaClient : public blink::WebUserMediaClient {
 public:
  explicit MockWebUserMediaClient(WebTestDelegate* delegate);
  ~MockWebUserMediaClient() override {}

  void requestUserMedia(const blink::WebUserMediaRequest&) override;
  void cancelUserMediaRequest(const blink::WebUserMediaRequest&) override;
  void requestMediaDevices(const blink::WebMediaDevicesRequest&) override;
  void cancelMediaDevicesRequest(const blink::WebMediaDevicesRequest&) override;
  void requestSources(const blink::WebMediaStreamTrackSourcesRequest&) override;

  // Task related methods
  WebTaskList* mutable_task_list() { return &task_list_; }

 private:
  WebTaskList task_list_;
  WebTestDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockWebUserMediaClient);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_WEB_USER_MEDIA_CLIENT_H_
