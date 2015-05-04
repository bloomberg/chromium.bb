// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/interstitials/native_web_interstitial_impl.h"

#include "base/logging.h"
#include "ios/web/public/interstitials/web_interstitial_delegate.h"
#include "ios/web/web_state/web_state_impl.h"
#include "ui/gfx/geometry/size.h"

namespace web {

// static
WebInterstitial* WebInterstitial::CreateNativeInterstitial(
    WebState* web_state,
    const GURL& url,
    scoped_ptr<NativeWebInterstitialDelegate> delegate) {
  WebStateImpl* web_state_impl = static_cast<WebStateImpl*>(web_state);
  return new NativeWebInterstitialImpl(web_state_impl, url, delegate.Pass());
}

NativeWebInterstitialImpl::NativeWebInterstitialImpl(
    WebStateImpl* web_state,
    const GURL& url,
    scoped_ptr<NativeWebInterstitialDelegate> delegate)
    : web::WebInterstitialImpl(web_state, url), delegate_(delegate.Pass()) {
  DCHECK(delegate_);
}

NativeWebInterstitialImpl::~NativeWebInterstitialImpl() {
}

void NativeWebInterstitialImpl::SetSize(const gfx::Size& size) {
  if (!content_view_)
    return;

  // Resize container and scroll view.
  CGSize cgSize = size.ToCGSize();
  [container_view_ setFrame:{[container_view_ frame].origin, cgSize}];
  [scroll_view_ setFrame:[container_view_ bounds]];

  // Resize content.
  CGSize contentSize = [content_view_ sizeThatFits:cgSize];
  [content_view_ setFrame:{CGPointZero, contentSize}];
  [scroll_view_ setContentSize:contentSize];
}

UIView* NativeWebInterstitialImpl::GetView() const {
  return container_view_.get();
}

UIScrollView* NativeWebInterstitialImpl::GetScrollView() const {
  return scroll_view_.get();
}

void NativeWebInterstitialImpl::PrepareForDisplay() {
  if (!content_view_) {
    container_view_.reset([[UIView alloc] initWithFrame:CGRectZero]);
    scroll_view_.reset([[UIScrollView alloc] initWithFrame:CGRectZero]);
    content_view_.reset(delegate_->GetContentView());
    [scroll_view_ addSubview:content_view_];
    [scroll_view_ setBackgroundColor:delegate_->GetScrollViewBackgroundColor()];
    [container_view_ addSubview:scroll_view_];
  }
}

WebInterstitialDelegate* NativeWebInterstitialImpl::GetDelegate() const {
  return delegate_.get();
}

void NativeWebInterstitialImpl::EvaluateJavaScript(
    NSString* script,
    JavaScriptCompletion completionHandler) {
  NOTREACHED() << "JavaScript cannot be evaluated on native interstitials.";
}

}  // namespace web
