// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "googleurl/src/gurl.h"

// This class uses the browser login credentials to fetch a channel ID for an
// app to use when sending server push notifications.
class AppNotifyChannelSetup
    : public base::RefCountedThreadSafe<AppNotifyChannelSetup> {
 public:
  class Delegate {
   public:
    // If successful, |channel_id| will be non-empty. On failure, |channel_id|
    // will be empty and |error| will contain an error to report to the JS
    // callback.
    virtual void AppNotifyChannelSetupComplete(int request_id,
                                               const std::string& channel_id,
                                               const std::string& error) = 0;
  };

  AppNotifyChannelSetup(int request_id,
                        const std::string& client_id,
                        const GURL& requestor_url,
                        base::WeakPtr<Delegate> delegate);

  // This begins the process of fetching the channel id using the browser login
  // credentials. If the user isn't logged in to chrome, this will first cause a
  // prompt to appear asking the user to log in.
  void Start();

 private:
  friend class base::RefCountedThreadSafe<AppNotifyChannelSetup>;

  virtual ~AppNotifyChannelSetup();

  void ReportResult(const std::string& channel_id, const std::string& error);

  int request_id_;
  std::string client_id_;
  GURL requestor_url_;
  base::WeakPtr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppNotifyChannelSetup);
};

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFY_CHANNEL_SETUP_H_
