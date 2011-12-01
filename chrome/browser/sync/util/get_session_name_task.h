// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_GET_SESSION_NAME_TASK_H_
#define CHROME_BROWSER_SYNC_UTIL_GET_SESSION_NAME_TASK_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace browser_sync {

class GetSessionNameTask
    : public base::RefCountedThreadSafe<GetSessionNameTask> {
 public:
  typedef base::Callback<void(const std::string& name)>
      OnSessionNameInitialized;

  explicit GetSessionNameTask(const OnSessionNameInitialized& callback);
  virtual ~GetSessionNameTask();

  void GetSessionNameAsync();

 private:
  friend class base::RefCountedThreadSafe<GetSessionNameTask>;

  FRIEND_TEST_ALL_PREFIXES(GetSessionNameTaskTest, GetHardwareModelName);
  FRIEND_TEST_ALL_PREFIXES(GetSessionNameTaskTest, GetComputerName);

  void InvokeCallback(const std::string& session_name);

#if defined(OS_MACOSX)
  // Returns the Hardware model name, without trailing numbers, if possible.
  // See http://www.cocoadev.com/index.pl?MacintoshModels for an example list of
  // models. If an error occurs trying to read the model, this simply returns
  // "Unknown".
  static std::string GetHardwareModelName();
#endif

#if defined(OS_WIN)
  // Returns the computer name or the empty string if an error occured.
  static std::string GetComputerName();
#endif

  const OnSessionNameInitialized callback_;
  const scoped_refptr<base::MessageLoopProxy> thread_;

  DISALLOW_COPY_AND_ASSIGN(GetSessionNameTask);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_GET_SESSION_NAME_TASK_H__
