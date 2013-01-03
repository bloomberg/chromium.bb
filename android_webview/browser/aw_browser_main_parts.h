// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_main_parts.h"

class MessageLoop;

namespace android_webview {

// TODO(joth): Remove this method when when http://crbug.com/161864 is closed.
bool UseCompositorDirectDraw();

class AwBrowserContext;

class AwBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit AwBrowserMainParts(AwBrowserContext* browser_context);
  virtual ~AwBrowserMainParts();

  // Overriding methods from content::BrowserMainParts.
  virtual void PreEarlyInitialization() OVERRIDE;
  virtual int PreCreateThreads() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;

 private:
  // Android specific UI MessageLoop.
  scoped_ptr<MessageLoop> main_message_loop_;

  AwBrowserContext* browser_context_;  // weak

  DISALLOW_COPY_AND_ASSIGN(AwBrowserMainParts);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_MAIN_PARTS_H_
