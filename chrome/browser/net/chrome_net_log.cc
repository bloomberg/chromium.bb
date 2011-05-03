// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_net_log.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/net/load_timing_observer.h"
#include "chrome/browser/net/net_log_logger.h"
#include "chrome/browser/net/passive_log_collector.h"
#include "chrome/common/chrome_switches.h"

ChromeNetLog::ThreadSafeObserver::ThreadSafeObserver(LogLevel log_level)
    : net_log_(NULL),
      log_level_(log_level) {
}

ChromeNetLog::ThreadSafeObserver::~ThreadSafeObserver() {
  DCHECK(!net_log_);
}

net::NetLog::LogLevel ChromeNetLog::ThreadSafeObserver::log_level() const {
  return log_level_;
}

void ChromeNetLog::ThreadSafeObserver::AssertNetLogLockAcquired() const {
  if (net_log_)
    net_log_->lock_.AssertAcquired();
}

void ChromeNetLog::ThreadSafeObserver::SetLogLevel(
    net::NetLog::LogLevel log_level) {
  DCHECK(net_log_);
  base::AutoLock lock(net_log_->lock_);
  log_level_ = log_level;
  net_log_->UpdateLogLevel();
}

ChromeNetLog::Entry::Entry(uint32 order,
                           net::NetLog::EventType type,
                           const base::TimeTicks& time,
                           net::NetLog::Source source,
                           net::NetLog::EventPhase phase,
                           net::NetLog::EventParameters* params)
    : order(order),
      type(type),
      time(time),
      source(source),
      phase(phase),
      params(params) {
}

ChromeNetLog::Entry::~Entry() {}

ChromeNetLog::ChromeNetLog()
    : last_id_(0),
      base_log_level_(LOG_BASIC),
      effective_log_level_(LOG_BASIC),
      passive_collector_(new PassiveLogCollector),
      load_timing_observer_(new LoadTimingObserver) {
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
        command_line_log_level <= LOG_BASIC) {
      base_log_level_ = static_cast<LogLevel>(command_line_log_level);
    }
  }

  AddObserver(passive_collector_.get());
  AddObserver(load_timing_observer_.get());

  if (command_line->HasSwitch(switches::kLogNetLog)) {
    net_log_logger_.reset(new NetLogLogger(
            command_line->GetSwitchValuePath(switches::kLogNetLog)));
    AddObserver(net_log_logger_.get());
  }
}

ChromeNetLog::~ChromeNetLog() {
  RemoveObserver(passive_collector_.get());
  RemoveObserver(load_timing_observer_.get());
  if (net_log_logger_.get()) {
    RemoveObserver(net_log_logger_.get());
  }
}

void ChromeNetLog::AddEntry(EventType type,
                            const base::TimeTicks& time,
                            const Source& source,
                            EventPhase phase,
                            EventParameters* params) {
  base::AutoLock lock(lock_);

  // Notify all of the log observers.
  FOR_EACH_OBSERVER(ThreadSafeObserver, observers_,
                    OnAddEntry(type, time, source, phase, params));
}

uint32 ChromeNetLog::NextID() {
  return base::subtle::NoBarrier_AtomicIncrement(&last_id_, 1);
}

net::NetLog::LogLevel ChromeNetLog::GetLogLevel() const {
  base::subtle::Atomic32 log_level =
      base::subtle::NoBarrier_Load(&effective_log_level_);
  return static_cast<net::NetLog::LogLevel>(log_level);
}

void ChromeNetLog::AddObserver(ThreadSafeObserver* observer) {
  base::AutoLock lock(lock_);
  AddObserverWhileLockHeld(observer);
}

void ChromeNetLog::RemoveObserver(ThreadSafeObserver* observer) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(observer->net_log_, this);
  observer->net_log_ = NULL;
  observers_.RemoveObserver(observer);
  UpdateLogLevel();
}

void ChromeNetLog::AddObserverAndGetAllPassivelyCapturedEvents(
    ThreadSafeObserver* observer, EntryList* passive_entries) {
  base::AutoLock lock(lock_);
  AddObserverWhileLockHeld(observer);
  passive_collector_->GetAllCapturedEvents(passive_entries);
}

void ChromeNetLog::GetAllPassivelyCapturedEvents(EntryList* passive_entries) {
  base::AutoLock lock(lock_);
  passive_collector_->GetAllCapturedEvents(passive_entries);
}

void ChromeNetLog::ClearAllPassivelyCapturedEvents() {
  base::AutoLock lock(lock_);
  passive_collector_->Clear();
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

void ChromeNetLog::AddObserverWhileLockHeld(ThreadSafeObserver* observer) {
  DCHECK(!observer->net_log_);
  observer->net_log_ = this;
  observers_.AddObserver(observer);
  UpdateLogLevel();
}
