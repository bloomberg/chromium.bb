// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_SYSLOGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_SYSLOGS_PROVIDER_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "content/browser/cancelable_request.h"

class CancelableRequestConsumerBase;

namespace chromeos {
namespace system {

typedef std::map<std::string, std::string> LogDictionaryType;

// This interface provides access to Chrome OS syslogs.
class SyslogsProvider : public CancelableRequestProvider {
 public:
  static SyslogsProvider* GetInstance();

  // The callback type used with RequestSyslogs().
  typedef base::Callback<void(LogDictionaryType*,
                              std::string*)> ReadCompleteCallback;

  // Used to specify the syslogs context with RequestSyslogs().
  enum SyslogsContext {
    SYSLOGS_FEEDBACK,
    SYSLOGS_SYSINFO,
    SYSLOGS_NETWORK,
    SYSLOGS_DEFAULT
  };

  // Request system logs. Read happens on the FILE thread and callback is
  // called on the thread this is called from (via ForwardResult).
  // Logs are owned by callback function (use delete when done with them).
  // Returns the request handle. Call CancelRequest(Handle) to cancel
  // the request before the callback gets called.
  virtual Handle RequestSyslogs(
      bool compress_logs,
      SyslogsContext context,
      CancelableRequestConsumerBase* consumer,
      const ReadCompleteCallback& callback) = 0;

 protected:
  virtual ~SyslogsProvider() {}
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_SYSLOGS_PROVIDER_H_
