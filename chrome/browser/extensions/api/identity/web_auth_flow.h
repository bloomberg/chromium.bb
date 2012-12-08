// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"

class Profile;
class WebAuthFlowTest;

namespace content {
class NotificationDetails;
class NotificationSource;
class RenderViewHost;
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
class WebAuthFlow : public content::NotificationObserver,
                    public content::WebContentsObserver {
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

   protected:
    virtual ~Delegate() {}
  };

  // Creates an instance with the given parameters.
  // Caller owns |delegate|.
  WebAuthFlow(Delegate* delegate,
              Profile* profile,
              const std::string& extension_id,
              const GURL& provider_url,
              Mode mode,
              const gfx::Rect& initial_bounds,
              chrome::HostDesktopType host_desktop_type);
  virtual ~WebAuthFlow();

  // Starts the flow.
  // Delegate will be called when the flow is done.
  virtual void Start();

 protected:
  // Overridable for testing.
  virtual content::WebContents* CreateWebContents();
  virtual void ShowAuthFlowPopup();

 private:
  friend class ::WebAuthFlowTest;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // WebContentsObserver implementation.
  virtual void ProvisionalChangeToMainFrameUrl(
      const GURL& url,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  bool BeforeUrlLoaded(const GURL& url);
  void AfterUrlLoaded();

  // Reports the results back to the delegate.
  void ReportResult(const GURL& url);
  // Checks if |url| is a valid redirect URL for the extension.
  bool IsValidRedirectUrl(const GURL& url) const;
  // Helper to initialize valid extensions URLs vector.
  void InitValidRedirectUrlPrefixes(const std::string& extension_id);

  Delegate* delegate_;
  Profile* profile_;
  GURL provider_url_;
  Mode mode_;
  gfx::Rect initial_bounds_;
  chrome::HostDesktopType host_desktop_type_;
  bool popup_shown_;
  // List of valid redirect URL prefixes.
  std::vector<std::string> valid_prefixes_;

  content::WebContents* contents_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthFlow);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
