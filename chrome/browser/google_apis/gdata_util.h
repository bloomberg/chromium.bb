// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_GDATA_UTIL_H_
#define CHROME_BROWSER_GOOGLE_APIS_GDATA_UTIL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/task_runner_util.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class FilePath;
class Profile;

namespace base {
class SequencedTaskRunner;
class Time;
}  // namespace base

namespace tracked_objects {
class Location;
}  // tracked_objects

namespace google_apis {
namespace util {

// Returns true if Drive v2 API is enabled via commandline switch.
bool IsDriveV2ApiEnabled();

// Parses an RFC 3339 date/time into a base::Time, returning true on success.
// The time string must be in the format "yyyy-mm-ddThh:mm:ss.dddTZ" (TZ is
// either '+hh:mm', '-hh:mm', 'Z' (representing UTC), or an empty string).
bool GetTimeFromString(const base::StringPiece& raw_value, base::Time* time);

// Formats a base::Time as an RFC 3339 date/time (in UTC).
std::string FormatTimeAsString(const base::Time& time);
// Formats a base::Time as an RFC 3339 date/time (in localtime).
std::string FormatTimeAsStringLocaltime(const base::Time& time);

// Wrapper around BrowserThread::PostTask to post a task to the blocking
// pool with the given sequence token.
void PostBlockingPoolSequencedTask(
    const tracked_objects::Location& from_here,
    base::SequencedTaskRunner* blocking_task_runner,
    const base::Closure& task);

// Similar to PostBlockingPoolSequencedTask() but this one takes a reply
// callback that runs on the calling thread.
void PostBlockingPoolSequencedTaskAndReply(
    const tracked_objects::Location& from_here,
    base::SequencedTaskRunner* blocking_task_runner,
    const base::Closure& request_task,
    const base::Closure& reply_task);

// Similar to PostBlockingPoolSequencedTaskAndReply() but this one runs the
// reply callback with the return value of the request task.
template <typename ReturnType>
void PostBlockingPoolSequencedTaskAndReplyWithResult(
    const tracked_objects::Location& from_here,
    base::SequencedTaskRunner* blocking_task_runner,
    const base::Callback<ReturnType(void)>& request_task,
    const base::Callback<void(ReturnType)>& reply_task) {
  const bool posted = base::PostTaskAndReplyWithResult(blocking_task_runner,
                                                       from_here,
                                                       request_task,
                                                       reply_task);
  DCHECK(posted);
}

}  // namespace util
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_UTIL_H_
