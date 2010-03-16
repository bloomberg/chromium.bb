// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_net_log.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/passive_log_collector.h"

ChromeNetLog::ChromeNetLog()
    : next_id_(0),
      passive_collector_(new PassiveLogCollector) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  AddObserver(passive_collector_.get());
}

ChromeNetLog::~ChromeNetLog() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  RemoveObserver(passive_collector_.get());
}

void ChromeNetLog::AddEntry(const Entry& entry) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // Notify all of the log observers.
  FOR_EACH_OBSERVER(Observer, observers_, OnAddEntry(entry));
}

int ChromeNetLog::NextID() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return next_id_++;
}

bool ChromeNetLog::HasListener() const {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // TODO(eroman): Hack to get refactor working.
  return passive_collector_->url_request_tracker()->IsUnbounded();
}

void ChromeNetLog::AddObserver(Observer* observer) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  observers_.AddObserver(observer);
}

void ChromeNetLog::RemoveObserver(Observer* observer) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  observers_.RemoveObserver(observer);
}

