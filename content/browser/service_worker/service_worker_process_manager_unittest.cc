// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerProcessManagerTest : public testing::Test {
 public:
  ServiceWorkerProcessManagerTest() {}

  virtual void SetUp() OVERRIDE {
    process_manager_.reset(new ServiceWorkerProcessManager(NULL));
    pattern_ = GURL("http://www.example.com/");
  }

  virtual void TearDown() OVERRIDE {
    process_manager_.reset();
  }

 protected:
  scoped_ptr<ServiceWorkerProcessManager> process_manager_;
  GURL pattern_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProcessManagerTest);
};

TEST_F(ServiceWorkerProcessManagerTest, SortProcess) {
  // Process 1 has 2 ref, 2 has 3 refs and 3 has 1 refs.
  process_manager_->AddProcessReferenceToPattern(pattern_, 1);
  process_manager_->AddProcessReferenceToPattern(pattern_, 1);
  process_manager_->AddProcessReferenceToPattern(pattern_, 2);
  process_manager_->AddProcessReferenceToPattern(pattern_, 2);
  process_manager_->AddProcessReferenceToPattern(pattern_, 2);
  process_manager_->AddProcessReferenceToPattern(pattern_, 3);

  // Process 2 has the biggest # of references and it should be chosen.
  EXPECT_THAT(process_manager_->SortProcessesForPattern(pattern_),
              testing::ElementsAre(2, 1, 3));

  process_manager_->RemoveProcessReferenceFromPattern(pattern_, 1);
  process_manager_->RemoveProcessReferenceFromPattern(pattern_, 1);
  // Scores for each process: 2 : 3, 3 : 1, process 1 is removed.
  EXPECT_THAT(process_manager_->SortProcessesForPattern(pattern_),
              testing::ElementsAre(2, 3));
}

}  // namespace content
