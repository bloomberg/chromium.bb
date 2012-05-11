// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_CAPTIVE_PORTAL_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_CAPTIVE_PORTAL_VIEW_H_
#pragma once

#include <string>
#include "chrome/browser/chromeos/login/simple_web_view_dialog.h"

namespace chromeos {

class CaptivePortalWindowProxy;

class CaptivePortalView : public SimpleWebViewDialog {
 public:
  CaptivePortalView(Profile* profile, CaptivePortalWindowProxy* proxy);
  virtual ~CaptivePortalView();

  // Starts loading.
  void StartLoad();

  // Overridden from views::WidgetDelegate:
  bool CanResize() const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;

  // Overridden from content::WebContentsDelegate:
  virtual void NavigationStateChanged(const content::WebContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;

 private:
  // Contains CaptivePortalWindowProxy to be notified when redirection state is
  // resolved.
  CaptivePortalWindowProxy* proxy_;

  bool redirected_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalView);
};

}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_CAPTIVE_PORTAL_VIEW_H_
