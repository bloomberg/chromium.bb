// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
#define CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
#pragma once

#include <string>

#include "base/task.h"
#include "chrome/browser/chromeos/network_state_notifier.h"
#include "content/browser/tab_contents/interstitial_page.h"

class DictionaryValue;
class Extension;
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

  // Only for testing.
  void EnableTest() {
    in_test_ = true;
  }

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

  // Retrieves template strings of the offline page for app and
  // normal site.
  void GetAppOfflineStrings(const Extension* app,
                            const string16& faield_url,
                            DictionaryValue* strings) const;
  void GetNormalOfflineStrings(const string16& faield_url,
                               DictionaryValue* strings) const;

  // True if there is a mobile network is available but
  // has not been activated.
  bool ShowActivationMessage();

  Delegate* delegate_;
  NotificationRegistrar registrar_;

  // True if the proceed is chosen.
  bool proceeded_;
  ScopedRunnableMethodFactory<OfflineLoadPage> method_factory_;

  bool in_test_;

  DISALLOW_COPY_AND_ASSIGN(OfflineLoadPage);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
