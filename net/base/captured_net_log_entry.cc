// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/captured_net_log_entry.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"

namespace net {

CapturedNetLogEntry::CapturedNetLogEntry(
    NetLog::EventType type,
    const base::TimeTicks& time,
    NetLog::Source source,
    NetLog::EventPhase phase,
    scoped_ptr<base::DictionaryValue> params)
    : type(type),
      time(time),
      source(source),
      phase(phase),
      params(params.Pass()) {
  // Only entries without a NetLog should have an invalid source.
  CHECK(source.IsValid());
}

CapturedNetLogEntry::CapturedNetLogEntry(const CapturedNetLogEntry& entry) {
  *this = entry;
}

CapturedNetLogEntry::~CapturedNetLogEntry() {}

CapturedNetLogEntry& CapturedNetLogEntry::operator=(
    const CapturedNetLogEntry& entry) {
  type = entry.type;
  time = entry.time;
  source = entry.source;
  phase = entry.phase;
  params.reset(entry.params ? entry.params->DeepCopy() : NULL);
  return *this;
}

bool CapturedNetLogEntry::GetStringValue(const std::string& name,
                                         std::string* value) const {
  if (!params)
    return false;
  return params->GetString(name, value);
}

bool CapturedNetLogEntry::GetIntegerValue(const std::string& name,
                                          int* value) const {
  if (!params)
    return false;
  return params->GetInteger(name, value);
}

bool CapturedNetLogEntry::GetListValue(const std::string& name,
                                       base::ListValue** value) const {
  if (!params)
    return false;
  return params->GetList(name, value);
}

bool CapturedNetLogEntry::GetNetErrorCode(int* value) const {
  return GetIntegerValue("net_error", value);
}

std::string CapturedNetLogEntry::GetParamsJson() const {
  if (!params)
    return std::string();
  std::string json;
  base::JSONWriter::Write(params.get(), &json);
  return json;
}

}  // namespace net
