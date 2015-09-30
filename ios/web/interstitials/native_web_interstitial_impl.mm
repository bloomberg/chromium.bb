// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/interstitials/native_web_interstitial_impl.h"

#include "base/logging.h"
#include "ios/web/public/interstitials/web_interstitial_delegate.h"
#import "ios/web/public/web_state/ui/crw_generic_content_view.h"
#include "ios/web/web_state/web_state_impl.h"
#include "ui/gfx/geometry/size.h"

namespace web {

// static
WebInterstitial* WebInterstitial::CreateNativeInterstitial(
    WebState* web_state,
    bool new_navigation,
    const GURL& url,
    scoped_ptr<NativeWebInterstitialDelegate> delegate) {
  WebStateImpl* web_state_impl = static_cast<WebStateImpl*>(web_state);
  return new NativeWebInterstitialImpl(web_state_impl, new_navigation, url,
                                       delegate.Pass());
}

NativeWebInterstitialImpl::NativeWebInterstitialImpl(
    WebStateImpl* web_state,
    bool new_navigation,
    const GURL& url,
    scoped_ptr<NativeWebInterstitialDelegate> delegate)
    : web::WebInterstitialImpl(web_state, new_navigation, url),
      delegate_(delegate.Pass()) {
  DCHECK(delegate_);
}

NativeWebInterstitialImpl::~NativeWebInterstitialImpl() {
}

CRWContentView* NativeWebInterstitialImpl::GetContentView() const {
  return content_view_.get();
}

void NativeWebInterstitialImpl::PrepareForDisplay() {
  if (!content_view_) {
    content_view_.reset([[CRWGenericContentView alloc]
        initWithView:delegate_->GetContentView()]);
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
