// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_FACADE_DELEGATE_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_FACADE_DELEGATE_H_

namespace content {
class WebContents;
}

namespace web {

// Interface used by WebStates to drive their WebContents facades.  This pushes
// the ownership of the facade out of the web-layer to simplify upstreaming
// efforts.  Once upstream features are componentized and use WebState, this
// class will no longer be necessary.
class WebStateFacadeDelegate {
 public:
  WebStateFacadeDelegate() {}
  virtual ~WebStateFacadeDelegate() {}

  // Returns the facade object being driven by this delegate.
  virtual content::WebContents* GetWebContentsFacade() = 0;

  // Called when the web state's |isLoading| property is changed.
  virtual void OnLoadingStateChanged() = 0;
  // Called when the current page has finished loading.
  virtual void OnPageLoaded() = 0;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_FACADE_DELEGATE_H_
