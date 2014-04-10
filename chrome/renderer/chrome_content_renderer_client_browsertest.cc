// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <vector>

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

typedef ChromeRenderViewTest InstantProcessNavigationTest;

// Tests that renderer-initiated navigations from an Instant render process get
// bounced back to the browser to be rebucketed into a non-Instant renderer if
// necessary.
TEST_F(InstantProcessNavigationTest, ForkForNavigationsFromInstantProcess) {
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kInstantProcess);
  bool unused;
  ChromeContentRendererClient* client =
      static_cast<ChromeContentRendererClient*>(content_renderer_client_.get());
  EXPECT_TRUE(client->ShouldFork(
      GetMainFrame(), GURL("http://foo"), "GET", false, false, &unused));
}

// Tests that renderer-initiated navigations from a non-Instant render process
// to potentially Instant URLs get bounced back to the browser to be rebucketed
// into an Instant renderer if necessary.
TEST_F(InstantProcessNavigationTest, ForkForNavigationsToSearchURLs) {
  ChromeContentRendererClient* client =
      static_cast<ChromeContentRendererClient*>(content_renderer_client_.get());
  chrome_render_thread_->set_io_message_loop_proxy(
      base::MessageLoopProxy::current());
  client->RenderThreadStarted();
  std::vector<GURL> search_urls;
  search_urls.push_back(GURL("http://example.com/search"));
  chrome_render_thread_->Send(new ChromeViewMsg_SetSearchURLs(
      search_urls, GURL("http://example.com/newtab")));
  bool unused;
  EXPECT_TRUE(client->ShouldFork(
      GetMainFrame(), GURL("http://example.com/newtab"), "GET", false, false,
      &unused));
  EXPECT_TRUE(client->ShouldFork(
      GetMainFrame(), GURL("http://example.com/search?q=foo"), "GET", false,
      false, &unused));
  EXPECT_FALSE(client->ShouldFork(
      GetMainFrame(), GURL("http://example.com/"), "GET", false, false,
      &unused));
}
