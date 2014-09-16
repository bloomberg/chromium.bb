// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/trace_net_log_observer.h"

#include <stdio.h>

#include <string>

#include "base/debug/trace_event.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/net_log.h"

namespace net {

namespace {

class TracedValue : public base::debug::ConvertableToTraceFormat {
 public:
  explicit TracedValue(scoped_ptr<base::Value> value) : value_(value.Pass()) {}

 private:
  virtual ~TracedValue() {}

  virtual void AppendAsTraceFormat(std::string* out) const OVERRIDE {
    if (value_) {
      std::string tmp;
      base::JSONWriter::Write(value_.get(), &tmp);
      *out += tmp;
    } else {
      *out += "\"\"";
    }
  }

 private:
  scoped_ptr<base::Value> value_;
};

}  // namespace

TraceNetLogObserver::TraceNetLogObserver() : net_log_to_watch_(NULL) {
}

TraceNetLogObserver::~TraceNetLogObserver() {
  DCHECK(!net_log_to_watch_);
  DCHECK(!net_log());
}

void TraceNetLogObserver::OnAddEntry(const NetLog::Entry& entry) {
  scoped_ptr<base::Value> params(entry.ParametersToValue());
  switch (entry.phase()) {
    case NetLog::PHASE_BEGIN:
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
          "netlog", NetLog::EventTypeToString(entry.type()), entry.source().id,
          "source_type", NetLog::SourceTypeToString(entry.source().type),
          "params", scoped_refptr<base::debug::ConvertableToTraceFormat>(
              new TracedValue(params.Pass())));
      break;
    case NetLog::PHASE_END:
      TRACE_EVENT_NESTABLE_ASYNC_END2(
          "netlog", NetLog::EventTypeToString(entry.type()), entry.source().id,
          "source_type", NetLog::SourceTypeToString(entry.source().type),
          "params", scoped_refptr<base::debug::ConvertableToTraceFormat>(
              new TracedValue(params.Pass())));
      break;
    case NetLog::PHASE_NONE:
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT2(
          "netlog", NetLog::EventTypeToString(entry.type()), entry.source().id,
          "source_type", NetLog::SourceTypeToString(entry.source().type),
          "params", scoped_refptr<base::debug::ConvertableToTraceFormat>(
              new TracedValue(params.Pass())));
      break;
  }
}

void TraceNetLogObserver::WatchForTraceStart(NetLog* netlog) {
  DCHECK(!net_log_to_watch_);
  DCHECK(!net_log());
  net_log_to_watch_ = netlog;
  base::debug::TraceLog::GetInstance()->AddEnabledStateObserver(this);
}

void TraceNetLogObserver::StopWatchForTraceStart() {
  // Should only stop if is currently watching.
  DCHECK(net_log_to_watch_);
  base::debug::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
  if (net_log())
    net_log()->RemoveThreadSafeObserver(this);
  net_log_to_watch_ = NULL;
}

void TraceNetLogObserver::OnTraceLogEnabled() {
  net_log_to_watch_->AddThreadSafeObserver(this,
                                           NetLog::LOG_STRIP_PRIVATE_DATA);
}

void TraceNetLogObserver::OnTraceLogDisabled() {
  if (net_log())
    net_log()->RemoveThreadSafeObserver(this);
}

}  // namespace net
