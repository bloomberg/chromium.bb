// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_net_log.h"

#include <stdio.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/net/net_log_logger.h"
#include "chrome/browser/net/net_log_temp_file.h"
#include "chrome/common/chrome_switches.h"

ChromeNetLog::ChromeNetLog()
    : last_id_(0),
      base_log_level_(LOG_NONE),
      effective_log_level_(LOG_NONE),
      net_log_temp_file_(new NetLogTempFile(this)) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  // Adjust base log level based on command line switch, if present.
  // This is done before adding any observers so the call to UpdateLogLevel when
  // an observers is added will set |effective_log_level_| correctly.
  if (command_line->HasSwitch(switches::kNetLogLevel)) {
    std::string log_level_string =
        command_line->GetSwitchValueASCII(switches::kNetLogLevel);
    int command_line_log_level;
    if (base::StringToInt(log_level_string, &command_line_log_level) &&
        command_line_log_level >= LOG_ALL &&
        command_line_log_level <= LOG_NONE) {
      base_log_level_ = static_cast<LogLevel>(command_line_log_level);
    }
  }

  if (command_line->HasSwitch(switches::kLogNetLog)) {
    base::FilePath log_path =
        command_line->GetSwitchValuePath(switches::kLogNetLog);
    // Much like logging.h, bypass threading restrictions by using fopen
    // directly.  Have to write on a thread that's shutdown to handle events on
    // shutdown properly, and posting events to another thread as they occur
    // would result in an unbounded buffer size, so not much can be gained by
    // doing this on another thread.  It's only used when debugging Chrome, so
    // performance is not a big concern.
    FILE* file = NULL;
#if defined(OS_WIN)
    file = _wfopen(log_path.value().c_str(), L"w");
#elif defined(OS_POSIX)
    file = fopen(log_path.value().c_str(), "w");
#endif

    if (file == NULL) {
      LOG(ERROR) << "Could not open file " << log_path.value()
                 << " for net logging";
    } else {
      net_log_logger_.reset(new NetLogLogger(file));
      net_log_logger_->StartObserving(this);
    }
  }
}

ChromeNetLog::~ChromeNetLog() {
  net_log_temp_file_.reset();
  // Remove the observers we own before we're destroyed.
  if (net_log_logger_)
    RemoveThreadSafeObserver(net_log_logger_.get());
}

void ChromeNetLog::OnAddEntry(const net::NetLog::Entry& entry) {
  base::AutoLock lock(lock_);

  // Notify all of the log observers.
  FOR_EACH_OBSERVER(ThreadSafeObserver, observers_, OnAddEntry(entry));
}

uint32 ChromeNetLog::NextID() {
  return base::subtle::NoBarrier_AtomicIncrement(&last_id_, 1);
}

net::NetLog::LogLevel ChromeNetLog::GetLogLevel() const {
  base::subtle::Atomic32 log_level =
      base::subtle::NoBarrier_Load(&effective_log_level_);
  return static_cast<net::NetLog::LogLevel>(log_level);
}

void ChromeNetLog::AddThreadSafeObserver(
    net::NetLog::ThreadSafeObserver* observer,
    LogLevel log_level) {
  base::AutoLock lock(lock_);

  observers_.AddObserver(observer);
  OnAddObserver(observer, log_level);
  UpdateLogLevel();
}

void ChromeNetLog::SetObserverLogLevel(
    net::NetLog::ThreadSafeObserver* observer,
    LogLevel log_level) {
  base::AutoLock lock(lock_);

  DCHECK(observers_.HasObserver(observer));
  OnSetObserverLogLevel(observer, log_level);
  UpdateLogLevel();
}

void ChromeNetLog::RemoveThreadSafeObserver(
    net::NetLog::ThreadSafeObserver* observer) {
  base::AutoLock lock(lock_);

  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
  OnRemoveObserver(observer);
  UpdateLogLevel();
}

void ChromeNetLog::UpdateLogLevel() {
  lock_.AssertAcquired();

  // Look through all the observers and find the finest granularity
  // log level (higher values of the enum imply *lower* log levels).
  LogLevel new_effective_log_level = base_log_level_;
  ObserverListBase<ThreadSafeObserver>::Iterator it(observers_);
  ThreadSafeObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    new_effective_log_level =
        std::min(new_effective_log_level, observer->log_level());
  }
  base::subtle::NoBarrier_Store(&effective_log_level_,
                                new_effective_log_level);
}
