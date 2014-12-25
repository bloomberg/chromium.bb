// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/plugins/shadow_dom_plugin_placeholder.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/mock_render_thread.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using testing::_;
using testing::SetArgPointee;

typedef ChromeRenderViewTest InstantProcessNavigationTest;

// Tests that renderer-initiated navigations from an Instant render process get
// bounced back to the browser to be rebucketed into a non-Instant renderer if
// necessary.
TEST_F(InstantProcessNavigationTest, ForkForNavigationsFromInstantProcess) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kInstantProcess);
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

namespace {

// Intercepts plugin info IPCs for a mock render thread within its scope,
// and allows tests to mock the response to each request.
class ScopedMockPluginInfoFilter : public IPC::Listener, public IPC::Sender {
 public:
  explicit ScopedMockPluginInfoFilter(
      content::MockRenderThread* mock_render_thread)
      : sink_(mock_render_thread->sink()), sender_(mock_render_thread) {
    sink_.AddFilter(this);
  }
  ~ScopedMockPluginInfoFilter() override { sink_.RemoveFilter(this); }

  bool OnMessageReceived(const IPC::Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(ScopedMockPluginInfoFilter, message)
      IPC_MESSAGE_HANDLER(ChromeViewHostMsg_GetPluginInfo, OnGetPluginInfo)
      IPC_MESSAGE_UNHANDLED(return false)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  bool Send(IPC::Message* message) override { return sender_->Send(message); }

  MOCK_METHOD5(OnGetPluginInfo,
               void(int render_frame_id,
                    const GURL& url,
                    const GURL& top_origin_url,
                    const std::string& mime_type,
                    ChromeViewHostMsg_GetPluginInfo_Output* output));

 private:
  IPC::TestSink& sink_;
  IPC::Sender* sender_;
  DISALLOW_COPY_AND_ASSIGN(ScopedMockPluginInfoFilter);
};

}  // namespace

class CreatePluginPlaceholderTest : public ChromeRenderViewTest {
 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnablePluginPlaceholderShadowDom);
  }

  content::RenderFrame* GetMainRenderFrame() const {
    return view_->GetMainRenderFrame();
  }

  int GetRoutingID() const { return GetMainRenderFrame()->GetRoutingID(); }
};

TEST_F(CreatePluginPlaceholderTest, MissingPlugin) {
  GURL url("http://www.example.com/example.swf");
  std::string mime_type("application/x-shockwave-flash");

  blink::WebPluginParams params;
  params.url = url;
  params.mimeType = base::ASCIIToUTF16(mime_type);

  ChromeViewHostMsg_GetPluginInfo_Output output;
  output.status.value = ChromeViewHostMsg_GetPluginInfo_Status::kNotFound;

  ScopedMockPluginInfoFilter filter(render_thread_.get());
#if defined(ENABLE_PLUGINS)
  EXPECT_CALL(filter, OnGetPluginInfo(GetRoutingID(), url, _, mime_type, _))
      .WillOnce(SetArgPointee<4>(output));
#endif

  scoped_ptr<blink::WebPluginPlaceholder> placeholder =
      content_renderer_client_->CreatePluginPlaceholder(
          GetMainRenderFrame(), GetMainFrame(), params);
  ASSERT_NE(nullptr, placeholder);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED),
            placeholder->message());
}

TEST_F(CreatePluginPlaceholderTest, PluginFound) {
  GURL url("http://www.example.com/example.swf");
  std::string mime_type(content::kFlashPluginSwfMimeType);

  blink::WebPluginParams params;
  params.url = url;
  params.mimeType = base::ASCIIToUTF16(mime_type);

  ChromeViewHostMsg_GetPluginInfo_Output output;
  output.status.value = ChromeViewHostMsg_GetPluginInfo_Status::kAllowed;

  ScopedMockPluginInfoFilter filter(render_thread_.get());
#if defined(ENABLE_PLUGINS)
  EXPECT_CALL(filter, OnGetPluginInfo(GetRoutingID(), url, _, mime_type, _))
      .WillOnce(SetArgPointee<4>(output));
#endif

  scoped_ptr<blink::WebPluginPlaceholder> placeholder =
      content_renderer_client_->CreatePluginPlaceholder(
          GetMainRenderFrame(), GetMainFrame(), params);
  EXPECT_EQ(nullptr, placeholder);
}
