// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/web_controller_provider_impl.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebControllerProviderImpl::WebControllerProviderImpl(
    web::BrowserState* browser_state)
    : ios::WebControllerProvider(browser_state),
      web_state_impl_(base::MakeUnique<web::WebStateImpl>(browser_state)),
      suppresses_dialogs_(false) {
  DCHECK(browser_state);
  web_state_impl_->GetNavigationManagerImpl().InitializeSession(nil, nil, NO,
                                                                -1);
  [web_state_impl_->GetWebController() setWebUsageEnabled:YES];
}

WebControllerProviderImpl::~WebControllerProviderImpl() {}

bool WebControllerProviderImpl::SuppressesDialogs() const {
  return suppresses_dialogs_;
}

void WebControllerProviderImpl::SetSuppressesDialogs(
    bool should_suppress_dialogs) {
  if (suppresses_dialogs_ != should_suppress_dialogs) {
    suppresses_dialogs_ = should_suppress_dialogs;
    [web_state_impl_->GetWebController()
        setShouldSuppressDialogs:suppresses_dialogs_];
  }
}

void WebControllerProviderImpl::LoadURL(const GURL& url) {
  if (!url.is_valid())
    return;

  [web_state_impl_->GetWebController()
      loadWithParams:web::NavigationManager::WebLoadParams(url)];
  [web_state_impl_->GetWebController() triggerPendingLoad];
  last_requested_url_ = url;
}

web::WebState* WebControllerProviderImpl::GetWebState() const {
  return web_state_impl_.get();
}

void WebControllerProviderImpl::InjectScript(
    const std::string& script,
    web::JavaScriptResultBlock completion) {
  if (web_state_impl_ &&
      web_state_impl_->GetVisibleURL() == last_requested_url_) {
    // Only inject the script if the web controller has finished loading the
    // last requested url.
    NSString* scriptString = base::SysUTF8ToNSString(script);
    [web_state_impl_->GetWebController() executeJavaScript:scriptString
                                         completionHandler:completion];
  } else if (completion) {
    // If the web controller isn't ready, call the |completion| immediately.
    completion(nil, nil);
  }
}
