// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/load_timing_observer.h"

#include "base/time.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "content/public/common/resource_response.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_netlog_params.h"

using base::Time;
using base::TimeTicks;
using content::BrowserThread;
using webkit_glue::ResourceLoaderBridge;
using webkit_glue::ResourceLoadTimingInfo;

const size_t kMaxNumEntries = 1000;

namespace {

const int64 kSyncPeriodMicroseconds = 1000 * 1000 * 10;

// We know that this conversion is not solid and suffers from world clock
// changes, but given that we sync clock every 10 seconds, it should be good
// enough for the load timing info.
static Time TimeTicksToTime(const TimeTicks& time_ticks) {
  static int64 tick_to_time_offset;
  static int64 last_sync_ticks = 0;
  if (time_ticks.ToInternalValue() - last_sync_ticks >
          kSyncPeriodMicroseconds) {
    int64 cur_time = (Time::Now() - Time()).InMicroseconds();
    int64 cur_time_ticks = (TimeTicks::Now() - TimeTicks()).InMicroseconds();
    // If we add this number to a time tick value, it gives the timestamp.
    tick_to_time_offset = cur_time - cur_time_ticks;
    last_sync_ticks = time_ticks.ToInternalValue();
  }
  return Time::FromInternalValue(time_ticks.ToInternalValue() +
                                 tick_to_time_offset);
}

static int32 TimeTicksToOffset(
    const TimeTicks& time_ticks,
    LoadTimingObserver::URLRequestRecord* record) {
  return static_cast<int32>(
      (time_ticks - record->base_ticks).InMillisecondsRoundedUp());
}

}  // namespace

LoadTimingObserver::URLRequestRecord::URLRequestRecord()
    : connect_job_id(net::NetLog::Source::kInvalidId),
      socket_log_id(net::NetLog::Source::kInvalidId),
      socket_reused(false) {
}

LoadTimingObserver::HTTPStreamJobRecord::HTTPStreamJobRecord()
    : socket_log_id(net::NetLog::Source::kInvalidId),
      socket_reused(false) {
}

LoadTimingObserver::LoadTimingObserver()
    : last_connect_job_id_(net::NetLog::Source::kInvalidId) {
}

LoadTimingObserver::~LoadTimingObserver() {
}

void LoadTimingObserver::StartObserving(net::NetLog* net_log) {
  net_log->AddThreadSafeObserver(this, net::NetLog::LOG_BASIC);
}

LoadTimingObserver::URLRequestRecord*
LoadTimingObserver::GetURLRequestRecord(uint32 source_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  URLRequestToRecordMap::iterator it = url_request_to_record_.find(source_id);
  if (it != url_request_to_record_.end())
    return &it->second;
  return NULL;
}

void LoadTimingObserver::OnAddEntry(const net::NetLog::Entry& entry) {
  // The events that the Observer is interested in only occur on the IO thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO))
    return;
  if (entry.source().type == net::NetLog::SOURCE_URL_REQUEST)
    OnAddURLRequestEntry(entry);
  else if (entry.source().type == net::NetLog::SOURCE_HTTP_STREAM_JOB)
    OnAddHTTPStreamJobEntry(entry);
  else if (entry.source().type == net::NetLog::SOURCE_CONNECT_JOB)
    OnAddConnectJobEntry(entry);
  else if (entry.source().type == net::NetLog::SOURCE_SOCKET)
    OnAddSocketEntry(entry);
}

// static
void LoadTimingObserver::PopulateTimingInfo(
    net::URLRequest* request,
    content::ResourceResponse* response) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!(request->load_flags() & net::LOAD_ENABLE_LOAD_TIMING))
    return;

  ChromeNetLog* chrome_net_log = static_cast<ChromeNetLog*>(
      request->net_log().net_log());
  if (chrome_net_log == NULL)
    return;

  uint32 source_id = request->net_log().source().id;
  LoadTimingObserver* observer = chrome_net_log->load_timing_observer();
  LoadTimingObserver::URLRequestRecord* record =
      observer->GetURLRequestRecord(source_id);
  if (record) {
    response->head.connection_id = record->socket_log_id;
    response->head.connection_reused = record->socket_reused;
#if !defined(OS_IOS)
    response->head.load_timing = record->timing;
#endif
  }
}

