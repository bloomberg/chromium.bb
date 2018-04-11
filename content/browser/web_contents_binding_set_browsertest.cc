// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_binding_set_test_binder.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_browser_associated_interfaces.mojom.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

namespace {

const char kTestHost1[] = "foo.com";
const char kTestHost2[] = "bar.com";

class WebContentsBindingSetBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule(kTestHost1, "127.0.0.1");
    host_resolver()->AddRule(kTestHost2, "127.0.0.1");
  }
};

class TestInterfaceBinder : public WebContentsBindingSetTestBinder<
                                mojom::BrowserAssociatedInterfaceTestDriver> {
 public:
  explicit TestInterfaceBinder(const base::Closure& bind_callback)
      : bind_callback_(bind_callback) {}
  ~TestInterfaceBinder() override {}

  void BindRequest(RenderFrameHost* frame_host,
                   mojom::BrowserAssociatedInterfaceTestDriverAssociatedRequest
                       request) override {
    bind_callback_.Run();
  }

 private:
  const base::Closure bind_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaceBinder);
};

class TestFrameInterfaceBinder : public mojom::WebContentsFrameBindingSetTest {
 public:
  explicit TestFrameInterfaceBinder(WebContents* web_contents)
      : bindings_(web_contents, this) {}
  ~TestFrameInterfaceBinder() override {}

 private:
  // mojom::WebContentsFrameBindingSetTest:
  void Ping(PingCallback callback) override { NOTREACHED(); }

  WebContentsFrameBindingSet<mojom::WebContentsFrameBindingSetTest> bindings_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsBindingSetBrowserTest, OverrideForTesting) {
  NavigateToURL(shell(), GURL("data:text/html,ho hum"));

  // Ensure that we can add a WebContentsFrameBindingSet and then override its
  // request handler.
  auto* web_contents = shell()->web_contents();
  WebContentsFrameBindingSet<mojom::BrowserAssociatedInterfaceTestDriver>
      frame_bindings(web_contents, nullptr);

  // Now override the binder for this interface. It quits |run_loop| whenever
  // an incoming interface request is received.
  base::RunLoop run_loop;
  WebContentsBindingSet::GetForWebContents<
      mojom::BrowserAssociatedInterfaceTestDriver>(web_contents)
      ->SetBinderForTesting(
          std::make_unique<TestInterfaceBinder>(run_loop.QuitClosure()));

  // Simulate an inbound request for the test interface. This should get routed
  // to the overriding binder and allow the test to complete.
  mojom::BrowserAssociatedInterfaceTestDriverAssociatedPtr override_client;
  static_cast<WebContentsImpl*>(web_contents)
      ->OnAssociatedInterfaceRequest(
          web_contents->GetMainFrame(),
          mojom::BrowserAssociatedInterfaceTestDriver::Name_,
          mojo::MakeRequestAssociatedWithDedicatedPipe(&override_client)
              .PassHandle());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebContentsBindingSetBrowserTest, CloseOnFrameDeletion) {
  EXPECT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(kTestHost1, "/hello.html")));

  // Simulate an inbound request on the navigated main frame.
  auto* web_contents = shell()->web_contents();
  TestFrameInterfaceBinder binder(web_contents);
  mojom::WebContentsFrameBindingSetTestAssociatedPtr override_client;
  static_cast<WebContentsImpl*>(web_contents)
      ->OnAssociatedInterfaceRequest(
          web_contents->GetMainFrame(),
          mojom::WebContentsFrameBindingSetTest::Name_,
          mojo::MakeRequestAssociatedWithDedicatedPipe(&override_client)
              .PassHandle());

  base::RunLoop run_loop;
  override_client.set_connection_error_handler(run_loop.QuitClosure());

  // Now navigate the WebContents elsewhere, eventually tearing down the old
  // main frame.
  RenderFrameDeletedObserver deleted_observer(web_contents->GetMainFrame());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(kTestHost2, "/title2.html")));
  deleted_observer.WaitUntilDeleted();

  // Verify that this message never reaches the binding for the old frame. If it
  // does, the impl will hit a DCHECK. The RunLoop terminates when the client is
  // disconnected.
  override_client->Ping(base::BindOnce([] {}));
  run_loop.Run();
}

}  // namespace content
