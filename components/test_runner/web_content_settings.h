// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_WEB_CONTENT_SETTINGS_H_
#define COMPONENTS_TEST_RUNNER_WEB_CONTENT_SETTINGS_H_

#include "base/macros.h"
#include "third_party/WebKit/public/web/WebContentSettingsClient.h"

namespace test_runner {

class WebTestDelegate;

class WebContentSettings : public blink::WebContentSettingsClient {
 public:
  WebContentSettings();
  ~WebContentSettings() override;

  // blink::WebContentSettingsClient:
  bool allowImage(bool enabledPerSettings,
                  const blink::WebURL& imageURL) override;
  bool allowMedia(const blink::WebURL& mediaURL) override;
  bool allowScriptFromSource(bool enabledPerSettings,
                             const blink::WebURL& scriptURL) override;
  bool allowStorage(bool local) override;
  bool allowPlugins(bool enabledPerSettings) override;
  bool allowDisplayingInsecureContent(bool enabledPerSettings,
                                      const blink::WebSecurityOrigin&,
                                      const blink::WebURL&) override;
  bool allowRunningInsecureContent(bool enabledPerSettings,
                                   const blink::WebSecurityOrigin&,
                                   const blink::WebURL&) override;

  // Hooks to set the different policies.
  void SetImagesAllowed(bool);
  void SetMediaAllowed(bool);
  void SetScriptsAllowed(bool);
  void SetStorageAllowed(bool);
  void SetPluginsAllowed(bool);
  void SetDisplayingInsecureContentAllowed(bool);
  void SetRunningInsecureContentAllowed(bool);

  void SetDelegate(WebTestDelegate*);
  void SetDumpCallbacks(bool);

  // Resets the policy to allow everything, except for running insecure content.
  void Reset();

 private:
  WebTestDelegate* delegate_;
  bool dump_callbacks_;

  bool images_allowed_;
  bool media_allowed_;
  bool scripts_allowed_;
  bool storage_allowed_;
  bool plugins_allowed_;
  bool displaying_insecure_content_allowed_;
  bool running_insecure_content_allowed_;

  DISALLOW_COPY_AND_ASSIGN(WebContentSettings);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_WEB_CONTENT_SETTINGS_H_
