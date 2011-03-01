// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SYSLOGS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SYSLOGS_LIBRARY_H_
#pragma once

#include "base/singleton.h"
#include "content/browser/cancelable_request.h"
#include "third_party/cros/chromeos_syslogs.h"

class CancelableRequestConsumerBase;

namespace chromeos {

// This interface defines interaction with the ChromeOS syslogs APIs.
class SyslogsLibrary : public CancelableRequestProvider {
 public:
  typedef Callback2<LogDictionaryType*,
                    std::string*>::Type ReadCompleteCallback;

  SyslogsLibrary() {}
  virtual ~SyslogsLibrary() {}

  // Request system logs. Read happens on the FILE thread and callback is
  // called on the thread this is called from (via ForwardResult).
  // Logs are owned by callback function (use delete when done with them).
  // Returns the request handle. Call CancelRequest(Handle) to cancel
  // the request before the callback gets called.
  virtual Handle RequestSyslogs(
      bool compress_logs, bool feedback_context,
      CancelableRequestConsumerBase* consumer,
      ReadCompleteCallback* callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static SyslogsLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SYSLOGS_LIBRARY_H_
