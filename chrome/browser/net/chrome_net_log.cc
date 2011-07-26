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

ChromeNetLog::ThreadSafeObserverImpl::ThreadSafeObserverImpl(LogLevel log_level)
    : net_log_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(internal_observer_(this, log_level)) {
}

ChromeNetLog::ThreadSafeObserverImpl::~ThreadSafeObserverImpl() {
  DCHECK(!net_log_);
}

void ChromeNetLog::ThreadSafeObserverImpl::AddAsObserver(
    ChromeNetLog* net_log) {
  DCHECK(!net_log_);
  net_log_ = net_log;
  net_log_->AddThreadSafeObserver(&internal_observer_);
}

void ChromeNetLog::ThreadSafeObserverImpl::RemoveAsObserver() {
  DCHECK(net_log_);
  net_log_->RemoveThreadSafeObserver(&internal_observer_);
  net_log_ = NULL;
}

void ChromeNetLog::ThreadSafeObserverImpl::SetLogLevel(
    net::NetLog::LogLevel log_level) {
  DCHECK(net_log_);
  base::AutoLock lock(net_log_->lock_);
  internal_observer_.SetLogLevel(log_level);
  net_log_->UpdateLogLevel();
}

void ChromeNetLog::ThreadSafeObserverImpl::
AddAsObserverAndGetAllPassivelyCapturedEvents(
    ChromeNetLog* net_log,
    EntryList* entries) {
  DCHECK(!net_log_);
  net_log_ = net_log;
  net_log_->AddObserverAndGetAllPassivelyCapturedEvents(&internal_observer_,
                                                        entries);
}

void ChromeNetLog::ThreadSafeObserverImpl::AssertNetLogLockAcquired() const {
  if (net_log_)
    net_log_->lock_.AssertAcquired();
}

ChromeNetLog::ThreadSafeObserverImpl::PassThroughObserver::PassThroughObserver(
    ChromeNetLog::ThreadSafeObserverImpl* owner,
    net::NetLog::LogLevel log_level)
    : net::NetLog::ThreadSafeObserver(log_level),
      ALLOW_THIS_IN_INITIALIZER_LIST(owner_(owner)) {
}

void ChromeNetLog::ThreadSafeObserverImpl::PassThroughObserver::OnAddEntry(
    net::NetLog::EventType type,
    const base::TimeTicks& time,
    const net::NetLog::Source& source,
    net::NetLog::EventPhase phase,
    net::NetLog::EventParameters* params) {
  owner_->OnAddEntry(type, time, source, phase, params);
}

void ChromeNetLog::ThreadSafeObserverImpl::PassThroughObserver::SetLogLevel(
    net::NetLog::LogLevel log_level) {
  log_level_ = log_level;
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

  passive_collector_->AddAsObserver(this);
  load_timing_observer_->AddAsObserver(this);

  if (command_line->HasSwitch(switches::kLogNetLog)) {
    net_log_logger_.reset(new NetLogLogger(
        command_line->GetSwitchValuePath(switches::kLogNetLog)));
    net_log_logger_->AddAsObserver(this);
  }
}

ChromeNetLog::~ChromeNetLog() {
  passive_collector_->RemoveAsObserver();
  load_timing_observer_->RemoveAsObserver();
  if (net_log_logger_.get()) {
    net_log_logger_->RemoveAsObserver();
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

void ChromeNetLog::AddThreadSafeObserver(
    net::NetLog::ThreadSafeObserver* observer) {
  base::AutoLock lock(lock_);
  AddObserverWhileLockHeld(observer);
}

void ChromeNetLog::RemoveThreadSafeObserver(
    net::NetLog::ThreadSafeObserver* observer) {
  base::AutoLock lock(lock_);
  observers_.RemoveObserver(observer);
  UpdateLogLevel();
}

void ChromeNetLog::AddObserverAndGetAllPassivelyCapturedEvents(
    net::NetLog::ThreadSafeObserver* observer, EntryList* passive_entries) {
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

void ChromeNetLog::AddObserverWhileLockHeld(
    net::NetLog::ThreadSafeObserver* observer) {
  observers_.AddObserver(observer);
  UpdateLogLevel();
}
