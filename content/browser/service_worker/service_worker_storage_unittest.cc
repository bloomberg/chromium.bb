// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerStorageTest : public testing::Test {
 public:
  ServiceWorkerStorageTest() {}

  virtual void SetUp() OVERRIDE {
    storage_.reset(new ServiceWorkerStorage(base::FilePath(), NULL));
  }

  virtual void TearDown() OVERRIDE { storage_.reset(); }

 protected:
  scoped_ptr<ServiceWorkerStorage> storage_;
};

}  // namespace content
