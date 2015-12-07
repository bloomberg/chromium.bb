// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

void DidAllocateWorkerProcess(const base::Closure& quit_closure,
                              ServiceWorkerStatusCode* status_out,
                              int* process_id_out,
                              bool* is_new_process_out,
                              ServiceWorkerStatusCode status,
                              int process_id,
                              bool is_new_process) {
  *status_out = status;
  *process_id_out = process_id;
  *is_new_process_out = is_new_process;
  quit_closure.Run();
}

}  // namespace

class ServiceWorkerProcessManagerTest : public testing::Test {
 public:
  ServiceWorkerProcessManagerTest() {}

  void SetUp() override {
    process_manager_.reset(new ServiceWorkerProcessManager(NULL));
    pattern_ = GURL("http://www.example.com/");
    script_url_ = GURL("http://www.example.com/sw.js");
  }

  void TearDown() override { process_manager_.reset(); }

 protected:
  scoped_ptr<ServiceWorkerProcessManager> process_manager_;
  GURL pattern_;
  GURL script_url_;

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

TEST_F(ServiceWorkerProcessManagerTest, AllocateWorkerProcess_InShutdown) {
  ASSERT_TRUE(process_manager_->IsShutdown());

  base::RunLoop run_loop;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  int process_id = -10;
  bool is_new_process = true;
  process_manager_->AllocateWorkerProcess(
      1, pattern_, script_url_,
      base::Bind(&DidAllocateWorkerProcess, run_loop.QuitClosure(), &status,
                 &process_id, &is_new_process));
  run_loop.Run();

  // Allocating a process in shutdown should abort.
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT, status);
  EXPECT_EQ(-1, process_id);
  EXPECT_FALSE(is_new_process);
  EXPECT_TRUE(process_manager_->instance_info_.empty());
}

}  // namespace content
