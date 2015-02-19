// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_WEB_WEB_CONTROLLER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_WEB_WEB_CONTROLLER_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "ios/web/public/block_types.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "url/gurl.h"

namespace web {
class BrowserState;
class WebState;
}

namespace ios {

class WebControllerProviderFactory;

// Setter and getter for the provider factory. The provider factory should be
// set early, before any component using WebControllerProviders is called.
void SetWebControllerProviderFactory(
    WebControllerProviderFactory* provider_factory);
WebControllerProviderFactory* GetWebControllerProviderFactory();

// Interface that provides URL-loading and JavaScript injection with optional
// dialog suppression.
class WebControllerProvider {
 public:
  // Constructor for a WebControllerProvider backed by a CRWWebController
  // initialized with |browser_state|.
  explicit WebControllerProvider(web::BrowserState* browser_state);
  virtual ~WebControllerProvider();

  // Determines whether JavaScript dialogs are allowed.
  virtual bool SuppressesDialogs() const;
  virtual void SetSuppressesDialogs(bool should_suppress_dialogs) {}

  // Triggers a load of |url|.
  virtual void LoadURL(const GURL& url) {}

  // Returns the WebState associated with this web controller.
  virtual web::WebState* GetWebState() const;

  // Injects |script| into the previously loaded page, if any, and calls
  // |completion| with the result.  Calls |completion| with nil parameters
  // when there is no previously loaded page.
  virtual void InjectScript(const std::string& script,
                            web::JavaScriptCompletion completion);
};

class WebControllerProviderFactory {
 public:
  WebControllerProviderFactory();
  virtual ~WebControllerProviderFactory();

  // Vends WebControllerProviders created using |browser_state|, passing
  // ownership to callers.
  virtual scoped_ptr<WebControllerProvider> CreateWebControllerProvider(
      web::BrowserState* browser_state);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_WEB_WEB_CONTROLLER_PROVIDER_H_
