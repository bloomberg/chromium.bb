// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace sync_file_system {

namespace drive_backend {
class MetadataDatabase;
}  // drive_backend

template <typename R>
void AssignAndQuit(base::RunLoop* run_loop, R* result_out, R result) {
  DCHECK(result_out);
  DCHECK(run_loop);
  *result_out = result;
  run_loop->Quit();
}

template <typename R> base::Callback<void(R)>
AssignAndQuitCallback(base::RunLoop* run_loop, R* result) {
  return base::Bind(&AssignAndQuit<R>, run_loop, base::Unretained(result));
}

template <typename Arg>
void ReceiveResult1(bool* done, Arg* arg_out, Arg arg) {
  EXPECT_FALSE(*done);
  *done = true;
  *arg_out = base::internal::CallbackForward(arg);
}

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

template <typename Arg>
base::Callback<void(Arg)> CreateResultReceiver(Arg* arg_out) {
  return base::Bind(&ReceiveResult1<Arg>,
                    base::Owned(new bool(false)), arg_out);
}

template <typename Arg1, typename Arg2>
base::Callback<void(Arg1, Arg2)> CreateResultReceiver(Arg1* arg1_out,
                                                      Arg2* arg2_out) {
  return base::Bind(&ReceiveResult2<Arg1, Arg2>,
                    base::Owned(new bool(false)),
                    arg1_out, arg2_out);
}

// Instantiate versions we know callers will need.
template base::Callback<void(SyncStatusCode)>
AssignAndQuitCallback(base::RunLoop*, SyncStatusCode*);

#define INSTANTIATE_RECEIVER(type)                                  \
  template base::Callback<void(type)> CreateResultReceiver(type*);
INSTANTIATE_RECEIVER(SyncStatusCode);
INSTANTIATE_RECEIVER(google_apis::GDataErrorCode);
#undef INSTANTIATE_RECEIVER

#define INSTANTIATE_RECEIVER(type1, type2)                \
  template base::Callback<void(type1, scoped_ptr<type2>)> \
  CreateResultReceiver(type1*, scoped_ptr<type2>*)
INSTANTIATE_RECEIVER(SyncStatusCode, drive_backend::MetadataDatabase);
INSTANTIATE_RECEIVER(google_apis::GDataErrorCode, google_apis::ResourceEntry);
#undef INSTANTIATE_RECEIVER

}  // namespace sync_file_system