void LoadTimingObserver::OnAddURLRequestEntry(const net::NetLog::Entry& entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool is_begin = entry.phase() == net::NetLog::PHASE_BEGIN;
  bool is_end = entry.phase() == net::NetLog::PHASE_END;

  if (entry.type() == net::NetLog::TYPE_URL_REQUEST_START_JOB) {
    if (is_begin) {
      // Only record timing for entries with corresponding flag.
      int load_flags;
      scoped_ptr<Value> event_params(entry.ParametersToValue());
      if (!net::StartEventLoadFlagsFromEventParams(event_params.get(),
                                                   &load_flags)) {
        NOTREACHED();
        return;
      }

      if (!(load_flags & net::LOAD_ENABLE_LOAD_TIMING))
        return;

      // Prevents us from passively growing the memory unbounded in case
      // something went wrong. Should not happen.
      if (url_request_to_record_.size() > kMaxNumEntries) {
        LOG(WARNING) << "The load timing observer url request count has grown "
                        "larger than expected, resetting";
        url_request_to_record_.clear();
      }

      URLRequestRecord& record = url_request_to_record_[entry.source().id];
      base::TimeTicks now = GetCurrentTime();
      record.base_ticks = now;
#if !defined(OS_IOS)
      record.timing = ResourceLoadTimingInfo();
      record.timing.base_ticks = now;
      record.timing.base_time = TimeTicksToTime(now);
#endif
    }
    return;
  } else if (entry.type() == net::NetLog::TYPE_REQUEST_ALIVE) {
    // Cleanup records based on the TYPE_REQUEST_ALIVE entry.
    if (is_end)
      url_request_to_record_.erase(entry.source().id);
    return;
  }

  URLRequestRecord* record = GetURLRequestRecord(entry.source().id);
  if (!record)
    return;

#if !defined(OS_IOS)
  ResourceLoadTimingInfo& timing = record->timing;

  switch (entry.type()) {
    case net::NetLog::TYPE_PROXY_SERVICE:
      if (is_begin)
        timing.proxy_start = TimeTicksToOffset(GetCurrentTime(), record);
      else if (is_end)
        timing.proxy_end = TimeTicksToOffset(GetCurrentTime(), record);
      break;
    case net::NetLog::TYPE_HTTP_STREAM_REQUEST_BOUND_TO_JOB: {
      net::NetLog::Source http_stream_job_source;
      scoped_ptr<Value> event_params(entry.ParametersToValue());
      if (!net::NetLog::Source::FromEventParameters(
              event_params.get(),
              &http_stream_job_source)) {
        NOTREACHED();
        return;
      }
      DCHECK_EQ(net::NetLog::SOURCE_HTTP_STREAM_JOB,
                http_stream_job_source.type);

      HTTPStreamJobToRecordMap::iterator it =
          http_stream_job_to_record_.find(http_stream_job_source.id);
      if (it == http_stream_job_to_record_.end())
        return;
      if (!it->second.connect_start.is_null()) {
        timing.connect_start = TimeTicksToOffset(it->second.connect_start,
                                                 record);
      }
      if (!it->second.connect_end.is_null())
        timing.connect_end = TimeTicksToOffset(it->second.connect_end, record);
      if (!it->second.dns_start.is_null())
        timing.dns_start = TimeTicksToOffset(it->second.dns_start, record);
      if (!it->second.dns_end.is_null())
        timing.dns_end = TimeTicksToOffset(it->second.dns_end, record);
      if (!it->second.ssl_start.is_null())
        timing.ssl_start = TimeTicksToOffset(it->second.ssl_start, record);
      if (!it->second.ssl_end.is_null())
        timing.ssl_end = TimeTicksToOffset(it->second.ssl_end, record);
      record->socket_reused = it->second.socket_reused;
      record->socket_log_id = it->second.socket_log_id;
      break;
    }
    case net::NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST:
      if (is_begin)
        timing.send_start = TimeTicksToOffset(GetCurrentTime(), record);
      else if (is_end)
        timing.send_end = TimeTicksToOffset(GetCurrentTime(), record);
      break;
    case net::NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS:
      if (is_begin) {
        timing.receive_headers_start =
            TimeTicksToOffset(GetCurrentTime(), record);
      } else if (is_end) {
        timing.receive_headers_end =
            TimeTicksToOffset(GetCurrentTime(), record);
      }
      break;
    default:
      break;
  }
#endif  // !defined(OS_IOS)
}

