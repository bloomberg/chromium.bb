// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_H_
#define CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class ThreadChecker;
}

namespace content {
class BrowserContext;
}

namespace net {
class URLRequestContextGetter;
}

namespace chromecast {

class CastService {
 public:
  // A callback that will be invoked when the user changes the opt-in stats
  // value.
  typedef base::Callback<void(bool)> OptInStatsChangedCallback;

  // Create() takes a separate url request context getter because the request
  // context getter obtained through the browser context might not be
  // appropriate for the url requests made by the cast service/reciever.
  // For example, on Chromecast, it is needed to pass in a system url request
  // context getter that would set the request context for NSS, which the main
  // getter doesn't do.
  static CastService* Create(
      content::BrowserContext* browser_context,
      net::URLRequestContextGetter* request_context_getter,
      const OptInStatsChangedCallback& opt_in_stats_callback);

  virtual ~CastService();

  // Start/stop the cast service.
  void Start();
  void Stop();

 protected:
  CastService(content::BrowserContext* browser_context,
              const OptInStatsChangedCallback& opt_in_stats_callback);
  virtual void Initialize() = 0;

  // Implementation-specific start/stop behavior.
  virtual void StartInternal() = 0;
  virtual void StopInternal() = 0;

  content::BrowserContext* browser_context() const { return browser_context_; }
  const OptInStatsChangedCallback& opt_in_stats_callback() const {
    return opt_in_stats_callback_;
  }

 private:
  content::BrowserContext* const browser_context_;
  const OptInStatsChangedCallback opt_in_stats_callback_;
  bool stopped_;
  const scoped_ptr<base::ThreadChecker> thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CastService);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_H_
