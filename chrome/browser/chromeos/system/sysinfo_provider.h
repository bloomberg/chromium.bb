// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_SYSINFO_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_SYSINFO_PROVIDER_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "chrome/browser/cancelable_request.h"

class CancelableRequestConsumerBase;

namespace chromeos {
namespace system {

typedef std::map<std::string, std::string> SysInfoResponse;

// Provides CancelableRequests for fetching system info (network state, etc)
// from the ChromeOS debug daemon.
class SysInfoProvider : public CancelableRequestProvider {
 public:
  // Allocates a new SysInfoProvider and returns it.
  static SysInfoProvider* Create();
  virtual ~SysInfoProvider() {}

  typedef base::Callback<void(SysInfoResponse*)> FetchCompleteCallback;

  // Fetches system info from the debug daemon, attributing the request to
  // |consumer|. Calls |callback| when all information has been fetched.
  virtual Handle Fetch(CancelableRequestConsumerBase* consumer,
                       const FetchCompleteCallback& callback) = 0;
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_SYSINFO_PROVIDER_H_
