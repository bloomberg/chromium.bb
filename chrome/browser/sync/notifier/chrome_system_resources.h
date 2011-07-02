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

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "google/cacheinvalidation/v2/system-resources.h"

namespace sync_notifier {

class CacheInvalidationPacketHandler;

class ChromeLogger : public invalidation::Logger {
 public:
  ChromeLogger();

  virtual ~ChromeLogger();

  virtual void Log(LogLevel level, const char* file, int line,
                   const char* format, ...);
};

class ChromeScheduler : public invalidation::Scheduler {
 public:
  ChromeScheduler();

  virtual ~ChromeScheduler();

  // Start and stop the scheduler.
  virtual void Start();
  virtual void Stop();

  // Overrides.
  virtual void Schedule(invalidation::TimeDelta delay,
                        invalidation::Closure* task);

  virtual bool IsRunningOnThread();

  virtual invalidation::Time GetCurrentTime();

 private:
  scoped_ptr<ScopedRunnableMethodFactory<ChromeScheduler> >
      scoped_runnable_method_factory_;
  // Holds all posted tasks that have not yet been run.
  std::set<invalidation::Closure*> posted_tasks_;

  // TODO(tim): Trying to debug bug crbug.com/64652.
  const MessageLoop* created_on_loop_;
  bool is_started_;
  bool is_stopped_;

  // If the scheduler has been started, inserts |task| into
  // |posted_tasks_| and returns a Task* to post.  Otherwise,
  // immediately deletes |task| and returns NULL.
  Task* MakeTaskToPost(invalidation::Closure* task);

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

  // invalidation::Storage overrides.
  virtual void WriteKey(const std::string& key, const std::string& value,
                        invalidation::WriteKeyCallback* done);

  virtual void ReadKey(const std::string& key,
                       invalidation::ReadKeyCallback* done);

  virtual void DeleteKey(const std::string& key,
                         invalidation::DeleteKeyCallback* done);

  virtual void ReadAllKeys(invalidation::ReadAllKeysCallback* key_callback);

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

  // Overrides.
  virtual void SendMessage(const std::string& outgoing_message);

  virtual void SetMessageReceiver(
      invalidation::MessageCallback* incoming_receiver);

  virtual void AddNetworkStatusReceiver(
      invalidation::NetworkStatusCallback* network_status_receiver);

 private:
  void HandleInboundMessage(const std::string& incoming_message);

  CacheInvalidationPacketHandler* packet_handler_;
  scoped_ptr<invalidation::MessageCallback> incoming_receiver_;
  std::vector<invalidation::NetworkStatusCallback*> network_status_receivers_;
};

class ChromeSystemResources : public invalidation::SystemResources {
 public:
  explicit ChromeSystemResources(StateWriter* state_writer);

  virtual ~ChromeSystemResources();

  // invalidation::SystemResources implementation.
  virtual void Start();
  virtual void Stop();
  virtual bool IsStarted();
  virtual void set_platform(const std::string& platform);
  virtual std::string platform();
  virtual ChromeLogger* logger();
  virtual ChromeStorage* storage();
  virtual ChromeNetwork* network();
  virtual ChromeScheduler* internal_scheduler();
  virtual ChromeScheduler* listener_scheduler();

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
