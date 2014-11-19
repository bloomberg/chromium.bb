// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/capturing_net_log.h"

namespace net {

CapturingNetLog::CapturingNetLog() {
  AddThreadSafeObserver(&capturing_net_log_observer_, LOG_ALL_BUT_BYTES);
}

CapturingNetLog::~CapturingNetLog() {
  RemoveThreadSafeObserver(&capturing_net_log_observer_);
}

void CapturingNetLog::SetLogLevel(NetLog::LogLevel log_level) {
  SetObserverLogLevel(&capturing_net_log_observer_, log_level);
}

void CapturingNetLog::GetEntries(
    CapturingNetLog::CapturedEntryList* entry_list) const {
  capturing_net_log_observer_.GetEntries(entry_list);
}

void CapturingNetLog::GetEntriesForSource(
    NetLog::Source source,
    CapturedEntryList* entry_list) const {
  capturing_net_log_observer_.GetEntriesForSource(source, entry_list);
}

size_t CapturingNetLog::GetSize() const {
  return capturing_net_log_observer_.GetSize();
}

void CapturingNetLog::Clear() {
  capturing_net_log_observer_.Clear();
}

CapturingBoundNetLog::CapturingBoundNetLog()
    : net_log_(BoundNetLog::Make(&capturing_net_log_,
                                 net::NetLog::SOURCE_NONE)) {
}

CapturingBoundNetLog::~CapturingBoundNetLog() {}

void CapturingBoundNetLog::GetEntries(
    CapturingNetLog::CapturedEntryList* entry_list) const {
  capturing_net_log_.GetEntries(entry_list);
}

void CapturingBoundNetLog::GetEntriesForSource(
    NetLog::Source source,
    CapturingNetLog::CapturedEntryList* entry_list) const {
  capturing_net_log_.GetEntriesForSource(source, entry_list);
}

size_t CapturingBoundNetLog::GetSize() const {
  return capturing_net_log_.GetSize();
}

void CapturingBoundNetLog::Clear() {
  capturing_net_log_.Clear();
}

void CapturingBoundNetLog::SetLogLevel(NetLog::LogLevel log_level) {
  capturing_net_log_.SetLogLevel(log_level);
}

}  // namespace net
