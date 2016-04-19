// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_CLIENT_H_
#define IOS_WEB_PUBLIC_WEB_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "ui/base/layout.h"
#include "url/url_util.h"

namespace base {
class RefCountedStaticMemory;
}

class GURL;

@class UIWebView;
@class NSString;

namespace net {
class SSLInfo;
}

namespace web {

class BrowserState;
class BrowserURLRewriter;
class WebClient;
class WebMainParts;
class WebState;

// Setter and getter for the client.  The client should be set early, before any
// web code is called.
void SetWebClient(WebClient* client);
WebClient* GetWebClient();

// Interface that the embedder of the web layer implements.
class WebClient {
 public:
  WebClient();
  virtual ~WebClient();

  // Allows the embedder to set a custom WebMainParts implementation for the
  // browser startup code.
  virtual WebMainParts* CreateWebMainParts();

  // Gives the embedder a chance to perform tasks before a web view is created.
  virtual void PreWebViewCreation() const {}

  // Gives the embedder a chance to set up the given web view before presenting
  // it in the UI.
  virtual void PostWebViewCreation(UIWebView* web_view) const {}

  // Gives the embedder a chance to register its own standard and saveable url
  // schemes early on in the startup sequence.
  virtual void AddAdditionalSchemes(
      std::vector<url::SchemeWithType>* additional_standard_schemes) const {}

  // Returns the languages used in the Accept-Languages HTTP header.
  // Used to decide URL formating.
  virtual std::string GetAcceptLangs(BrowserState* state) const;

  // Returns the embedding application locale string.
  virtual std::string GetApplicationLocale() const;

  // Returns true if URL has application specific schema. Embedder must return
  // true for every custom app specific schema it supports. For example Chromium
  // browser would return true for "chrome://about" URL.
  virtual bool IsAppSpecificURL(const GURL& url) const;

  // Returns true if web views can be created using an alloc, init call.
  // Web view creation using an alloc, init call is disabled by default.
  // If this is disallowed all web view creation must happen through the
  // web view creation utils methods that vend a web view.
  // This is called once (only in debug builds) before the first web view is
  // created and not called repeatedly.
  virtual bool AllowWebViewAllocInit() const;

  // Returns text to be displayed for an unsupported plugin.
  virtual base::string16 GetPluginNotSupportedText() const;

  // Returns a string describing the embedder product name and version, of the
  // form "productname/version".  Used as part of the user agent string.
  virtual std::string GetProduct() const;

  // Returns the user agent. |desktop_user_agent| is true if desktop user agent
  // is requested.
  virtual std::string GetUserAgent(bool desktop_user_agent) const;

  // Returns a string resource given its id.
  virtual base::string16 GetLocalizedString(int message_id) const;

  // Returns the contents of a resource in a StringPiece given the resource id.
  virtual base::StringPiece GetDataResource(int resource_id,
                                            ui::ScaleFactor scale_factor) const;

  // Returns the raw bytes of a scale independent data resource.
  virtual base::RefCountedStaticMemory* GetDataResourceBytes(
      int resource_id) const;

  // Returns a list of additional WebUI schemes, if any. These additional
  // schemes act as aliases to the about: scheme. The additional schemes may or
  // may not serve specific WebUI pages depending on the particular
  // URLDataSourceIOS and its override of
  // URLDataSourceIOS::ShouldServiceRequest. For all schemes returned here,
  // view-source is allowed.
  virtual void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) {}

  // Gives the embedder a chance to add url rewriters to the BrowserURLRewriter
  // singleton.
  virtual void PostBrowserURLRewriterCreation(BrowserURLRewriter* rewriter) {}

  // Gives the embedder a chance to provide the JavaScript to be injected into
  // the web view as early as possible. Result must not be nil.
  virtual NSString* GetEarlyPageScript() const;

  // Informs the embedder that a certificate error has occurred. If
  // |overridable| is true, the user can ignore the error and continue. The
  // embedder can call the |callback| asynchronously (an argument of true means
  // that |cert_error| should be ignored and web// should load the page).
  virtual void AllowCertificateError(
      WebState* web_state,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool overridable,
      const base::Callback<void(bool)>& callback);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_CLIENT_H_
