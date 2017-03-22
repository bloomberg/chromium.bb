// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/web_contents_binding_set_test_binder.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_browser_associated_interfaces.mojom.h"

namespace content {

using WebContentsBindingSetBrowserTest = ContentBrowserTest;

namespace {

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
          base::MakeUnique<TestInterfaceBinder>(run_loop.QuitClosure()));

  // Simulate an inbound request for the test interface. This should get routed
  // to the overriding binder and allow the test to complete.
  mojom::BrowserAssociatedInterfaceTestDriverAssociatedPtr override_client;
  static_cast<WebContentsImpl*>(web_contents)
      ->OnAssociatedInterfaceRequest(
          web_contents->GetMainFrame(),
          mojom::BrowserAssociatedInterfaceTestDriver::Name_,
          mojo::MakeIsolatedRequest(&override_client).PassHandle());
  run_loop.Run();
}

}  // namespace content
