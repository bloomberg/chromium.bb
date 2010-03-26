// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SYNC_ENGINE_SYNCER_COMMAND_TEST_H_
#define CHROME_TEST_SYNC_ENGINE_SYNCER_COMMAND_TEST_H_

#include <vector>

#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

// A test fixture that simplifies writing unit tests for individual
// SyncerCommands, providing convenient access to a test directory
// and a syncer session.
template<typename T>
class SyncerCommandTestWithParam : public testing::TestWithParam<T>,
                                   public sessions::SyncSession::Delegate,
                                   public ModelSafeWorkerRegistrar {
 public:
  // SyncSession::Delegate implementation.
  virtual void OnSilencedUntil(const base::TimeTicks& silenced_until) {
    FAIL() << "Should not get silenced.";
  }
  virtual bool IsSyncingCurrentlySilenced() {
    ADD_FAILURE() << "No requests for silenced state should be made.";
    return false;
  }
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) {
    FAIL() << "Should not get poll interval update.";
  }
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) {
    FAIL() << "Should not get poll interval update.";
  }

  // ModelSafeWorkerRegistrar implementation.
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) {
    std::vector<scoped_refptr<ModelSafeWorker> >::iterator it;
    for (it = workers_.begin(); it != workers_.end(); ++it)
      out->push_back(*it);
  }
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
    ModelSafeRoutingInfo copy(routing_info_);
    out->swap(copy);
  }

 protected:
  SyncerCommandTestWithParam() {}
  virtual ~SyncerCommandTestWithParam() {}
  virtual void SetUp() {
    syncdb_.SetUp();
    context_.reset(new sessions::SyncSessionContext(NULL, NULL,
        syncdb_.manager(), registrar()));
    context_->set_account_name(syncdb_.name());
  }
  virtual void TearDown() {
    syncdb_.TearDown();
  }

  const TestDirectorySetterUpper& syncdb() const { return syncdb_; }
  sessions::SyncSessionContext* context() const { return context_.get(); }
  sessions::SyncSession::Delegate* delegate() { return this; }
  ModelSafeWorkerRegistrar* registrar() { return this; }
  // Lazily create a session.
  sessions::SyncSession* session() {
    if (!session_.get())
      session_.reset(new sessions::SyncSession(context(), delegate()));
    return session_.get();
  }
  void ClearSession() {
    session_.reset();
  }

  std::vector<scoped_refptr<ModelSafeWorker> >* workers() {
    return &workers_;
  }

  const ModelSafeRoutingInfo& routing_info() {return routing_info_; }
  ModelSafeRoutingInfo* mutable_routing_info() { return &routing_info_; }

 private:
  TestDirectorySetterUpper syncdb_;
  scoped_ptr<sessions::SyncSessionContext> context_;
  scoped_ptr<sessions::SyncSession> session_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;
  ModelSafeRoutingInfo routing_info_;
  DISALLOW_COPY_AND_ASSIGN(SyncerCommandTestWithParam);
};

class SyncerCommandTest : public SyncerCommandTestWithParam<void*> {};

}  // namespace browser_sync

#endif  // CHROME_TEST_SYNC_ENGINE_SYNCER_COMMAND_TEST_H_
