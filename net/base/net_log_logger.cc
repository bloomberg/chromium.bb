// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_logger.h"

#include <stdio.h>

#include <set>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/net_log_util.h"
#include "net/url_request/url_request_context.h"

namespace net {

NetLogLogger::NetLogLogger()
    : log_level_(NetLog::LOG_STRIP_PRIVATE_DATA), added_events_(false) {
}

NetLogLogger::~NetLogLogger() {
}

void NetLogLogger::set_log_level(net::NetLog::LogLevel log_level) {
  DCHECK(!net_log());
  log_level_ = log_level;
}

void NetLogLogger::StartObserving(net::NetLog* net_log,
                                  base::ScopedFILE file,
                                  base::Value* constants,
                                  net::URLRequestContext* url_request_context) {
  DCHECK(file.get());
  file_ = file.Pass();
  added_events_ = false;

  // Write constants to the output file.  This allows loading files that have
  // different source and event types, as they may be added and removed
  // between Chrome versions.
  std::string json;
  if (constants) {
    base::JSONWriter::Write(constants, &json);
  } else {
    scoped_ptr<base::DictionaryValue> scoped_constants(GetNetConstants());
    base::JSONWriter::Write(scoped_constants.get(), &json);
  }
  fprintf(file_.get(), "{\"constants\": %s,\n", json.c_str());

  // Start events array.  It's closed in StopObserving().
  fprintf(file_.get(), "\"events\": [\n");

  // Add events for in progress requests if a context is given.
  if (url_request_context) {
    DCHECK(url_request_context->CalledOnValidThread());

    std::set<URLRequestContext*> contexts;
    contexts.insert(url_request_context);
    CreateNetLogEntriesForActiveObjects(contexts, this);
  }

  net_log->AddThreadSafeObserver(this, log_level_);
}

void NetLogLogger::StopObserving(net::URLRequestContext* url_request_context) {
  net_log()->RemoveThreadSafeObserver(this);

  // End events array.
  fprintf(file_.get(), "]");

  // Write state of the URLRequestContext when logging stopped.
  if (url_request_context) {
    DCHECK(url_request_context->CalledOnValidThread());

    std::string json;
    scoped_ptr<base::DictionaryValue> net_info =
        GetNetInfo(url_request_context, NET_INFO_ALL_SOURCES);
    base::JSONWriter::Write(net_info.get(), &json);
    fprintf(file_.get(), ",\"tabInfo\": %s\n", json.c_str());
  }
  fprintf(file_.get(), "}");

  file_.reset();
}

void NetLogLogger::OnAddEntry(const net::NetLog::Entry& entry) {
  // Add a comma and newline for every event but the first.  Newlines are needed
  // so can load partial log files by just ignoring the last line.  For this to
  // work, lines cannot be pretty printed.
  scoped_ptr<base::Value> value(entry.ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), &json);
  fprintf(file_.get(), "%s%s",
          (added_events_ ? ",\n" : ""),
          json.c_str());
  added_events_ = true;
}

}  // namespace net
