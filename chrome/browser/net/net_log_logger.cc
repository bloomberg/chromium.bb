// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_log_logger.h"

#include <stdio.h>

#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"

NetLogLogger::NetLogLogger(const FilePath &log_path)
    : ThreadSafeObserver(net::NetLog::LOG_ALL_BUT_BYTES) {
  if (!log_path.empty()) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    file_.Set(file_util::OpenFile(log_path, "w"));
  }
}

NetLogLogger::~NetLogLogger() {}

void NetLogLogger::OnAddEntry(net::NetLog::EventType type,
                              const base::TimeTicks& time,
                              const net::NetLog::Source& source,
                              net::NetLog::EventPhase phase,
                              net::NetLog::EventParameters* params) {
  scoped_ptr<Value> value(net::NetLog::EntryToDictionaryValue(type, time,
                                                              source, phase,
                                                              params, true));
  // Don't pretty print, so each JSON value occupies a single line, with no
  // breaks (Line breaks in any text field will be escaped).  Using strings
  // instead of integer identifiers allows logs from older versions to be
  // loaded, though a little extra parsing has to be done when loading a log.
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  if (!file_.get()) {
    VLOG(1) << json;
  } else {
    fprintf(file_.get(), "%s\n", json.c_str());
  }
}

