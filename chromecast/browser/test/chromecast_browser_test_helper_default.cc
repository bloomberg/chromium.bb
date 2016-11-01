// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/test/chromecast_browser_test_helper.h"

#include "base/memory/ptr_util.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_media_blocker.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace chromecast {
namespace shell {
namespace {

class DefaultHelper : public ChromecastBrowserTestHelper {
 public:
  ~DefaultHelper() override {}

  content::WebContents* NavigateToURL(const GURL& url) override {
    window_.reset(new CastContentWindow);

    web_contents_ = window_->CreateWebContents(
        CastBrowserProcess::GetInstance()->browser_context());
    window_->CreateWindowTree(web_contents_.get());
    blocker_.reset(new CastMediaBlocker(
        content::MediaSession::Get(web_contents_.get()), web_contents_.get()));

    content::WaitForLoadStop(web_contents_.get());
    content::TestNavigationObserver same_tab_observer(web_contents_.get(), 1);
    content::NavigationController::LoadURLParams params(url);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    web_contents_->GetController().LoadURLWithParams(params);
    same_tab_observer.Wait();

    return web_contents_.get();
  }

  void BlockMediaLoading(bool block) override {
    blocker_->BlockMediaLoading(block);
  }

 private:
  std::unique_ptr<CastContentWindow> window_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<CastMediaBlocker> blocker_;
};

}  // namespace

std::unique_ptr<ChromecastBrowserTestHelper>
ChromecastBrowserTestHelper::Create() {
  return base::MakeUnique<DefaultHelper>();
}

}  // namespace shell
}  // namespace chromecast
