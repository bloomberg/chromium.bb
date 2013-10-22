// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"

#include "base/run_loop.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "content/public/test/test_utils.h"

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

template <typename Arg>
base::Callback<void(Arg)> CreateResultReceiver(Arg* arg_out) {
  return base::Bind(&ReceiveResult1<Arg>,
                    base::Owned(new bool(false)), arg_out);
}

// Instantiate versions we know callers will need.
template base::Callback<void(SyncStatusCode)>
AssignAndQuitCallback(base::RunLoop*, SyncStatusCode*);

#define INSTANTIATE_RECEIVER(type)                                  \
  template base::Callback<void(type)> CreateResultReceiver(type*);
INSTANTIATE_RECEIVER(SyncStatusCode);
INSTANTIATE_RECEIVER(google_apis::GDataErrorCode);
#undef INSTANTIATE_RECEIVER

}  // namespace sync_file_system
