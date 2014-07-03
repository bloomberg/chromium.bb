// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"


#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistrationTest : public testing::Test {
 public:
  ServiceWorkerRegistrationTest()
      : io_thread_(BrowserThread::IO, &message_loop_) {}

  virtual void SetUp() OVERRIDE {
    context_.reset(
        new ServiceWorkerContextCore(base::FilePath(),
                                     base::MessageLoopProxy::current(),
                                     base::MessageLoopProxy::current(),
                                     NULL,
                                     NULL,
                                     NULL));
    context_ptr_ = context_->AsWeakPtr();
  }

  virtual void TearDown() OVERRIDE {
    context_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  scoped_ptr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerContextCore> context_ptr_;
  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl io_thread_;
};

}  // namespace content
