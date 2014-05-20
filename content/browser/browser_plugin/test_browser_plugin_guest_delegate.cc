// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_browser_plugin_guest_delegate.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/referrer.h"

namespace content {

TestBrowserPluginGuestDelegate::TestBrowserPluginGuestDelegate(
    BrowserPluginGuest* guest) :
    guest_(guest) {
}

TestBrowserPluginGuestDelegate::~TestBrowserPluginGuestDelegate() {
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

void TestBrowserPluginGuestDelegate::Destroy() {
  if (!destruction_callback_.is_null())
    destruction_callback_.Run(guest_->GetWebContents());
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
