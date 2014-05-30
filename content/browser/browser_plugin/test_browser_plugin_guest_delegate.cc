// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_browser_plugin_guest_delegate.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/referrer.h"

namespace content {

// This observer ensures that the TestBrowserPluginGuestDelegate destroys itself
// when its embedder goes away.
class TestBrowserPluginGuestDelegate::EmbedderWebContentsObserver :
    public WebContentsObserver {
 public:
  explicit EmbedderWebContentsObserver(TestBrowserPluginGuestDelegate* guest)
      : WebContentsObserver(guest->GetEmbedderWebContents()),
        guest_(guest) {
  }

  virtual ~EmbedderWebContentsObserver() {
  }

  // WebContentsObserver implementation.
  virtual void WebContentsDestroyed() OVERRIDE {
    guest_->Destroy();
  }

 private:
  TestBrowserPluginGuestDelegate* guest_;

  DISALLOW_COPY_AND_ASSIGN(EmbedderWebContentsObserver);
};

TestBrowserPluginGuestDelegate::TestBrowserPluginGuestDelegate(
    BrowserPluginGuest* guest) :
    WebContentsObserver(guest->GetWebContents()),
    guest_(guest) {
}

TestBrowserPluginGuestDelegate::~TestBrowserPluginGuestDelegate() {
}

WebContents* TestBrowserPluginGuestDelegate::GetEmbedderWebContents() const {
  return guest_->embedder_web_contents();
}

void TestBrowserPluginGuestDelegate::LoadURLWithParams(
    const GURL& url,
    const Referrer& referrer,
    PageTransition transition_type,
    WebContents* web_contents) {
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.referrer = referrer;
  load_url_params.transition_type = transition_type;
  load_url_params.extra_headers = std::string();
  web_contents->GetController().LoadURLWithParams(load_url_params);
}

void TestBrowserPluginGuestDelegate::WebContentsDestroyed() {
  delete this;
}

void TestBrowserPluginGuestDelegate::DidAttach() {
  embedder_web_contents_observer_.reset(
      new EmbedderWebContentsObserver(this));

}

void TestBrowserPluginGuestDelegate::Destroy() {
  if (!destruction_callback_.is_null())
    destruction_callback_.Run();
  delete guest_->GetWebContents();
}

void TestBrowserPluginGuestDelegate::NavigateGuest(const std::string& src) {
  GURL url(src);
  LoadURLWithParams(url,
                    Referrer(),
                    PAGE_TRANSITION_AUTO_TOPLEVEL,
                    guest_->GetWebContents());
}

void TestBrowserPluginGuestDelegate::RegisterDestructionCallback(
    const DestructionCallback& callback) {
  destruction_callback_ = callback;
}


}  // namespace content
