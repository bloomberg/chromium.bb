// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/event_logger.h"

namespace drive {

EventLogger::Event::Event(int id, const std::string& what)
    : id(id),
      when(base::Time::Now()),
      what(what) {
}

EventLogger::EventLogger(size_t history_size)
    : history_size_(history_size),
      next_event_id_(0) {
}

EventLogger::~EventLogger() {
}

void EventLogger::Log(const std::string& what) {
  history_.push_back(Event(next_event_id_, what));
  ++next_event_id_;
  if (history_.size() > history_size_)
    history_.pop_front();
}

}  // namespace drive
