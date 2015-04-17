// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/test_net_log.h"

namespace net {

TestNetLog::TestNetLog() {
  DeprecatedAddObserver(&capturing_net_log_observer_, LOG_ALL_BUT_BYTES);
}

TestNetLog::~TestNetLog() {
  DeprecatedRemoveObserver(&capturing_net_log_observer_);
}

void TestNetLog::SetLogLevel(NetLog::LogLevel log_level) {
  SetObserverLogLevel(&capturing_net_log_observer_, log_level);
}

void TestNetLog::GetEntries(TestNetLog::CapturedEntryList* entry_list) const {
  capturing_net_log_observer_.GetEntries(entry_list);
}

void TestNetLog::GetEntriesForSource(NetLog::Source source,
                                     CapturedEntryList* entry_list) const {
  capturing_net_log_observer_.GetEntriesForSource(source, entry_list);
}

size_t TestNetLog::GetSize() const {
  return capturing_net_log_observer_.GetSize();
}

void TestNetLog::Clear() {
  capturing_net_log_observer_.Clear();
}

BoundTestNetLog::BoundTestNetLog()
    : net_log_(
          BoundNetLog::Make(&capturing_net_log_, net::NetLog::SOURCE_NONE)) {
}

BoundTestNetLog::~BoundTestNetLog() {
}

void BoundTestNetLog::GetEntries(
    TestNetLog::CapturedEntryList* entry_list) const {
  capturing_net_log_.GetEntries(entry_list);
}

void BoundTestNetLog::GetEntriesForSource(
    NetLog::Source source,
    TestNetLog::CapturedEntryList* entry_list) const {
  capturing_net_log_.GetEntriesForSource(source, entry_list);
}

size_t BoundTestNetLog::GetSize() const {
  return capturing_net_log_.GetSize();
}

void BoundTestNetLog::Clear() {
  capturing_net_log_.Clear();
}

void BoundTestNetLog::SetLogLevel(NetLog::LogLevel log_level) {
  capturing_net_log_.SetLogLevel(log_level);
}

}  // namespace net
