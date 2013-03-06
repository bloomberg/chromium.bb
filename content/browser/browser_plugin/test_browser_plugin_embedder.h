// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_EMBEDDER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_EMBEDDER_H_

#include "base/compiler_specific.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/public/test/test_utils.h"

namespace content {

class BrowserPluginGuest;
class RenderViewHost;
class WebContentsImpl;

// Test class for BrowserPluginEmbedder.
//
// Provides utilities to wait for certain state/messages in
// BrowserPluginEmbedder to be used in tests.
class TestBrowserPluginEmbedder : public BrowserPluginEmbedder {
 public:
  TestBrowserPluginEmbedder(WebContentsImpl* web_contents);
  virtual ~TestBrowserPluginEmbedder();

  // Asks the renderer process for RenderViewHost at (|x|, |y|) and waits until
  // the response arrives.
  void WaitForRenderViewHostAtPosition(int x, int y);
  RenderViewHost* last_rvh_at_position_response() {
    return last_rvh_at_position_response_;
  }

  WebContentsImpl* web_contents() const;

 private:
  void GetRenderViewHostCallback(RenderViewHost* rvh, int x, int y);

  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  RenderViewHost* last_rvh_at_position_response_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginEmbedder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_EMBEDDER_H_
