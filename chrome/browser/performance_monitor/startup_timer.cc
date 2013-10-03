// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/startup_timer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

namespace performance_monitor {

namespace {
// Needed because Database::AddMetric is overloaded, so base::Bind doesn't work.
void AddMetricToDatabaseOnBackgroundThread(Database* database,
                                           const Metric& metric) {
  database->AddMetric(metric);
}

}  // namespace

// static
StartupTimer* StartupTimer::g_startup_timer_ = NULL;

StartupTimer::StartupTimer() : startup_begin_(base::TimeTicks::Now()),
                               startup_type_(STARTUP_NORMAL),
                               performance_monitor_initialized_(false) {
  CHECK(!g_startup_timer_);
  g_startup_timer_ = this;

  // We need this check because, under certain rare circumstances,
  // NotificationService::current() will return null, and this will cause a
  // segfault in NotificationServiceImpl::AddObserver(). Currently, this only
  // happens as a result of the child process launched by BrowserMainTest.
  // WarmConnectionFieldTrial_Invalid.
  if (content::NotificationService::current()) {
    registrar_.Add(this, chrome::NOTIFICATION_PERFORMANCE_MONITOR_INITIALIZED,
                   content::NotificationService::AllSources());
  }
}

StartupTimer::~StartupTimer() {
  DCHECK(this == g_startup_timer_);
  g_startup_timer_ = NULL;
}

bool StartupTimer::SignalStartupComplete(StartupType startup_type) {
  DCHECK(elapsed_startup_time_ == base::TimeDelta());

  startup_type_ = startup_type;

  elapsed_startup_time_ =
      base::TimeTicks::Now() - total_pause_ - startup_begin_;

  if (performance_monitor_initialized_)
    InsertElapsedStartupTime();

  return true;
}

// static
void StartupTimer::PauseTimer() {
  // Check that the timer is not already paused.
  DCHECK(g_startup_timer_->pause_started_ == base::TimeTicks());

  g_startup_timer_->pause_started_ = base::TimeTicks::Now();
}

// static
void StartupTimer::UnpauseTimer() {
  // Check that the timer has been paused.
  DCHECK(g_startup_timer_->pause_started_ != base::TimeTicks());

  g_startup_timer_->total_pause_ += base::TimeTicks::Now() -
                                    g_startup_timer_->pause_started_;

  g_startup_timer_->pause_started_ = base::TimeTicks();
}

void StartupTimer::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  CHECK(type == chrome::NOTIFICATION_PERFORMANCE_MONITOR_INITIALIZED);
  performance_monitor_initialized_ = true;

  if (PerformanceMonitor::GetInstance()->database_logging_enabled()) {
    if (elapsed_startup_time_ != base::TimeDelta())
      InsertElapsedStartupTime();
    if (elapsed_session_restore_times_.size())
      InsertElapsedSessionRestoreTime();
  }
}

// static
void StartupTimer::SetElapsedSessionRestoreTime(
    const base::TimeDelta& elapsed_session_restore_time) {
  if (PerformanceMonitor::GetInstance()->database_logging_enabled()) {
    g_startup_timer_->elapsed_session_restore_times_.push_back(
        elapsed_session_restore_time);

    if (g_startup_timer_->performance_monitor_initialized_)
      g_startup_timer_->InsertElapsedSessionRestoreTime();
  }
}

void StartupTimer::InsertElapsedStartupTime() {
  content::BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(
          &AddMetricToDatabaseOnBackgroundThread,
          base::Unretained(PerformanceMonitor::GetInstance()->database()),
          Metric(startup_type_ == STARTUP_NORMAL ? METRIC_STARTUP_TIME
                                                 : METRIC_TEST_STARTUP_TIME,
                 base::Time::Now(),
                 static_cast<double>(
                     elapsed_startup_time_.ToInternalValue()))));
}

void StartupTimer::InsertElapsedSessionRestoreTime() {
  for (std::vector<base::TimeDelta>::const_iterator iter =
           elapsed_session_restore_times_.begin();
       iter != elapsed_session_restore_times_.end(); ++iter) {
    content::BrowserThread::PostBlockingPoolSequencedTask(
        Database::kDatabaseSequenceToken,
        FROM_HERE,
        base::Bind(
            &AddMetricToDatabaseOnBackgroundThread,
            base::Unretained(PerformanceMonitor::GetInstance()->database()),
            Metric(METRIC_SESSION_RESTORE_TIME,
                   base::Time::Now(),
                   static_cast<double>(iter->ToInternalValue()))));
  }
}

}  // namespace performance_monitor
