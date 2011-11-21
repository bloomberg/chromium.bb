// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BENCHMARKING_MESSAGE_FILTER_H_
#define CHROME_BROWSER_CHROME_BENCHMARKING_MESSAGE_FILTER_H_
#pragma once

#include "content/browser/browser_message_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"

namespace net {
class URLRequestContextGetter;
}

class Profile;

// This class filters out incoming Chrome-specific benchmarking IPC messages
// for the renderer process on the IPC thread.
class ChromeBenchmarkingMessageFilter : public BrowserMessageFilter {
 public:
  ChromeBenchmarkingMessageFilter(
      int render_process_id,
      Profile* profile,
      net::URLRequestContextGetter* request_context);

  // BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  virtual ~ChromeBenchmarkingMessageFilter();

  // Message handlers.
  void OnCloseCurrentConnections();
  void OnClearCache(bool preserve_ssl_host_info, IPC::Message* reply_msg);
  void OnClearHostResolverCache(int* result);
  void OnEnableSpdy(bool enable);
  void OnSetCacheMode(bool enabled);
  void OnClearPredictorCache(int* result);

  // Returns true if benchmarking is enabled for chrome.
  bool CheckBenchmarkingEnabled() const;

  int render_process_id_;

  // The Profile associated with our renderer process.  This should only be
  // accessed on the UI thread!
  Profile* profile_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBenchmarkingMessageFilter);
};

#endif  // CHROME_BROWSER_CHROME_BENCHMARKING_MESSAGE_FILTER_H_

