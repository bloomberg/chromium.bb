// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_CONTENT_SETTINGS_CLIENT_H_
#define COMPONENTS_TEST_RUNNER_MOCK_CONTENT_SETTINGS_CLIENT_H_

#include "base/macros.h"
#include "third_party/WebKit/public/web/WebContentSettingsClient.h"

namespace test_runner {

class LayoutTestRuntimeFlags;
class WebTestDelegate;

class MockContentSettingsClient : public blink::WebContentSettingsClient {
 public:
  // Caller has to guarantee that |layout_test_runtime_flags| lives longer
  // than the MockContentSettingsClient being constructed here.
  MockContentSettingsClient(LayoutTestRuntimeFlags* layout_test_runtime_flags);

  ~MockContentSettingsClient() override;

  // blink::WebContentSettingsClient:
  bool allowImage(bool enabledPerSettings,
                  const blink::WebURL& imageURL) override;
  bool allowMedia(const blink::WebURL& mediaURL) override;
  bool allowScriptFromSource(bool enabledPerSettings,
                             const blink::WebURL& scriptURL) override;
  bool allowStorage(bool local) override;
  bool allowPlugins(bool enabledPerSettings) override;
  bool allowDisplayingInsecureContent(bool enabledPerSettings,
                                      const blink::WebURL&) override;
  bool allowRunningInsecureContent(bool enabledPerSettings,
                                   const blink::WebSecurityOrigin&,
                                   const blink::WebURL&) override;
  bool allowAutoplay(bool defaultValue) override;

  void SetDelegate(WebTestDelegate*);

 private:
  WebTestDelegate* delegate_;

  LayoutTestRuntimeFlags* flags_;

  DISALLOW_COPY_AND_ASSIGN(MockContentSettingsClient);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_CONTENT_SETTINGS_CLIENT_H_
