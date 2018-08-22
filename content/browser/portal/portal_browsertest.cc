// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/portal/portal.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/portal/portal.mojom.h"
#include "url/url_constants.h"

namespace content {

// The PortalCreatedObserver observes portal creations on
// |render_frame_host_impl|. This observer can be used to monitor for multiple
// Portal creations on the same RenderFrameHost, by repeatedly calling
// WaitUntilPortalCreated().
//
// The PortalCreatedObserver replaces the Portal interface in the
// RenderFrameHosts' BinderRegistry, so when the observer is destroyed the
// RenderFrameHost is left without an interface and attempts to create the
// interface will fail.
class PortalCreatedObserver {
 public:
  explicit PortalCreatedObserver(RenderFrameHostImpl* render_frame_host_impl)
      : render_frame_host_impl_(render_frame_host_impl) {
    service_manager::BinderRegistry& registry =
        render_frame_host_impl->BinderRegistryForTesting();

    registry.AddInterface(base::BindRepeating(
        [](PortalCreatedObserver* observer,
           RenderFrameHostImpl* render_frame_host_impl,
           blink::mojom::PortalRequest request) {
          observer->portal_ =
              Portal::Create(render_frame_host_impl, std::move(request));
          if (observer->run_loop_)
            observer->run_loop_->Quit();
        },
        base::Unretained(this), base::Unretained(render_frame_host_impl)));
  }

  ~PortalCreatedObserver() {
    service_manager::BinderRegistry& registry =
        render_frame_host_impl_->BinderRegistryForTesting();

    registry.RemoveInterface<Portal>();
  }

  Portal* WaitUntilPortalCreated() {
    Portal* portal = portal_;
    if (portal) {
      portal_ = nullptr;
      return portal;
    }

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;

    portal = portal_;
    portal_ = nullptr;
    return portal;
  }

 private:
  RenderFrameHostImpl* render_frame_host_impl_;
  base::RunLoop* run_loop_ = nullptr;
  Portal* portal_ = nullptr;
};

class PortalBrowserTest : public ContentBrowserTest {
 protected:
  PortalBrowserTest() {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPortals);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the renderer can create a Portal.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, CreatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  PortalCreatedObserver portal_created_observer(main_frame);
  EXPECT_TRUE(
      ExecJs(main_frame,
             "document.body.appendChild(document.createElement('portal'));"));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  EXPECT_NE(nullptr, portal);
}

}  // namespace content
