// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_NET_BENCHMARKING_MESSAGE_FILTER_H_
#define CHROME_BROWSER_CHROME_NET_BENCHMARKING_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace net {
class URLRequestContextGetter;
}

class Profile;

// This class filters out incoming Chrome-specific benchmarking IPC messages
// for the renderer process on the IPC thread.
class ChromeNetBenchmarkingMessageFilter
    : public content::BrowserMessageFilter {
 public:
  ChromeNetBenchmarkingMessageFilter(
      Profile* profile,
      net::URLRequestContextGetter* request_context);

  // content::BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;


 private:
  ~ChromeNetBenchmarkingMessageFilter() override;

  // Message handlers.
  void OnCloseCurrentConnections();
  void OnClearCache(IPC::Message* reply_msg);
  void OnClearHostResolverCache();
  void OnSetCacheMode(bool enabled);
  void OnClearPredictorCache();

  // Returns true if benchmarking is enabled for chrome.
  bool CheckBenchmarkingEnabled() const;

  // The Profile associated with our renderer process.  This should only be
  // accessed on the UI thread!
  // TODO(623967): Store the Predictor* here instead of the Profile.
  Profile* profile_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetBenchmarkingMessageFilter);
};

#endif  // CHROME_BROWSER_CHROME_NET_BENCHMARKING_MESSAGE_FILTER_H_

