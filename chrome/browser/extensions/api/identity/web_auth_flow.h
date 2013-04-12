// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_

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

// Controller class for web based auth flows. The WebAuthFlow starts
// by navigating a WebContents to a URL specificed by the caller. Any
// time the WebContents navigates to a new URL, the flow's delegate is
// notified. The delegate is expected to delete the flow when
// navigation reaches a known target URL.
//
// The WebContents is not displayed until the first page load
// completes. This allows the flow to complete without flashing a
// window on screen if the provider immediately redirects to the
// target URL.
//
// A WebAuthFlow can be started in Mode::SILENT, which never displays
// a window. If a window would be required, the flow fails.
class WebAuthFlow : public content::NotificationObserver,
                    public content::WebContentsObserver {
 public:
  enum Mode {
    INTERACTIVE,  // Show UI to the user if necessary.
    SILENT        // No UI should be shown.
  };

  enum Failure {
    WINDOW_CLOSED,  // Window closed by user.
    INTERACTION_REQUIRED  // Non-redirect page load in silent mode.
  };

  class Delegate {
   public:
    // Called when the auth flow fails. This means that the flow did not result
    // in a successful redirect to a valid redirect URL.
    virtual void OnAuthFlowFailure(Failure failure) = 0;
    // Called on redirects and other navigations to see if the URL should stop
    // the flow.
    virtual void OnAuthFlowURLChange(const GURL& redirect_url) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates an instance with the given parameters.
  // Caller owns |delegate|.
  WebAuthFlow(Delegate* delegate,
              Profile* profile,
              const GURL& provider_url,
              Mode mode,
              const gfx::Rect& initial_bounds,
              chrome::HostDesktopType host_desktop_type);
  virtual ~WebAuthFlow();

  // Starts the flow.
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

  void BeforeUrlLoaded(const GURL& url);
  void AfterUrlLoaded();

  Delegate* delegate_;
  Profile* profile_;
  GURL provider_url_;
  Mode mode_;
  gfx::Rect initial_bounds_;
  chrome::HostDesktopType host_desktop_type_;
  bool popup_shown_;

  content::WebContents* contents_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthFlow);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
