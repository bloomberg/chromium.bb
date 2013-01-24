// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_BROWSERETEST_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_BROWSERETEST_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/browser_plugin/mock_browser_plugin_manager.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

class RenderThreadImpl;

namespace content {

class BrowserPluginTest : public RenderViewTest {
 public:
  BrowserPluginTest();
  virtual ~BrowserPluginTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
  MockBrowserPluginManager* browser_plugin_manager() const {
    return static_cast<MockBrowserPluginManager*>(
        static_cast<RenderViewImpl*>(view_)->browser_plugin_manager());
  }
  std::string ExecuteScriptAndReturnString(const std::string& script);
  int ExecuteScriptAndReturnInt(const std::string& script);
  bool ExecuteScriptAndReturnBool(const std::string& script, bool* result);
 private:
  ContentRendererClient content_renderer_client_;
};

}  // namespace content

#endif //  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_BROWSERETEST_H_

