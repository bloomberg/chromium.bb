// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_distiller_page.h"

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "components/google/core/browser/google_util.h"
#include "ios/chrome/browser/reading_list/favicon_web_state_dispatcher_impl.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#import "net/base/mac/url_conversions.h"
#include "net/cert/cert_status_flags.h"
#include "url/url_constants.h"

namespace {
// The delay given to the web page to render after the PageLoaded callback.
const int64_t kPageLoadDelayInSeconds = 2;

const char* kGetIframeURLJavaScript =
    "document.getElementsByTagName('iframe')[0].src;";
}  // namespace

namespace reading_list {

ReadingListDistillerPage::ReadingListDistillerPage(
    web::BrowserState* browser_state,
    FaviconWebStateDispatcher* web_state_dispatcher)
    : dom_distiller::DistillerPageIOS(browser_state),
      web_state_dispatcher_(web_state_dispatcher),
      weak_ptr_factory_(this) {}

ReadingListDistillerPage::~ReadingListDistillerPage() {}

void ReadingListDistillerPage::SetRedirectionCallback(
    RedirectionCallback redirection_callback) {
  redirection_callback_ = redirection_callback;
}

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
  original_url_ = url;

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

bool ReadingListDistillerPage::IsLoadingSuccess(
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status != web::PageLoadCompletionStatus::SUCCESS) {
    return false;
  }
  if (!CurrentWebState() || !CurrentWebState()->GetNavigationManager() ||
      !CurrentWebState()->GetNavigationManager()->GetLastCommittedItem()) {
    // Only distill fully loaded, committed pages. If the page was not fully
    // loaded, web::PageLoadCompletionStatus::FAILURE should have been passed to
    // OnLoadURLDone. But check that the item exist before using it anyway.
    return false;
  }
  web::NavigationItem* item =
      CurrentWebState()->GetNavigationManager()->GetLastCommittedItem();
  if (!item->GetURL().SchemeIsCryptographic()) {
    // HTTP is allowed.
    return true;
  }

  // On SSL connections, check there was no error.
  const web::SSLStatus& ssl_status = item->GetSSL();
  if (net::IsCertStatusError(ssl_status.cert_status) &&
      !net::IsCertStatusMinorError(ssl_status.cert_status)) {
    return false;
  }
  return true;
}

void ReadingListDistillerPage::OnLoadURLDone(
    web::PageLoadCompletionStatus load_completion_status) {
  if (!IsLoadingSuccess(load_completion_status)) {
    DistillerPageIOS::OnLoadURLDone(load_completion_status);
    return;
  }
  // Page is loaded but rendering may not be done yet. Give a delay to the page.
  base::WeakPtr<ReadingListDistillerPage> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ReadingListDistillerPage::DelayedOnLoadURLDone, weak_this),
      base::TimeDelta::FromSeconds(kPageLoadDelayInSeconds));
}

void ReadingListDistillerPage::DelayedOnLoadURLDone() {
  if (IsGoogleCachedAMPPage()) {
    // Workaround for Google AMP pages.
    HandleGoogleCachedAMPPage();
  } else {
    ContinuePageDistillation();
  }
}

void ReadingListDistillerPage::ContinuePageDistillation() {
  // The page is ready to be distilled.
  // If the visible URL is not the original URL, notify the caller that URL
  // changed.
  GURL redirected_url = CurrentWebState()->GetVisibleURL();
  if (redirected_url != original_url_ && !redirection_callback_.is_null()) {
    redirection_callback_.Run(original_url_, redirected_url);
  }
  DistillerPageIOS::OnLoadURLDone(web::PageLoadCompletionStatus::SUCCESS);
}

bool ReadingListDistillerPage::IsGoogleCachedAMPPage() {
  // All google AMP pages have URL in the form "https://google_domain/amp/..."
  // and a valid certificate.
  // This method checks that this is strictly the case.
  const GURL& url = CurrentWebState()->GetLastCommittedURL();
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }
  if (!google_util::IsGoogleDomainUrl(
          url, google_util::DISALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS) ||
      !url.path().compare(0, 4, "amp/")) {
    return false;
  }
  const web::SSLStatus& ssl_status = CurrentWebState()
                                         ->GetNavigationManager()
                                         ->GetLastCommittedItem()
                                         ->GetSSL();
  if (!ssl_status.certificate ||
      (net::IsCertStatusError(ssl_status.cert_status) &&
       !net::IsCertStatusMinorError(ssl_status.cert_status))) {
    return false;
  }

  return true;
}

void ReadingListDistillerPage::HandleGoogleCachedAMPPage() {
  base::WeakPtr<ReadingListDistillerPage> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  [CurrentWebState()->GetJSInjectionReceiver()
      executeJavaScript:@(kGetIframeURLJavaScript)
      completionHandler:^(id result, NSError* error) {
        if (weak_this &&
            !weak_this->HandleGoogleCachedAMPPageJavaScriptResult(result,
                                                                  error)) {
          // If there is an error on navigation, continue normal distillation.
          weak_this->ContinuePageDistillation();
        }
        // If there is no error, the navigation completion will trigger a new
        // |OnLoadURLDone| call that will resume the distillation.
      }];
}

bool ReadingListDistillerPage::HandleGoogleCachedAMPPageJavaScriptResult(
    id result,
    id error) {
  if (error) {
    return false;
  }
  NSString* result_string = base::mac::ObjCCast<NSString>(result);
  NSURL* new_url = [NSURL URLWithString:result_string];
  if (!new_url) {
    return false;
  }
  bool is_cdn_ampproject =
      [[new_url host] isEqualToString:@"cdn.ampproject.org"];
  bool is_cdn_ampproject_subdomain =
      [[new_url host] hasSuffix:@".cdn.ampproject.org"];

  if (!is_cdn_ampproject && !is_cdn_ampproject_subdomain) {
    return false;
  }
  GURL new_gurl = net::GURLWithNSURL(new_url);
  if (!new_gurl.is_valid()) {
    return false;
  }
  web::NavigationManager::WebLoadParams params(new_gurl);
  CurrentWebState()->GetNavigationManager()->LoadURLWithParams(params);
  return true;
}

}  // namespace reading_list
