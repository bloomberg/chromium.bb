// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_ACCESS_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_ACCESS_H_
#pragma once

#include <string>

#include "base/callback_old.h"
#include "content/browser/cancelable_request.h"
#include "unicode/timezone.h"

class CancelableRequestConsumerBase;

namespace chromeos {

typedef std::map<std::string, std::string> LogDictionaryType;

// This interface provides access to Chrome OS system APIs such as the
// timezone setting.
class SystemAccess : public CancelableRequestProvider {
 public:
  class Observer {
   public:
    // Called when the timezone has changed. |timezone| is non-null.
    virtual void TimezoneChanged(const icu::TimeZone& timezone) = 0;
  };

  static SystemAccess* GetInstance();

  // Returns the current timezone as an icu::Timezone object.
  virtual const icu::TimeZone& GetTimezone() = 0;

  // Sets the current timezone. |timezone| must be non-null.
  virtual void SetTimezone(const icu::TimeZone& timezone) = 0;

  // Retrieve the named machine statistic (e.g. "hardware_class").
  // This does not update the statistcs. If the |name| is not set, |result|
  // preserves old value.
  virtual bool GetMachineStatistic(const std::string& name,
                                   std::string* result) = 0;

  // The callback type used with RequestSyslogs().
  typedef Callback2<LogDictionaryType*,
                    std::string*>::Type ReadCompleteCallback;

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
      ReadCompleteCallback* callback) = 0;

  // The observer is used to monitor timezone changes.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  virtual ~SystemAccess() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_ACCESS_H_
