// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/dummy_blimp_client_context.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"

#if defined(OS_ANDROID)
#include "blimp/client/core/android/dummy_blimp_client_context_android.h"
#endif  // OS_ANDROID

namespace blimp {
namespace client {

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/blimp_client_context_impl.cc should be linked in to
// any binary using BlimpClientContext::Create.
// static
BlimpClientContext* BlimpClientContext::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner) {
#if defined(OS_ANDROID)
  return new DummyBlimpClientContextAndroid();
#else
  return new DummyBlimpClientContext();
#endif  // defined(OS_ANDROID)
}

DummyBlimpClientContext::DummyBlimpClientContext() : BlimpClientContext() {}

DummyBlimpClientContext::~DummyBlimpClientContext() {}

void DummyBlimpClientContext::SetDelegate(
    BlimpClientContextDelegate* delegate) {}

std::unique_ptr<BlimpContents> DummyBlimpClientContext::CreateBlimpContents() {
  return nullptr;
}

void DummyBlimpClientContext::Connect() {
  NOTREACHED();
}

}  // namespace client
}  // namespace blimp
