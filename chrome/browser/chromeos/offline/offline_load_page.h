// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
#define CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/network_state_notifier.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_service.h"

class TabContents;

namespace chromeos {

// OfflineLoadPage class shows the interstitial page that is shown
// when no network is available and hides when some network (either
// one of wifi, 3g or ethernet) becomes available.
// It deletes itself when the interstitial page is closed.
class OfflineLoadPage : public InterstitialPage {
 public:
  // A delegate class that is called when the interstitinal page
  // is closed.
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    // Called when a user selected to proceed or not to proceed
    // with loading.
    virtual void OnBlockingPageComplete(bool proceed) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };
  static void Show(int process_host_id, int render_view_id,
                   const GURL& url, Delegate* delegate);
  // Import show here so that overloading works.
  using InterstitialPage::Show;

 protected:
  // Create a offline load page for the |tab_contents|.
  OfflineLoadPage(TabContents* tab_contents, const GURL& url,
                  Delegate* delegate);
  virtual ~OfflineLoadPage() {}

 private:
  // InterstitialPage implementation.
  virtual std::string GetHTMLContents();
  virtual void CommandReceived(const std::string& command);
  virtual void Proceed();
  virtual void DontProceed();

  // Overrides InterstitialPage's Observe.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Delegate* delegate_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(OfflineLoadPage);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
