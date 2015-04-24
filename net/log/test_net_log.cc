// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/test_net_log.h"

namespace net {

TestNetLog::TestNetLog() {
  DeprecatedAddObserver(&capturing_net_log_observer_,
                        NetLogCaptureMode::IncludeCookiesAndCredentials());
}

TestNetLog::~TestNetLog() {
  DeprecatedRemoveObserver(&capturing_net_log_observer_);
}

void TestNetLog::SetCaptureMode(NetLogCaptureMode capture_mode) {
  SetObserverCaptureMode(&capturing_net_log_observer_, capture_mode);
}

void TestNetLog::GetEntries(CapturedNetLogEntry::List* entry_list) const {
  capturing_net_log_observer_.GetEntries(entry_list);
}

void TestNetLog::GetEntriesForSource(
    NetLog::Source source,
    CapturedNetLogEntry::List* entry_list) const {
  capturing_net_log_observer_.GetEntriesForSource(source, entry_list);
}

size_t TestNetLog::GetSize() const {
  return capturing_net_log_observer_.GetSize();
}

void TestNetLog::Clear() {
  capturing_net_log_observer_.Clear();
}

BoundTestNetLog::BoundTestNetLog()
    : net_log_(BoundNetLog::Make(&capturing_net_log_, NetLog::SOURCE_NONE)) {
}

BoundTestNetLog::~BoundTestNetLog() {
}

void BoundTestNetLog::GetEntries(CapturedNetLogEntry::List* entry_list) const {
  capturing_net_log_.GetEntries(entry_list);
}

void BoundTestNetLog::GetEntriesForSource(
    NetLog::Source source,
    CapturedNetLogEntry::List* entry_list) const {
  capturing_net_log_.GetEntriesForSource(source, entry_list);
}

size_t BoundTestNetLog::GetSize() const {
  return capturing_net_log_.GetSize();
}

void BoundTestNetLog::Clear() {
  capturing_net_log_.Clear();
}

void BoundTestNetLog::SetCaptureMode(NetLogCaptureMode capture_mode) {
  capturing_net_log_.SetCaptureMode(capture_mode);
}

}  // namespace net
