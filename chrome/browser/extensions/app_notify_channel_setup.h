// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

class Profile;

// This class uses the browser login credentials to fetch a channel ID for an
// app to use when sending server push notifications.
class AppNotifyChannelSetup
    : public URLFetcher::Delegate,
      public base::RefCountedThreadSafe<AppNotifyChannelSetup> {
 public:
  class Delegate {
   public:
    // If successful, |channel_id| will be non-empty. On failure, |channel_id|
    // will be empty and |error| will contain an error to report to the JS
    // callback.
    virtual void AppNotifyChannelSetupComplete(const std::string& channel_id,
                                               const std::string& error,
                                               int return_route_id,
                                               int callback_id) = 0;
  };

  AppNotifyChannelSetup(Profile* profile,
                        const std::string& client_id,
                        const GURL& requestor_url,
                        int return_route_id,
                        int callback_id,
                        base::WeakPtr<Delegate> delegate);

  // This begins the process of fetching the channel id using the browser login
  // credentials. If the user isn't logged in to chrome, this will first cause a
  // prompt to appear asking the user to log in.
  void Start();

 protected:
  // URLFetcher::Delegate.
  virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<AppNotifyChannelSetup>;

  virtual ~AppNotifyChannelSetup();

  void ReportResult(const std::string& channel_id, const std::string& error);

  Profile* profile_;
  std::string client_id_;
  GURL requestor_url_;
  int return_route_id_;
  int callback_id_;
  base::WeakPtr<Delegate> delegate_;
  scoped_ptr<URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AppNotifyChannelSetup);
};

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_
