// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Simple system resources class that uses the current message loop
// for scheduling.  Assumes the current message loop is already
// running.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_CHROME_SYSTEM_RESOURCES_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_CHROME_SYSTEM_RESOURCES_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "google/cacheinvalidation/v2/system-resources.h"

namespace sync_notifier {

class CacheInvalidationPacketHandler;

class ChromeLogger : public invalidation::Logger {
 public:
  ChromeLogger();

  virtual ~ChromeLogger();

  // invalidation::Logger implementation.
  virtual void Log(LogLevel level, const char* file, int line,
                   const char* format, ...) OVERRIDE;
};

class ChromeScheduler : public invalidation::Scheduler {
 public:
  ChromeScheduler();

  virtual ~ChromeScheduler();

  // Start and stop the scheduler.
  void Start();
  void Stop();

  // invalidation::Scheduler implementation.
  virtual void Schedule(invalidation::TimeDelta delay,
                        invalidation::Closure* task) OVERRIDE;

  virtual bool IsRunningOnThread() const OVERRIDE;

  virtual invalidation::Time GetCurrentTime() const OVERRIDE;

 private:
  base::WeakPtrFactory<ChromeScheduler> weak_factory_;
  // Holds all posted tasks that have not yet been run.
  std::set<invalidation::Closure*> posted_tasks_;

  const MessageLoop* created_on_loop_;
  bool is_started_;
  bool is_stopped_;

  // Runs the task, deletes it, and removes it from |posted_tasks_|.
  void RunPostedTask(invalidation::Closure* task);
};

class ChromeStorage : public invalidation::Storage {
 public:
  ChromeStorage(StateWriter* state_writer, invalidation::Scheduler* scheduler);

  virtual ~ChromeStorage();

  void SetInitialState(const std::string& value) {
    cached_state_ = value;
  }

  // invalidation::Storage implementation.
  virtual void WriteKey(const std::string& key, const std::string& value,
                        invalidation::WriteKeyCallback* done) OVERRIDE;

  virtual void ReadKey(const std::string& key,
                       invalidation::ReadKeyCallback* done) OVERRIDE;

  virtual void DeleteKey(const std::string& key,
                         invalidation::DeleteKeyCallback* done) OVERRIDE;

  virtual void ReadAllKeys(
      invalidation::ReadAllKeysCallback* key_callback) OVERRIDE;

 private:
  // Runs the given storage callback with SUCCESS status and deletes it.
  void RunAndDeleteWriteKeyCallback(
      invalidation::WriteKeyCallback* callback);

  // Runs the given callback with the given value and deletes it.
  void RunAndDeleteReadKeyCallback(
      invalidation::ReadKeyCallback* callback, const std::string& value);

  StateWriter* state_writer_;
  invalidation::Scheduler* scheduler_;
  std::string cached_state_;
};

class ChromeNetwork : public invalidation::NetworkChannel {
 public:
  ChromeNetwork();

  virtual ~ChromeNetwork();

  void UpdatePacketHandler(CacheInvalidationPacketHandler* packet_handler);

  // invalidation::NetworkChannel implementation.
  virtual void SendMessage(const std::string& outgoing_message) OVERRIDE;

  virtual void SetMessageReceiver(
      invalidation::MessageCallback* incoming_receiver) OVERRIDE;

  virtual void AddNetworkStatusReceiver(
      invalidation::NetworkStatusCallback* network_status_receiver) OVERRIDE;

 private:
  void HandleInboundMessage(const std::string& incoming_message);

  CacheInvalidationPacketHandler* packet_handler_;
  scoped_ptr<invalidation::MessageCallback> incoming_receiver_;
  std::vector<invalidation::NetworkStatusCallback*> network_status_receivers_;
  base::WeakPtrFactory<ChromeNetwork> weak_factory_;
};

class ChromeSystemResources : public invalidation::SystemResources {
 public:
  explicit ChromeSystemResources(StateWriter* state_writer);

  virtual ~ChromeSystemResources();

  // invalidation::SystemResources implementation.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsStarted() const OVERRIDE;
  virtual void set_platform(const std::string& platform);
  virtual std::string platform() const OVERRIDE;
  virtual ChromeLogger* logger() OVERRIDE;
  virtual ChromeStorage* storage() OVERRIDE;
  virtual ChromeNetwork* network() OVERRIDE;
  virtual ChromeScheduler* internal_scheduler() OVERRIDE;
  virtual ChromeScheduler* listener_scheduler() OVERRIDE;

 private:
  bool is_started_;
  std::string platform_;
  scoped_ptr<ChromeLogger> logger_;
  scoped_ptr<ChromeScheduler> internal_scheduler_;
  scoped_ptr<ChromeScheduler> listener_scheduler_;
  scoped_ptr<ChromeStorage> storage_;
  scoped_ptr<ChromeNetwork> network_;
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_CHROME_SYSTEM_RESOURCES_H_
