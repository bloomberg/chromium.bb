// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_system_resources.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "google/cacheinvalidation/v2/types.h"

namespace sync_notifier {

ChromeLogger::ChromeLogger() {}
ChromeLogger::~ChromeLogger() {}

void ChromeLogger::Log(LogLevel level, const char* file, int line,
                       const char* format, ...) {
  logging::LogSeverity log_severity = logging::LOG_INFO;
  switch (level) {
    case FINE_LEVEL:
      log_severity = logging::LOG_VERBOSE;
      break;
    case INFO_LEVEL:
      log_severity = logging::LOG_INFO;
      break;
    case WARNING_LEVEL:
      log_severity = logging::LOG_WARNING;
      break;
    case SEVERE_LEVEL:
      log_severity = logging::LOG_ERROR;
      break;
  }
  // We treat LOG(INFO) as VLOG(1).
  if ((log_severity >= logging::GetMinLogLevel()) &&
      ((log_severity != logging::LOG_INFO) ||
      (1 <= logging::GetVlogLevelHelper(file, ::strlen(file))))) {
    va_list ap;
    va_start(ap, format);
    std::string result;
    base::StringAppendV(&result, format, ap);
    logging::LogMessage(file, line, log_severity).stream() << result;
    va_end(ap);
  }
}

ChromeScheduler::ChromeScheduler()
    : created_on_loop_(MessageLoop::current()),
      is_started_(false),
      is_stopped_(false) {
  CHECK(created_on_loop_);
}

ChromeScheduler::~ChromeScheduler() {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  CHECK(is_stopped_);
}

void ChromeScheduler::Start() {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  CHECK(!is_started_);
  is_started_ = true;
  is_stopped_ = false;
  scoped_runnable_method_factory_.reset(
      new ScopedRunnableMethodFactory<ChromeScheduler>(this));
}

void ChromeScheduler::Stop() {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  is_stopped_ = true;
  is_started_ = false;
  scoped_runnable_method_factory_.reset();
  STLDeleteElements(&posted_tasks_);
  posted_tasks_.clear();
}

void ChromeScheduler::Schedule(
    invalidation::TimeDelta delay,
    invalidation::Closure* task) {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  Task* task_to_post = MakeTaskToPost(task);
  if (!task_to_post) {
    return;
  }
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, task_to_post, delay.InMillisecondsRoundedUp());
}

bool ChromeScheduler::IsRunningOnThread() const {
  return created_on_loop_ == MessageLoop::current();
}

invalidation::Time ChromeScheduler::GetCurrentTime() const {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  return base::Time::Now();
}

Task* ChromeScheduler::MakeTaskToPost(
    invalidation::Closure* task) {
  DCHECK(invalidation::IsCallbackRepeatable(task));
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  if (!scoped_runnable_method_factory_.get()) {
    delete task;
    return NULL;
  }
  posted_tasks_.insert(task);
  Task* task_to_post =
      scoped_runnable_method_factory_->NewRunnableMethod(
          &ChromeScheduler::RunPostedTask, task);
  return task_to_post;
}

void ChromeScheduler::RunPostedTask(invalidation::Closure* task) {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  RunAndDeleteClosure(task);
  posted_tasks_.erase(task);
}

ChromeStorage::ChromeStorage(StateWriter* state_writer,
                             invalidation::Scheduler* scheduler)
    : state_writer_(state_writer),
      scheduler_(scheduler) {
  DCHECK(state_writer_);
  DCHECK(scheduler_);
}

ChromeStorage::~ChromeStorage() {}

void ChromeStorage::WriteKey(const std::string& key, const std::string& value,
                             invalidation::WriteKeyCallback* done) {
  CHECK(state_writer_);
  // TODO(ghc): actually write key,value associations, and don't invoke the
  // callback until the operation completes.
  state_writer_->WriteState(value);
  cached_state_ = value;
  // According to the cache invalidation API folks, we can do this as
  // long as we make sure to clear the persistent state that we start
  // up the cache invalidation client with.  However, we musn't do it
  // right away, as we may be called under a lock that the callback
  // uses.
  scheduler_->Schedule(
      invalidation::Scheduler::NoDelay(),
      invalidation::NewPermanentCallback(
          this, &ChromeStorage::RunAndDeleteWriteKeyCallback,
          done));
}