void LoadTimingObserver::OnAddHTTPStreamJobEntry(
    const net::NetLog::Entry& entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool is_begin = entry.phase() == net::NetLog::PHASE_BEGIN;
  bool is_end = entry.phase() == net::NetLog::PHASE_END;

  if (entry.type() == net::NetLog::TYPE_HTTP_STREAM_JOB) {
    if (is_begin) {
      // Prevents us from passively growing the memory unbounded in
      // case something went wrong. Should not happen.
      if (http_stream_job_to_record_.size() > kMaxNumEntries) {
        LOG(WARNING) << "The load timing observer http stream job count "
                        "has grown larger than expected, resetting";
        http_stream_job_to_record_.clear();
      }

      http_stream_job_to_record_.insert(
          std::make_pair(entry.source().id, HTTPStreamJobRecord()));
    } else if (is_end) {
      http_stream_job_to_record_.erase(entry.source().id);
    }
    return;
  }

  HTTPStreamJobToRecordMap::iterator it =
      http_stream_job_to_record_.find(entry.source().id);
  if (it == http_stream_job_to_record_.end())
    return;

  switch (entry.type()) {
    case net::NetLog::TYPE_SOCKET_POOL:
      if (is_begin)
        it->second.connect_start = GetCurrentTime();
      else if (is_end)
        it->second.connect_end = GetCurrentTime();
      break;
    case net::NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB: {
      net::NetLog::Source connect_job_source;
      scoped_ptr<Value> event_params(entry.ParametersToValue());
      if (!net::NetLog::Source::FromEventParameters(event_params.get(),
                                                    &connect_job_source)) {
        NOTREACHED();
        return;
      }
      DCHECK_EQ(net::NetLog::SOURCE_CONNECT_JOB, connect_job_source.type);

      if (last_connect_job_id_ == connect_job_source.id &&
          !last_connect_job_record_.dns_start.is_null()) {
        it->second.dns_start = last_connect_job_record_.dns_start;
        it->second.dns_end = last_connect_job_record_.dns_end;
      }
      break;
    }
    case net::NetLog::TYPE_SOCKET_POOL_REUSED_AN_EXISTING_SOCKET:
      it->second.socket_reused = true;
      break;
    case net::NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET: {
      net::NetLog::Source socket_source;
      scoped_ptr<Value> event_params(entry.ParametersToValue());
      if (!net::NetLog::Source::FromEventParameters(event_params.get(),
                                                    &socket_source)) {
        NOTREACHED();
        return;
      }

      it->second.socket_log_id = socket_source.id;
      if (!it->second.socket_reused) {
        SocketToRecordMap::iterator socket_it =
            socket_to_record_.find(it->second.socket_log_id);
        if (socket_it != socket_to_record_.end() &&
            !socket_it->second.ssl_start.is_null()) {
          it->second.ssl_start = socket_it->second.ssl_start;
          it->second.ssl_end = socket_it->second.ssl_end;
        }
      }
      break;
    }
    default:
      break;
  }
}

void LoadTimingObserver::OnAddConnectJobEntry(const net::NetLog::Entry& entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool is_begin = entry.phase() == net::NetLog::PHASE_BEGIN;
  bool is_end = entry.phase() == net::NetLog::PHASE_END;

  // Manage record lifetime based on the SOCKET_POOL_CONNECT_JOB entry.
  if (entry.type() == net::NetLog::TYPE_SOCKET_POOL_CONNECT_JOB) {
    if (is_begin) {
      // Prevents us from passively growing the memory unbounded in case
      // something went wrong. Should not happen.
      if (connect_job_to_record_.size() > kMaxNumEntries) {
        LOG(WARNING) << "The load timing observer connect job count has grown "
                        "larger than expected, resetting";
        connect_job_to_record_.clear();
      }

      connect_job_to_record_.insert(
          std::make_pair(entry.source().id, ConnectJobRecord()));
    } else if (is_end) {
      ConnectJobToRecordMap::iterator it =
          connect_job_to_record_.find(entry.source().id);
      if (it != connect_job_to_record_.end()) {
        last_connect_job_id_ = it->first;
        last_connect_job_record_ = it->second;
        connect_job_to_record_.erase(it);
      }
    }
  } else if (entry.type() == net::NetLog::TYPE_HOST_RESOLVER_IMPL) {
    ConnectJobToRecordMap::iterator it =
        connect_job_to_record_.find(entry.source().id);
    if (it != connect_job_to_record_.end()) {
      if (is_begin)
        it->second.dns_start = GetCurrentTime();
      else if (is_end)
        it->second.dns_end = GetCurrentTime();
    }
  }
}

void LoadTimingObserver::OnAddSocketEntry(const net::NetLog::Entry& entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool is_begin = entry.phase() == net::NetLog::PHASE_BEGIN;
  bool is_end = entry.phase() == net::NetLog::PHASE_END;

  // Manage record lifetime based on the SOCKET_ALIVE entry.
  if (entry.type() == net::NetLog::TYPE_SOCKET_ALIVE) {
    if (is_begin) {
      // Prevents us from passively growing the memory unbounded in case
      // something went wrong. Should not happen.
      if (socket_to_record_.size() > kMaxNumEntries) {
        LOG(WARNING) << "The load timing observer socket count has grown "
                        "larger than expected, resetting";
        socket_to_record_.clear();
      }

      socket_to_record_.insert(
          std::make_pair(entry.source().id, SocketRecord()));
    } else if (is_end) {
      socket_to_record_.erase(entry.source().id);
    }
    return;
  }
  SocketToRecordMap::iterator it = socket_to_record_.find(entry.source().id);
  if (it == socket_to_record_.end())
    return;

  if (entry.type() == net::NetLog::TYPE_SSL_CONNECT) {
    if (is_begin)
      it->second.ssl_start = GetCurrentTime();
    else if (is_end)
      it->second.ssl_end = GetCurrentTime();
  }
}

base::TimeTicks LoadTimingObserver::GetCurrentTime() const {
  return base::TimeTicks::Now();
}
