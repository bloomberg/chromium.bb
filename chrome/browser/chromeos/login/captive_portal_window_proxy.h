// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_CAPTIVE_PORTAL_WINDOW_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_CAPTIVE_PORTAL_WINDOW_PROXY_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

namespace chromeos {

class CaptivePortalView;

// Delegate interface for CaptivePortalWindowProxy.
class CaptivePortalWindowProxyDelegate {
 public:
  // Called when a captive portal is detected.
  virtual void OnPortalDetected() = 0;

 protected:
  virtual ~CaptivePortalWindowProxyDelegate() {}
};

// Proxy which manages showing of the window for CaptivePortal sign-in.
class CaptivePortalWindowProxy : public views::WidgetObserver {
 public:
  typedef CaptivePortalWindowProxyDelegate Delegate;

  CaptivePortalWindowProxy(Delegate* delegate, gfx::NativeWindow parent);
  virtual ~CaptivePortalWindowProxy();

  // Shows captive portal window only after a redirection has happened. So it is
  // safe to call this method, when the caller isn't 100% sure that the network
  // is in the captive portal state.
  // Subsequent call to this method would reuses existing view
  // but reloads test page (generate_204).
  void ShowIfRedirected();

  // Forces captive portal window show.
  void Show();

  // Closes the window.
  void Close();

  // Called by CaptivePortalView when URL loading was redirected from the
  // original URL.
  void OnRedirected();

  // Called by CaptivePortalView when origin URL is loaded without any
  // redirections.
  void OnOriginalURLLoaded();

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

 private:
  Delegate* delegate_;
  views::Widget* widget_;
  scoped_ptr<CaptivePortalView> captive_portal_view_;
  gfx::NativeWindow parent_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalWindowProxy);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_CAPTIVE_PORTAL_WINDOW_PROXY_H_
