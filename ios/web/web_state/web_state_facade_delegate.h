// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_FACADE_DELEGATE_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_FACADE_DELEGATE_H_

class GURL;

namespace base {
class ListValue;
}

namespace content {
class WebContents;
}

namespace web {

class WebStateImpl;

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

  // TODO(stuartmorgan): Remove this block of methods once all necessary
  // WebUIs have been converted to the web framework.
  // Creates a content::WebUI page for the given url, owned by the facade.
  // This is used to create WebUIs that have not been converted to the web
  // infrastructure, but are made in the normal way (i.e., not managed by the
  // embedder).
  virtual void CreateLegacyWebUI(const GURL& url) = 0;
  // Clears any current WebUI. Should be called when the page changes.
  virtual void ClearLegacyWebUI() = 0;
  // Returns true if there is a legacy WebUI active.
  virtual bool HasLegacyWebUI() = 0;
  // Processes a message from a legacy WebUI displayed at the given URL.
  virtual void ProcessLegacyWebUIMessage(const GURL& source_url,
                                         const std::string& message,
                                         const base::ListValue& args) = 0;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_FACADE_DELEGATE_H_
