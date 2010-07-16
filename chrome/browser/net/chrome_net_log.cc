// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_net_log.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/load_timing_observer.h"
#include "chrome/browser/net/passive_log_collector.h"

ChromeNetLog::ChromeNetLog()
    : next_id_(1),
      passive_collector_(new PassiveLogCollector),
      load_timing_observer_(new LoadTimingObserver) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  AddObserver(passive_collector_.get());
  AddObserver(load_timing_observer_.get());
}

ChromeNetLog::~ChromeNetLog() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  RemoveObserver(passive_collector_.get());
  RemoveObserver(load_timing_observer_.get());
}

void ChromeNetLog::AddEntry(EventType type,
                            const base::TimeTicks& time,
                            const Source& source,
                            EventPhase phase,
                            EventParameters* params) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // Notify all of the log observers.
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnAddEntry(type, time, source, phase, params));
}

uint32 ChromeNetLog::NextID() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return next_id_++;
}

bool ChromeNetLog::HasListener() const {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // (Don't count the PassiveLogCollector observer).
  return observers_.size() > 1;
}

void ChromeNetLog::AddObserver(Observer* observer) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  observers_.AddObserver(observer);
}

void ChromeNetLog::RemoveObserver(Observer* observer) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  observers_.RemoveObserver(observer);
}
