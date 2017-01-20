// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_thread.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_test_sink.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "url/gurl.h"

using InstantProcessNavigationTest = ChromeRenderViewTest;
using ChromeContentRendererClientSearchBoxTest = ChromeRenderViewTest;

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

const char kHtmlWithIframe[] ="<iframe srcdoc=\"Nothing here\"></iframe>";

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
  chrome_render_thread_->set_io_task_runner(
      base::ThreadTaskRunnerHandle::Get());
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

TEST_F(ChromeContentRendererClientSearchBoxTest, RewriteThumbnailURL) {
  // Instantiate a SearchBox for the main render frame.
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(GetMainFrame());
  new SearchBox(render_frame);

  // Load a page that contains an iframe.
  LoadHTML(kHtmlWithIframe);

  ChromeContentRendererClient client;

  // Create a thumbnail URL containing the correct render view ID and an
  // arbitrary instant restricted ID.
  GURL thumbnail_url(base::StringPrintf(
      "chrome-search:/thumb/%i/1",
      render_frame->GetRenderView()->GetRoutingID()));

  GURL result;
  // Make sure the SearchBox rewrites a thumbnail request from the main frame.
  EXPECT_TRUE(client.WillSendRequest(GetMainFrame(), ui::PAGE_TRANSITION_LINK,
                                     blink::WebURL(thumbnail_url), &result));

  // Make sure the SearchBox rewrites a thumbnail request from the iframe.
  blink::WebFrame* child_frame = GetMainFrame()->firstChild();
  ASSERT_TRUE(child_frame);
  ASSERT_TRUE(child_frame->isWebLocalFrame());
  blink::WebLocalFrame* local_child =
      static_cast<blink::WebLocalFrame*>(child_frame);
  EXPECT_TRUE(client.WillSendRequest(local_child, ui::PAGE_TRANSITION_LINK,
                                     blink::WebURL(thumbnail_url), &result));
}

// The tests below examine Youtube requests that use the Flash API and ensure
// that the requests have been modified to instead use HTML5. The tests also
// check the MIME type of the request to ensure that it is "text/html".
namespace {

struct FlashEmbedsTestData {
  std::string name;
  std::string host;
  std::string path;
  std::string type;
  std::string expected_url;
};

const FlashEmbedsTestData kFlashEmbedsTestData[] = {
  {
    "Valid URL, no parameters",
    "www.youtube.com",
    "/v/deadbeef",
    "application/x-shockwave-flash",
    "/embed/deadbeef"
  },
  {
    "Valid URL, no parameters, subdomain",
    "www.foo.youtube.com",
    "/v/deadbeef",
    "application/x-shockwave-flash",
    "/embed/deadbeef"
  },
  {
    "Valid URL, many parameters",
    "www.youtube.com",
    "/v/deadbeef?start=4&fs=1",
    "application/x-shockwave-flash",
    "/embed/deadbeef?start=4&fs=1"
  },
  {
    "Invalid parameter construct, many parameters",
    "www.youtube.com",
    "/v/deadbeef&bar=4&foo=6",
    "application/x-shockwave-flash",
    "/embed/deadbeef?bar=4&foo=6"
  },
  {
    "Valid URL, enablejsapi=1",
    "www.youtube.com",
    "/v/deadbeef?enablejsapi=1",
    "",
    "/v/deadbeef?enablejsapi=1"
  }
};

}  // namespace

class ChromeContentRendererClientBrowserTest :
    public InProcessBrowserTest,
    public ::testing::WithParamInterface<FlashEmbedsTestData> {
 public:
  void MonitorRequestHandler(const net::test_server::HttpRequest& request) {
    // We're only interested in YouTube video embeds
    if (request.headers.at("Host").find("youtube.com") == std::string::npos)
      return;

    if (request.relative_url.find("/embed") != 0 &&
        request.relative_url.find("/v") != 0)
      return;

    auto type = request.headers.find("Accept");
    EXPECT_NE(std::string::npos, type->second.find("text/html"))
        << "Type is not text/html for test " << GetParam().name;

    EXPECT_EQ(request.relative_url, GetParam().expected_url)
        << "URL is wrong for test " << GetParam().name;
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     message_runner_->QuitClosure());
  }

  void WaitForYouTubeRequest() {
    message_runner_->Run();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory(
        base::FilePath(kDocRoot));
    embedded_test_server()->RegisterRequestMonitor(base::Bind(
        &ChromeContentRendererClientBrowserTest::MonitorRequestHandler,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
    message_runner_ = new content::MessageLoopRunner();
  }

 private:
  scoped_refptr<content::MessageLoopRunner> message_runner_;
};

IN_PROC_BROWSER_TEST_P(ChromeContentRendererClientBrowserTest,
                       RewriteYouTubeFlashEmbed) {
  GURL url(embedded_test_server()->GetURL("/flash_embeds.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string port = std::to_string(embedded_test_server()->port());

  std::string video_url =
      "http://" + GetParam().host + ":" + port + GetParam().path;
  EXPECT_TRUE(ExecuteScript(web_contents,
      "appendEmbedToDOM('" + video_url + "','" + GetParam().type + "');"));
  WaitForYouTubeRequest();
}

IN_PROC_BROWSER_TEST_P(ChromeContentRendererClientBrowserTest,
                       RewriteYouTubeFlashEmbedObject) {
  GURL url(embedded_test_server()->GetURL("/flash_embeds.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* web_contents =
     browser()->tab_strip_model()->GetActiveWebContents();
  std::string port = std::to_string(embedded_test_server()->port());

  std::string video_url =
     "http://" + GetParam().host + ":" + port + GetParam().path;
  EXPECT_TRUE(ExecuteScript(web_contents,
     "appendDataEmbedToDOM('" + video_url + "','" + GetParam().type + "');"));
  WaitForYouTubeRequest();
}

INSTANTIATE_TEST_CASE_P(
    FlashEmbeds,
    ChromeContentRendererClientBrowserTest,
    ::testing::ValuesIn(kFlashEmbedsTestData));
