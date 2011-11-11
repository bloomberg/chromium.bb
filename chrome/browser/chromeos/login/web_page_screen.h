// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEB_PAGE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEB_PAGE_SCREEN_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"

class GURL;

namespace chromeos {

// Base class for wizard screen that holds web page.
class WebPageScreen : public TabContentsDelegate {
 public:
  explicit WebPageScreen();
  virtual ~WebPageScreen();

  // Exits from the screen with the specified exit code.
  virtual void CloseScreen(ScreenObserver::ExitCodes code) = 0;

 protected:
  // TabContentsDelegate implementation:
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      content::NavigationType navigation_type) OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;

  // Called by |timeout_timer_|. Stops page fetching and closes screen.
  virtual void OnNetworkTimeout();

  // Start/stop timeout timer.
  void StartTimeoutTimer();
  void StopTimeoutTimer();

 private:
  // Timer used for network response timeout.
  base::OneShotTimer<WebPageScreen> timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(WebPageScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEB_PAGE_SCREEN_H_
