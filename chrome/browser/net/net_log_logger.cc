// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_log_logger.h"

#include "base/json/json_writer.h"
#include "base/values.h"

NetLogLogger::NetLogLogger() : Observer(net::NetLog::LOG_ALL_BUT_BYTES) {}

NetLogLogger::~NetLogLogger() {}

void NetLogLogger::OnAddEntry(net::NetLog::EventType type,
                              const base::TimeTicks& time,
                              const net::NetLog::Source& source,
                              net::NetLog::EventPhase phase,
                              net::NetLog::EventParameters* params) {
  scoped_ptr<Value> value(net::NetLog::EntryToDictionaryValue(type, time,
                                                              source, phase,
                                                              params, true));
  std::string json;
  base::JSONWriter::Write(value.get(), true, &json);
  VLOG(1) << json;
}

