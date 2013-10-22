// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class RunLoop;
}

namespace sync_file_system {

template <typename Arg1, typename Arg2>
void ReceiveResult2(bool* done,
                    Arg1* arg1_out,
                    Arg2* arg2_out,
                    Arg1 arg1,
                    Arg2 arg2) {
  EXPECT_FALSE(*done);
  *done = true;
  *arg1_out = base::internal::CallbackForward(arg1);
  *arg2_out = base::internal::CallbackForward(arg2);
}

template <typename R>
void AssignAndQuit(base::RunLoop* run_loop, R* result_out, R result);

template <typename R> base::Callback<void(R)>
AssignAndQuitCallback(base::RunLoop* run_loop, R* result);

template <typename Arg>
base::Callback<void(Arg)> CreateResultReceiver(Arg* arg_out);

template <typename Arg1, typename Arg2>
base::Callback<void(Arg1, Arg2)> CreateResultReceiver(Arg1* arg1_out,
                                                      Arg2* arg2_out) {
  return base::Bind(&ReceiveResult2<Arg1, Arg2>,
                    base::Owned(new bool(false)),
                    arg1_out, arg2_out);
}

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_
