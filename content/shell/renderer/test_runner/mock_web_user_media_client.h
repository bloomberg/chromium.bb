// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_USER_MEDIA_CLIENT_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_USER_MEDIA_CLIENT_H_

#include "base/macros.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"

namespace content {

class WebTestDelegate;

class MockWebUserMediaClient : public blink::WebUserMediaClient {
 public:
  explicit MockWebUserMediaClient(WebTestDelegate* delegate);
  virtual ~MockWebUserMediaClient() {}

  virtual void requestUserMedia(const blink::WebUserMediaRequest&) OVERRIDE;
  virtual void cancelUserMediaRequest(
      const blink::WebUserMediaRequest&) OVERRIDE;
  virtual void requestMediaDevices(
      const blink::WebMediaDevicesRequest&) OVERRIDE;
  virtual void cancelMediaDevicesRequest(
      const blink::WebMediaDevicesRequest&) OVERRIDE;

  // Task related methods
  WebTaskList* mutable_task_list() { return &task_list_; }

 private:
  WebTaskList task_list_;
  WebTestDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockWebUserMediaClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEB_USER_MEDIA_CLIENT_H_
