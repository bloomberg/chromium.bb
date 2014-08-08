// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SSL_SSL_BLOCKING_PAGE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace content {
class InterstitialPage;
class WebContents;
}

#if defined(ENABLE_EXTENSIONS)
namespace extensions {
class ExperienceSamplingEvent;
}
#endif

// This class is responsible for showing/hiding the interstitial page that is
// shown when a certificate error happens.
// It deletes itself when the interstitial page is closed.
//
// This class should only be used on the UI thread because its implementation
// uses captive_portal::CaptivePortalService which can only be accessed on the
// UI thread.
class SSLBlockingPage : public content::InterstitialPageDelegate,
                        public content::NotificationObserver {
 public:
  // These represent the commands sent from the interstitial JavaScript. They
  // are defined in chrome/browser/resources/ssl/ssl_errors_common.js.
  // DO NOT reorder or change these without also changing the JavaScript!
  enum SSLBlockingPageCommands {
    CMD_DONT_PROCEED = 0,
    CMD_PROCEED = 1,
    CMD_MORE = 2,
    CMD_RELOAD = 3,
    CMD_HELP = 4,
    CMD_CLOCK = 5
  };

  virtual ~SSLBlockingPage();

  // Create an interstitial and show it.
  void Show();

  // Creates an SSL blocking page. If the blocking page isn't shown, the caller
  // is responsible for cleaning up the blocking page, otherwise the
  // interstitial takes ownership when shown.
  SSLBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool overridable,
      bool strict_enforcement,
      const base::Callback<void(bool)>& callback);

  // A method that sets strings in the specified dictionary from the passed
  // vector so that they can be used to resource the ssl_roadblock.html/
  // ssl_error.html files.
  // Note: there can be up to 5 strings in |extra_info|.
  static void SetExtraInfo(base::DictionaryValue* strings,
                           const std::vector<base::string16>& extra_info);

 protected:
  // InterstitialPageDelegate implementation.
  virtual std::string GetHTMLContents() OVERRIDE;
  virtual void CommandReceived(const std::string& command) OVERRIDE;
  virtual void OverrideEntry(content::NavigationEntry* entry) OVERRIDE;
  virtual void OverrideRendererPrefs(
      content::RendererPreferences* prefs) OVERRIDE;
  virtual void OnProceed() OVERRIDE;
  virtual void OnDontProceed() OVERRIDE;

 private:
  void NotifyDenyCertificate();
  void NotifyAllowCertificate();

  // Used to query the HistoryService to see if the URL is in history. For UMA.
  void OnGotHistoryCount(bool success, int num_visits, base::Time first_visit);

  // content::NotificationObserver:
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

  base::Callback<void(bool)> callback_;

  content::WebContents* web_contents_;
  int cert_error_;
  const net::SSLInfo ssl_info_;
  GURL request_url_;
  // Could the user successfully override the error?
  bool overridable_;
  // Has the site requested strict enforcement of certificate errors?
  bool strict_enforcement_;
  content::InterstitialPage* interstitial_page_;  // Owns us.
  // Is the hostname for an internal network?
  bool internal_;
  // How many times is this same URL in history?
  int num_visits_;
  // Used for getting num_visits_.
  base::CancelableTaskTracker request_tracker_;
  // Is captive portal detection enabled?
  bool captive_portal_detection_enabled_;
  // Did the probe complete before the interstitial was closed?
  bool captive_portal_probe_completed_;
  // Did the captive portal probe receive an error or get a non-HTTP response?
  bool captive_portal_no_response_;
  // Was a captive portal detected?
  bool captive_portal_detected_;

  // For the FieldTrial: this contains the name of the condition.
  std::string trial_condition_;

#if defined(ENABLE_EXTENSIONS)
  // For Chrome Experience Sampling Platform: this maintains event state.
  scoped_ptr<extensions::ExperienceSamplingEvent> sampling_event_;
#endif

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLBlockingPage);
};

#endif  // CHROME_BROWSER_SSL_SSL_BLOCKING_PAGE_H_