void ChromeStorage::ReadKey(const std::string& key,
                            invalidation::ReadKeyCallback* done) {
  scheduler_->Schedule(
      invalidation::Scheduler::NoDelay(),
      invalidation::NewPermanentCallback(
          this, &ChromeStorage::RunAndDeleteReadKeyCallback,
          done, cached_state_));
}

void ChromeStorage::DeleteKey(const std::string& key,
                              invalidation::DeleteKeyCallback* done) {
  // TODO(ghc): Implement.
  LOG(WARNING) << "ignoring call to DeleteKey(" << key << ", callback)";
}

void ChromeStorage::ReadAllKeys(invalidation::ReadAllKeysCallback* done) {
  // TODO(ghc): Implement.
  LOG(WARNING) << "ignoring call to ReadAllKeys(callback)";
}

void ChromeStorage::RunAndDeleteWriteKeyCallback(
    invalidation::WriteKeyCallback* callback) {
  callback->Run(invalidation::Status(invalidation::Status::SUCCESS, ""));
  delete callback;
}

void ChromeStorage::RunAndDeleteReadKeyCallback(
    invalidation::ReadKeyCallback* callback, const std::string& value) {
  callback->Run(std::make_pair(
      invalidation::Status(invalidation::Status::SUCCESS, ""),
      value));
  delete callback;
}

ChromeNetwork::ChromeNetwork()
    : packet_handler_(NULL),
      scoped_callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

ChromeNetwork::~ChromeNetwork() {
  STLDeleteElements(&network_status_receivers_);
}

void ChromeNetwork::SendMessage(const std::string& outgoing_message) {
  if (packet_handler_) {
    packet_handler_->SendMessage(outgoing_message);
  }
}

void ChromeNetwork::SetMessageReceiver(
    invalidation::MessageCallback* incoming_receiver) {
  incoming_receiver_.reset(incoming_receiver);
}

void ChromeNetwork::AddNetworkStatusReceiver(
    invalidation::NetworkStatusCallback* network_status_receiver) {
  network_status_receivers_.push_back(network_status_receiver);
}

void ChromeNetwork::UpdatePacketHandler(
    CacheInvalidationPacketHandler* packet_handler) {
  packet_handler_ = packet_handler;
  if (packet_handler_ != NULL) {
    packet_handler_->SetMessageReceiver(
        scoped_callback_factory_.NewCallback(
            &ChromeNetwork::HandleInboundMessage));
  }
}

void ChromeNetwork::HandleInboundMessage(const std::string& incoming_message) {
  if (incoming_receiver_.get()) {
    incoming_receiver_->Run(incoming_message);
  }
}

ChromeSystemResources::ChromeSystemResources(StateWriter* state_writer)
    : is_started_(false),
      logger_(new ChromeLogger()),
      internal_scheduler_(new ChromeScheduler()),
      listener_scheduler_(new ChromeScheduler()),
      storage_(new ChromeStorage(state_writer, internal_scheduler_.get())),
      network_(new ChromeNetwork()) {
}

ChromeSystemResources::~ChromeSystemResources() {
  Stop();
}

void ChromeSystemResources::Start() {
  internal_scheduler_->Start();
  listener_scheduler_->Start();
}

void ChromeSystemResources::Stop() {
  internal_scheduler_->Stop();
  listener_scheduler_->Stop();
}

bool ChromeSystemResources::IsStarted() const {
  return is_started_;
}

void ChromeSystemResources::set_platform(const std::string& platform) {
  platform_ = platform;
}

std::string ChromeSystemResources::platform() const {
  return platform_;
}

ChromeLogger* ChromeSystemResources::logger() {
  return logger_.get();
}

ChromeStorage* ChromeSystemResources::storage() {
  return storage_.get();
}

ChromeNetwork* ChromeSystemResources::network() {
  return network_.get();
}

ChromeScheduler* ChromeSystemResources::internal_scheduler() {
  return internal_scheduler_.get();
}

ChromeScheduler* ChromeSystemResources::listener_scheduler() {
  return listener_scheduler_.get();
}

}  // namespace sync_notifier
