// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
#define CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/task.h"
#include "chrome/browser/tab_contents/chrome_interstitial_page.h"
#include "net/base/network_change_notifier.h"

class Extension;
class OfflineResourceHandler;
class TabContents;

namespace base {
class DictionaryValue;
}

namespace chromeos {

// OfflineLoadPage class shows the interstitial page that is shown
// when no network is available and hides when some network (either
// one of wifi, 3g or ethernet) becomes available.
// It deletes itself when the interstitial page is closed.
class OfflineLoadPage : public ChromeInterstitialPage,
                        public net::NetworkChangeNotifier::OnlineStateObserver {
 public:
  // Create a offline load page for the |tab_contents|.
  OfflineLoadPage(TabContents* tab_contents, const GURL& url,
                  OfflineResourceHandler* handler);

 protected:
  virtual ~OfflineLoadPage();

  // Overridden by tests.
  virtual void NotifyBlockingPageComplete(bool proceed);

 private:
  // ChromeInterstitialPage implementation.
  virtual std::string GetHTMLContents() OVERRIDE;
  virtual void CommandReceived(const std::string& command) OVERRIDE;
  virtual void Proceed() OVERRIDE;
  virtual void DontProceed() OVERRIDE;

  // net::NetworkChangeNotifier::OnlineStateObserver overrides.
  virtual void OnOnlineStateChanged(bool online) OVERRIDE;

  // Retrieves template strings of the offline page for app and
  // normal site.
  void GetAppOfflineStrings(const Extension* app,
                            const string16& faield_url,
                            base::DictionaryValue* strings) const;
  void GetNormalOfflineStrings(const string16& faield_url,
                               base::DictionaryValue* strings) const;

  // True if there is a mobile network is available but
  // has not been activated.
  bool ShowActivationMessage();

  scoped_refptr<OfflineResourceHandler> handler_;

  // True if the proceed is chosen.
  bool proceeded_;
  ScopedRunnableMethodFactory<OfflineLoadPage> method_factory_;

  bool in_test_;

  DISALLOW_COPY_AND_ASSIGN(OfflineLoadPage);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
