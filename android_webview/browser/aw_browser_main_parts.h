// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/browser_main_parts.h"

namespace base {
class MessageLoop;
}

namespace android_webview {

class AwContentBrowserClient;

class AwBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit AwBrowserMainParts(AwContentBrowserClient* browser_client);
  ~AwBrowserMainParts() override;

  // Overriding methods from content::BrowserMainParts.
  void PreEarlyInitialization() override;
  int PreCreateThreads() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;

 private:
  // Android specific UI MessageLoop.
  std::unique_ptr<base::MessageLoop> main_message_loop_;

  AwContentBrowserClient* browser_client_;

  DISALLOW_COPY_AND_ASSIGN(AwBrowserMainParts);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_
