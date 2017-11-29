// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_state.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

void TestWebState::AddObserver(WebStateObserver* observer) {
  observers_.AddObserver(observer);
}

void TestWebState::RemoveObserver(WebStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

TestWebState::TestWebState()
    : browser_state_(nullptr),
      web_usage_enabled_(true),
      is_loading_(false),
      is_visible_(false),
      is_crashed_(false),
      is_evicted_(false),
      has_opener_(false),
      trust_level_(kAbsolute),
      content_is_html_(true),
      web_view_proxy_(nil) {}

TestWebState::~TestWebState() {
  for (auto& observer : observers_)
    observer.WebStateDestroyed(this);
};

WebStateDelegate* TestWebState::GetDelegate() {
  return nil;
}

void TestWebState::SetDelegate(WebStateDelegate* delegate) {}

BrowserState* TestWebState::GetBrowserState() const {
  return browser_state_;
}

bool TestWebState::IsWebUsageEnabled() const {
  return web_usage_enabled_;
}

void TestWebState::SetWebUsageEnabled(bool enabled) {
  web_usage_enabled_ = enabled;
  if (!web_usage_enabled_)
    SetIsEvicted(true);
}

bool TestWebState::ShouldSuppressDialogs() const {
  return false;
}

void TestWebState::SetShouldSuppressDialogs(bool should_suppress) {}

UIView* TestWebState::GetView() {
  return view_;
}

void TestWebState::WasShown() {
  is_visible_ = true;
  for (auto& observer : observers_)
    observer.WasShown(this);
}

void TestWebState::WasHidden() {
  is_visible_ = false;
  for (auto& observer : observers_)
    observer.WasHidden(this);
}

const NavigationManager* TestWebState::GetNavigationManager() const {
  return navigation_manager_.get();
}

NavigationManager* TestWebState::GetNavigationManager() {
  return navigation_manager_.get();
}

const SessionCertificatePolicyCache*
TestWebState::GetSessionCertificatePolicyCache() const {
  return nullptr;
}

SessionCertificatePolicyCache*
TestWebState::GetSessionCertificatePolicyCache() {
  return nullptr;
}

CRWSessionStorage* TestWebState::BuildSessionStorage() {
  return nil;
}

void TestWebState::SetNavigationManager(
    std::unique_ptr<NavigationManager> navigation_manager) {
  navigation_manager_ = std::move(navigation_manager);
}

void TestWebState::SetView(UIView* view) {
  view_ = view;
}

void TestWebState::SetIsCrashed(bool value) {
  is_crashed_ = value;
  if (is_crashed_)
    SetIsEvicted(true);
}

void TestWebState::SetIsEvicted(bool value) {
  is_evicted_ = value;
}

void TestWebState::SetWebViewProxy(CRWWebViewProxyType web_view_proxy) {
  web_view_proxy_ = web_view_proxy;
}

CRWJSInjectionReceiver* TestWebState::GetJSInjectionReceiver() const {
  return injection_receiver_;
}

void TestWebState::ExecuteJavaScript(const base::string16& javascript) {}

void TestWebState::ExecuteJavaScript(const base::string16& javascript,
                                     const JavaScriptResultCallback& callback) {
  callback.Run(nullptr);
}

void TestWebState::ExecuteUserJavaScript(NSString* javaScript) {}

const std::string& TestWebState::GetContentsMimeType() const {
  return mime_type_;
}

bool TestWebState::ContentIsHTML() const {
  return content_is_html_;
}

const GURL& TestWebState::GetVisibleURL() const {
  return url_;
}

const GURL& TestWebState::GetLastCommittedURL() const {
  return url_;
}

GURL TestWebState::GetCurrentURL(URLVerificationTrustLevel* trust_level) const {
  *trust_level = trust_level_;
  return url_;
}

bool TestWebState::IsShowingWebInterstitial() const {
  return false;
}

WebInterstitial* TestWebState::GetWebInterstitial() const {
  return nullptr;
}

void TestWebState::SetBrowserState(BrowserState* browser_state) {
  browser_state_ = browser_state;
}

void TestWebState::SetJSInjectionReceiver(
    CRWJSInjectionReceiver* injection_receiver) {
  injection_receiver_ = injection_receiver;
}

void TestWebState::SetContentIsHTML(bool content_is_html) {
  content_is_html_ = content_is_html;
}

const base::string16& TestWebState::GetTitle() const {
  return title_;
}

bool TestWebState::IsLoading() const {
  return is_loading_;
}

double TestWebState::GetLoadingProgress() const {
  return 0.0;
}

bool TestWebState::IsVisible() const {
  return is_visible_;
}

bool TestWebState::IsCrashed() const {
  return is_crashed_;
}

bool TestWebState::IsEvicted() const {
  return is_evicted_;
}

bool TestWebState::IsBeingDestroyed() const {
  return false;
}

void TestWebState::SetLoading(bool is_loading) {
  if (is_loading == is_loading_)
    return;

  is_loading_ = is_loading;

  if (is_loading) {
    for (auto& observer : observers_)
      observer.DidStartLoading(this);
  } else {
    for (auto& observer : observers_)
      observer.DidStopLoading(this);
  }
}

void TestWebState::OnPageLoaded(
    PageLoadCompletionStatus load_completion_status) {
  for (auto& observer : observers_)
    observer.PageLoaded(this, load_completion_status);
}

void TestWebState::OnNavigationStarted(NavigationContext* navigation_context) {
  for (auto& observer : observers_)
    observer.DidStartNavigation(this, navigation_context);
}

void TestWebState::OnNavigationFinished(NavigationContext* navigation_context) {
  for (auto& observer : observers_)
    observer.DidFinishNavigation(this, navigation_context);
}

void TestWebState::OnRenderProcessGone() {
  for (auto& observer : observers_)
    observer.RenderProcessGone(this);
}

void TestWebState::OnFormActivity(const FormActivityParams& params) {
  for (auto& observer : observers_) {
    observer.FormActivityRegistered(this, params);
  }
}

void TestWebState::OnDocumentSubmitted(const std::string& form_name,
                                       bool user_initiated) {
  for (auto& observer : observers_) {
    observer.DocumentSubmitted(this, form_name, user_initiated);
  }
}

void TestWebState::OnVisibleSecurityStateChanged() {
  for (auto& observer : observers_) {
    observer.DidChangeVisibleSecurityState(this);
  }
}

void TestWebState::ShowTransientContentView(CRWContentView* content_view) {
  if (content_view) {
    transient_content_view_ = content_view;
  }
}

void TestWebState::ClearTransientContentView() {
  transient_content_view_ = nil;
}

CRWContentView* TestWebState::GetTransientContentView() {
  return transient_content_view_;
}

void TestWebState::SetCurrentURL(const GURL& url) {
  url_ = url;
}

void TestWebState::SetVisibleURL(const GURL& url) {
  url_ = url;
}

void TestWebState::SetTrustLevel(URLVerificationTrustLevel trust_level) {
  trust_level_ = trust_level;
}

CRWWebViewProxyType TestWebState::GetWebViewProxy() const {
  return web_view_proxy_;
}

WebStateInterfaceProvider* TestWebState::GetWebStateInterfaceProvider() {
  return nullptr;
}

bool TestWebState::HasOpener() const {
  return has_opener_;
}

void TestWebState::SetHasOpener(bool has_opener) {
  has_opener_ = has_opener;
}

void TestWebState::TakeSnapshot(const SnapshotCallback& callback,
                                CGSize target_size) const {
  callback.Run(gfx::Image([[UIImage alloc] init]));
}

base::WeakPtr<WebState> TestWebState::AsWeakPtr() {
  NOTREACHED();
  return base::WeakPtr<WebState>();
}

}  // namespace web
