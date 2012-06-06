// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/extensions/web_auth_flow_window.h"
#include "content/public/browser/web_contents_delegate.h"
#include "googleurl/src/gurl.h"

class WebAuthFlowTest;

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

// Controller class to perform an auth flow with a provider.
// This is the class to start the auth flow and it takes care of all the
// details. It behaves the following way:
// Given a provider URL, load the URL and perform usual web navigation
// until it results in redirection to a valid extension redirect URL.
// The provider can show any UI to the user if needed before redirecting
// to an appropriate URL.
// TODO(munjal): Add link to the design doc here.
class WebAuthFlow : public content::WebContentsDelegate,
                    public WebAuthFlowWindow::Delegate {
 public:
  enum Mode {
    INTERACTIVE,  // Show UI to the user if necessary.
    SILENT        // No UI should be shown.
  };

  class Delegate {
   public:
    // Called when the auth flow is completed successfully.
    // |redirect_url| is the full URL the provider redirected to at the end
    // of the flow.
    virtual void OnAuthFlowSuccess(const std::string& redirect_url) = 0;
    // Called when the auth flow fails. This means that the flow did not result
    // in a successful redirect to a valid redirect URL or the user canceled
    // the flow.
    virtual void OnAuthFlowFailure() = 0;

    virtual ~Delegate() { }
  };

  // Interceptor interface for testing.
  class InterceptorForTests {
   public:
    virtual GURL DoIntercept(const GURL& provider_url) = 0;
    virtual ~InterceptorForTests() { }
  };
  static void SetInterceptorForTests(InterceptorForTests* interceptor);

  // Creates an instance with the given parameters.
  // Caller owns |delegate|.
  WebAuthFlow(Delegate* delegate,
              content::BrowserContext* browser_context,
              const std::string& extension_id,
              const GURL& provider_url,
              Mode mode);
  virtual ~WebAuthFlow();

  // Starts the flow.
  // Delegate will be called when the flow is done.
  virtual void Start();

 protected:
  virtual content::WebContents* CreateWebContents();
  virtual WebAuthFlowWindow* CreateAuthWindow();

 private:
  friend class ::WebAuthFlowTest;

  // WebContentsDelegate implementation.
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;
  virtual void NavigationStateChanged(
      const content::WebContents* source, unsigned changed_flags) OVERRIDE;

  // WebAuthFlowWindow::Delegate implementation.
  virtual void OnClose() OVERRIDE;

  static InterceptorForTests* interceptor;

  void OnUrlLoaded();
  // Reports the results back to the delegate.
  void ReportResult(const GURL& result);
  // Helper to initialize valid extensions URLs vector.
  void InitValidRedirectUrlPrefixes(const std::string& extension_id);
  // Checks if |url| is a valid redirect URL for the extension.
  bool IsValidRedirectUrl(const GURL& url) const;

  Delegate* delegate_;
  content::BrowserContext* browser_context_;
  GURL provider_url_;
  Mode mode_;
  // List of valid redirect URL prefixes.
  std::vector<std::string> valid_prefixes_;

  content::WebContents* contents_;
  WebAuthFlowWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthFlow);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
