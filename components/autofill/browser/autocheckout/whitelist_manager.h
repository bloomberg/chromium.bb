// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_WHITELIST_MANAGER_H_
#define COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_WHITELIST_MANAGER_H_

#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "base/timer.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace autofill {
namespace autocheckout {

// Downloads and caches the list of URL prefixes whitelisted for use with
// Autocheckout.
class WhitelistManager : public net::URLFetcherDelegate,
                         public base::SupportsUserData::Data {
 public:
  static WhitelistManager* GetForBrowserContext(
      content::BrowserContext* context);

  // Matches the url with whitelist and return the matched url prefix.
  // Returns empty string when it is not matched.
  std::string GetMatchedURLPrefix(const GURL& url) const;

 protected:
  explicit WhitelistManager(net::URLRequestContextGetter* context_getter);
  virtual ~WhitelistManager();

  // Schedules a future call to TriggerDownload if one isn't already pending.
  virtual void ScheduleDownload(size_t interval_seconds);

  // Start the download timer. It is called by ScheduleDownload(), and exposed
  // as a separate method for mocking out in tests.
  virtual void StartDownloadTimer(size_t interval_seconds);

  // Timer callback indicating it's time to download whitelist from server.
  void TriggerDownload();

  // Used by tests only.
  void StopDownloadTimer();

  const std::vector<std::string>& url_prefixes() const {
    return url_prefixes_;
  }

 private:
  // Implements net::URLFetcherDelegate.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Parse whitelist data and build whitelist.
  void BuildWhitelist(const std::string& data);

  // A list of whitelisted url prefixes.
  std::vector<std::string> url_prefixes_;

  base::OneShotTimer<WhitelistManager> download_timer_;

  // Indicates that the last triggered download hasn't resolved yet.
  bool callback_is_pending_;

  // The context for the request.
  net::URLRequestContextGetter* const context_getter_;  // WEAK

  // State of the kEnableExperimentalFormFilling flag.
  const bool experimental_form_filling_enabled_;

  // State of the kBypassAutocheckoutWhitelist flag.
  const bool bypass_autocheckout_whitelist_;

  // The request object.
  scoped_ptr<net::URLFetcher> request_;

  DISALLOW_COPY_AND_ASSIGN(WhitelistManager);
};

}  // namespace autocheckout
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_AUTOCHECKOUT_WHITELIST_MANAGER_H_

