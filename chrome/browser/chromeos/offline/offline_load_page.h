// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
#define CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "googleurl/src/gurl.h"
#include "net/base/network_change_notifier.h"

namespace base {
class DictionaryValue;
}

namespace content {
class InterstitialPage;
class WebContents;
}

namespace extensions {
class Extension;
}

namespace chromeos {

// OfflineLoadPage class shows the interstitial page that is shown
// when no network is available and hides when some network (either
// one of wifi, 3g or ethernet) becomes available.
// It deletes itself when the interstitial page is closed.
class OfflineLoadPage
    : public content::InterstitialPageDelegate,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // Passed a boolean indicating whether or not it is OK to proceed with the
  // page load.
  typedef base::Callback<void(bool /*proceed*/)> CompletionCallback;

  // Create a offline load page for the |web_contents|.  The callback will be
  // run on the IO thread.
  OfflineLoadPage(content::WebContents* web_contents, const GURL& url,
                  const CompletionCallback& callback);

  void Show();

 protected:
  virtual ~OfflineLoadPage();

  // Overridden by tests.
  virtual void NotifyBlockingPageComplete(bool proceed);

 private:
  friend class TestOfflineLoadPage;

  // InterstitialPageDelegate implementation.
  virtual std::string GetHTMLContents() OVERRIDE;
  virtual void CommandReceived(const std::string& command) OVERRIDE;
  virtual void OverrideRendererPrefs(
      content::RendererPreferences* prefs) OVERRIDE;
  virtual void OnProceed() OVERRIDE;
  virtual void OnDontProceed() OVERRIDE;

  // net::NetworkChangeNotifier::ConnectionTypeObserver overrides.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // Retrieves template strings of the offline page for app and
  // normal site.
  void GetAppOfflineStrings(const extensions::Extension* app,
                            base::DictionaryValue* strings) const;
  void GetNormalOfflineStrings(base::DictionaryValue* strings) const;

  // True if there is a mobile network is available but
  // has not been activated.
  bool ShowActivationMessage();

  CompletionCallback callback_;

  // True if the proceed is chosen.
  bool proceeded_;

  content::WebContents* web_contents_;
  GURL url_;
  content::InterstitialPage* interstitial_page_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(OfflineLoadPage);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OFFLINE_OFFLINE_LOAD_PAGE_H_
