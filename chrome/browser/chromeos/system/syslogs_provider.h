// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_SYSLOGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_SYSLOGS_PROVIDER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/task/cancelable_task_tracker.h"

namespace chromeos {
namespace system {

// Maximum number of bytes in system info log chunk to be still included
// in product specific data.
extern const size_t kFeedbackMaxLength;

// Maximum number of lines in system info log chunk to be still included
// in product specific data.
extern const size_t kFeedbackMaxLineCount;

typedef std::map<std::string, std::string> LogDictionaryType;

// This interface provides access to Chrome OS syslogs.
class SyslogsProvider {
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
  // called on the thread this is called from. Logs are owned by callback
  // function (use delete when done with them).
  // Call base::CancelableTaskTracker::TryCancel() with the returned
  // task ID to cancel
  // task and callback.
  virtual base::CancelableTaskTracker::TaskId RequestSyslogs(
      bool compress_logs,
      SyslogsContext context,
      const ReadCompleteCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

 protected:
  virtual ~SyslogsProvider() {}
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_SYSLOGS_PROVIDER_H_
