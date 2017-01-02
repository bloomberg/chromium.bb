// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_distiller_page.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "ios/chrome/browser/reading_list/favicon_web_state_dispatcher_impl.h"
#import "ios/web/public/web_state/web_state.h"

namespace {
// The delay between the page load and the distillation in seconds.
const int64_t kDistillationDelayInSeconds = 2;
}  // namespace

namespace reading_list {

ReadingListDistillerPage::ReadingListDistillerPage(
    web::BrowserState* browser_state,
    FaviconWebStateDispatcher* web_state_dispatcher)
    : dom_distiller::DistillerPageIOS(browser_state),
      web_state_dispatcher_(web_state_dispatcher),
      weak_ptr_factory_(this) {}

ReadingListDistillerPage::~ReadingListDistillerPage() {}

void ReadingListDistillerPage::DistillPageImpl(const GURL& url,
                                               const std::string& script) {
  std::unique_ptr<web::WebState> old_web_state = DetachWebState();
  if (old_web_state) {
    web_state_dispatcher_->ReturnWebState(std::move(old_web_state));
  }
  std::unique_ptr<web::WebState> new_web_state =
      web_state_dispatcher_->RequestWebState();
  if (new_web_state) {
    favicon::WebFaviconDriver* favicon_driver =
        favicon::WebFaviconDriver::FromWebState(new_web_state.get());
    favicon_driver->FetchFavicon(url);
  }
  AttachWebState(std::move(new_web_state));

  DistillerPageIOS::DistillPageImpl(url, script);
}

void ReadingListDistillerPage::OnDistillationDone(const GURL& page_url,
                                                  const base::Value* value) {
  std::unique_ptr<web::WebState> old_web_state = DetachWebState();
  if (old_web_state) {
    web_state_dispatcher_->ReturnWebState(std::move(old_web_state));
  }
  DistillerPageIOS::OnDistillationDone(page_url, value);
}

void ReadingListDistillerPage::OnLoadURLDone(
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::FAILURE) {
    DistillerPageIOS::OnLoadURLDone(load_completion_status);
    return;
  }
  base::WeakPtr<ReadingListDistillerPage> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ReadingListDistillerPage::DelayedOnLoadURLDone,
                            weak_this, load_completion_status),
      base::TimeDelta::FromSeconds(kDistillationDelayInSeconds));
}

void ReadingListDistillerPage::DelayedOnLoadURLDone(
    web::PageLoadCompletionStatus load_completion_status) {
  DistillerPageIOS::OnLoadURLDone(load_completion_status);
}
}  // namespace reading_list
