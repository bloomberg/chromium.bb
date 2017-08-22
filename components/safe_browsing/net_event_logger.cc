// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/net_event_logger.h"

#include <memory>

#include "base/bind.h"
#include "base/values.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

// Return a dictionary with "url"=|url-spec| and optionally
// |name|=|value| (if not null), for netlogging.
// This will also add a reference to the original request's net_log ID.
std::unique_ptr<base::Value> NetLogUrlCallback(
    const net::NetLogWithSource* net_log,
    const GURL& url,
    const char* name,
    const char* value,
    net::NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> event_params(
      new base::DictionaryValue());
  event_params->SetString("url", url.spec());
  if (name && value)
    event_params->SetString(name, value);
  net_log->source().AddToEventParameters(event_params.get());
  return std::move(event_params);
}

// Return a dictionary with |name|=|value|, for netlogging.
std::unique_ptr<base::Value> NetLogStringCallback(const char* name,
                                                  const char* value,
                                                  net::NetLogCaptureMode) {
  std::unique_ptr<base::DictionaryValue> event_params(
      new base::DictionaryValue());
  if (name && value)
    event_params->SetString(name, value);
  return std::move(event_params);
}

}  // namespace

NetEventLogger::NetEventLogger(const net::NetLogWithSource* net_log)
    : net_log_(net_log),
      net_log_with_sb_source_(
          net::NetLogWithSource::Make(net_log_->net_log(),
                                      net::NetLogSourceType::SAFE_BROWSING)) {}

void NetEventLogger::BeginNetLogEvent(net::NetLogEventType type,
                                      const GURL& url,
                                      const char* name,
                                      const char* value) {
  net_log_with_sb_source_.BeginEvent(
      type, base::Bind(&NetLogUrlCallback, net_log_, url, name, value));
  net_log_->AddEvent(
      type, net_log_with_sb_source_.source().ToEventParametersCallback());
}

void NetEventLogger::EndNetLogEvent(net::NetLogEventType type,
                                    const char* name,
                                    const char* value) {
  net_log_with_sb_source_.EndEvent(
      type, base::Bind(&NetLogStringCallback, name, value));
  net_log_->AddEvent(
      type, net_log_with_sb_source_.source().ToEventParametersCallback());
}

}  // namespace safe_browsing
