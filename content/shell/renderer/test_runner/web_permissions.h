// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_PERMISSIONS_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_PERMISSIONS_H_

#include "base/macros.h"
#include "third_party/WebKit/public/web/WebPermissionClient.h"

namespace content {

class WebTestDelegate;

class WebPermissions : public blink::WebPermissionClient {
 public:
  WebPermissions();
  virtual ~WebPermissions();

  // blink::WebPermissionClient:
  virtual bool allowImage(bool enabledPerSettings,
                          const blink::WebURL& imageURL);
  virtual bool allowMedia(const blink::WebURL& mediaURL);
  virtual bool allowScriptFromSource(bool enabledPerSettings,
                                     const blink::WebURL& scriptURL);
  virtual bool allowStorage(bool local);
  virtual bool allowPlugins(bool enabledPerSettings);
  virtual bool allowDisplayingInsecureContent(bool enabledPerSettings,
                                              const blink::WebSecurityOrigin&,
                                              const blink::WebURL&);
  virtual bool allowRunningInsecureContent(bool enabledPerSettings,
                                           const blink::WebSecurityOrigin&,
                                           const blink::WebURL&);

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

  DISALLOW_COPY_AND_ASSIGN(WebPermissions);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_PERMISSIONS_H_
