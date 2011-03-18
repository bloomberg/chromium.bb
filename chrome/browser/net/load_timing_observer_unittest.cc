// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/load_timing_observer.h"

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/time.h"
#include "content/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_netlog_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using net::NetLog;
using base::TimeDelta;

// Serves to Identify the current thread as the IO thread.
class LoadTimingObserverTest : public testing::Test {
 public:
  LoadTimingObserverTest() : io_thread_(BrowserThread::IO, &message_loop_) {
  }

 private:
  MessageLoop message_loop_;
  BrowserThread io_thread_;
};

base::TimeTicks current_time;

void AddStartEntry(LoadTimingObserver& observer,
                   const NetLog::Source& source,
                   NetLog::EventType type,
                   NetLog::EventParameters* params) {
  observer.OnAddEntry(type, current_time, source, NetLog::PHASE_BEGIN, params);
}

void AddEndEntry(LoadTimingObserver& observer,
                 const NetLog::Source& source,
                 NetLog::EventType type,
                 NetLog::EventParameters* params) {
  observer.OnAddEntry(type, current_time, source, NetLog::PHASE_END, params);
}

void AddStartURLRequestEntries(LoadTimingObserver& observer,
                               uint32 id,
                               bool request_timing) {
  scoped_refptr<net::URLRequestStartEventParameters> params(
      new net::URLRequestStartEventParameters(
          GURL(StringPrintf("http://req%d", id)),
          "GET",
          request_timing ? net::LOAD_ENABLE_LOAD_TIMING : 0,
          net::LOW));
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, id);
  AddStartEntry(observer, source, NetLog::TYPE_REQUEST_ALIVE, NULL);
  AddStartEntry(observer,
                source,
                NetLog::TYPE_URL_REQUEST_START_JOB,
                params.get());
}

void AddEndURLRequestEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, id);
  AddEndEntry(observer, source, NetLog::TYPE_REQUEST_ALIVE, NULL);
  AddEndEntry(observer,
              source,
              NetLog::TYPE_URL_REQUEST_START_JOB,
              NULL);
}

void AddStartHTTPStreamJobEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_HTTP_STREAM_JOB, id);
  AddStartEntry(observer, source, NetLog::TYPE_HTTP_STREAM_JOB, NULL);
}

void AddEndHTTPStreamJobEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_HTTP_STREAM_JOB, id);
  AddEndEntry(observer, source, NetLog::TYPE_HTTP_STREAM_JOB, NULL);
}

void AddStartConnectJobEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_CONNECT_JOB, id);
  AddStartEntry(observer, source, NetLog::TYPE_SOCKET_POOL_CONNECT_JOB, NULL);
}

void AddEndConnectJobEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_CONNECT_JOB, id);
  AddEndEntry(observer, source, NetLog::TYPE_SOCKET_POOL_CONNECT_JOB, NULL);
}

void AddStartSocketEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_SOCKET, id);
  AddStartEntry(observer, source, NetLog::TYPE_SOCKET_ALIVE, NULL);
}

void AddEndSocketEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_SOCKET, id);
  AddEndEntry(observer, source, NetLog::TYPE_SOCKET_ALIVE, NULL);
}

void BindURLRequestToHTTPStreamJob(LoadTimingObserver& observer,
                                   NetLog::Source url_request_source,
                                   NetLog::Source http_stream_job_source) {
  scoped_refptr<net::NetLogSourceParameter> params(
      new net::NetLogSourceParameter("source_dependency",
                                     http_stream_job_source));
  AddStartEntry(observer,
                url_request_source,
                NetLog::TYPE_HTTP_STREAM_REQUEST_BOUND_TO_JOB,
                params.get());
}

void BindHTTPStreamJobToConnectJob(LoadTimingObserver& observer,
                                   NetLog::Source& http_stream_job_source,
                                   NetLog::Source& connect_source) {
  scoped_refptr<net::NetLogSourceParameter> params(
      new net::NetLogSourceParameter("source_dependency", connect_source));
  AddStartEntry(observer,
                http_stream_job_source,
                NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB,
                params.get());
}

void BindHTTPStreamJobToSocket(LoadTimingObserver& observer,
                               NetLog::Source& http_stream_job_source,
                               NetLog::Source& socket_source) {
  scoped_refptr<net::NetLogSourceParameter> params(
      new net::NetLogSourceParameter("source_dependency", socket_source));
  AddStartEntry(observer,
                http_stream_job_source,
                NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET,
                params.get());
}

}  // namespace

// Test that net::URLRequest with no load timing flag is not processed.
TEST_F(LoadTimingObserverTest, NoLoadTimingEnabled) {
  LoadTimingObserver observer;

  AddStartURLRequestEntries(observer, 0, false);
  LoadTimingObserver::URLRequestRecord* record =
      observer.GetURLRequestRecord(0);
  ASSERT_TRUE(record == NULL);
}

// Test that URLRequestRecord is created, deleted and is not growing unbound.
TEST_F(LoadTimingObserverTest, URLRequestRecord) {
  LoadTimingObserver observer;

  // Create record.
  AddStartURLRequestEntries(observer, 0, true);
  LoadTimingObserver::URLRequestRecord* record =
      observer.GetURLRequestRecord(0);
  ASSERT_FALSE(record == NULL);

  // Collect record.
  AddEndURLRequestEntries(observer, 0);
  record = observer.GetURLRequestRecord(0);
  ASSERT_TRUE(record == NULL);

  // Check unbound growth.
  for (size_t i = 1; i < 1100; ++i)
    AddStartURLRequestEntries(observer, i, true);
  record = observer.GetURLRequestRecord(1);
  ASSERT_TRUE(record == NULL);
}

// Test that HTTPStreamJobRecord is created, deleted and is not growing unbound.
TEST_F(LoadTimingObserverTest, HTTPStreamJobRecord) {
  LoadTimingObserver observer;

  // Create record.
  AddStartHTTPStreamJobEntries(observer, 0);
  ASSERT_FALSE(observer.http_stream_job_to_record_.find(0) ==
                   observer.http_stream_job_to_record_.end());

  // Collect record.
  AddEndHTTPStreamJobEntries(observer, 0);
  ASSERT_TRUE(observer.http_stream_job_to_record_.find(0) ==
                   observer.http_stream_job_to_record_.end());

  // Check unbound growth.
  for (size_t i = 1; i < 1100; ++i)
    AddStartHTTPStreamJobEntries(observer, i);
  ASSERT_TRUE(observer.http_stream_job_to_record_.find(1) ==
                  observer.http_stream_job_to_record_.end());
}

// Test that ConnectJobRecord is created, deleted and is not growing unbound.
TEST_F(LoadTimingObserverTest, ConnectJobRecord) {
  LoadTimingObserver observer;

  // Create record.
  AddStartConnectJobEntries(observer, 0);
  ASSERT_FALSE(observer.connect_job_to_record_.find(0) ==
                   observer.connect_job_to_record_.end());

  // Collect record.
  AddEndConnectJobEntries(observer, 0);
  ASSERT_TRUE(observer.connect_job_to_record_.find(0) ==
                   observer.connect_job_to_record_.end());

  // Check unbound growth.
  for (size_t i = 1; i < 1100; ++i)
    AddStartConnectJobEntries(observer, i);
  ASSERT_TRUE(observer.connect_job_to_record_.find(1) ==
                  observer.connect_job_to_record_.end());
}

// Test that SocketRecord is created, deleted and is not growing unbound.
TEST_F(LoadTimingObserverTest, SocketRecord) {
  LoadTimingObserver observer;

  // Create record.
  AddStartSocketEntries(observer, 0);
  ASSERT_FALSE(observer.socket_to_record_.find(0) ==
                   observer.socket_to_record_.end());

  // Collect record.
  AddEndSocketEntries(observer, 0);
  ASSERT_TRUE(observer.socket_to_record_.find(0) ==
                   observer.socket_to_record_.end());


  // Check unbound growth.
  for (size_t i = 1; i < 1100; ++i)
    AddStartSocketEntries(observer, i);
  ASSERT_TRUE(observer.socket_to_record_.find(1) ==
                  observer.socket_to_record_.end());
}

// Test that basic time is set to the request.
TEST_F(LoadTimingObserverTest, BaseTicks) {
  LoadTimingObserver observer;
  current_time += TimeDelta::FromSeconds(1);
  AddStartURLRequestEntries(observer, 0, true);

  LoadTimingObserver::URLRequestRecord* record =
      observer.GetURLRequestRecord(0);
  ASSERT_EQ(1000000, record->base_ticks.ToInternalValue());
}

// Test proxy time detection.
TEST_F(LoadTimingObserverTest, ProxyTime) {
  LoadTimingObserver observer;
  current_time += TimeDelta::FromSeconds(1);

  AddStartURLRequestEntries(observer, 0, true);
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);

  current_time += TimeDelta::FromSeconds(2);
  AddStartEntry(observer, source, NetLog::TYPE_PROXY_SERVICE, NULL);
  current_time += TimeDelta::FromSeconds(3);
  AddEndEntry(observer, source, NetLog::TYPE_PROXY_SERVICE, NULL);

  LoadTimingObserver::URLRequestRecord* record =
      observer.GetURLRequestRecord(0);
  ASSERT_EQ(2000, record->timing.proxy_start);
  ASSERT_EQ(5000, record->timing.proxy_end);
}

// Test connect time detection.
TEST_F(LoadTimingObserverTest, ConnectTime) {
  LoadTimingObserver observer;
  current_time += TimeDelta::FromSeconds(1);

  AddStartURLRequestEntries(observer, 0, true);
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);

  NetLog::Source http_stream_job_source(NetLog::SOURCE_HTTP_STREAM_JOB, 1);
  AddStartHTTPStreamJobEntries(observer, 1);

  current_time += TimeDelta::FromSeconds(2);
  AddStartEntry(observer, http_stream_job_source, NetLog::TYPE_SOCKET_POOL,
                NULL);
  current_time += TimeDelta::FromSeconds(3);
  AddEndEntry(observer, http_stream_job_source, NetLog::TYPE_SOCKET_POOL, NULL);

  BindURLRequestToHTTPStreamJob(observer, source, http_stream_job_source);

  LoadTimingObserver::URLRequestRecord* record =
      observer.GetURLRequestRecord(0);
  ASSERT_EQ(2000, record->timing.connect_start);
  ASSERT_EQ(5000, record->timing.connect_end);
}

// Test dns time detection.
TEST_F(LoadTimingObserverTest, DnsTime) {
  LoadTimingObserver observer;

  // Start request.
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);
  AddStartURLRequestEntries(observer, 0, true);
  current_time += TimeDelta::FromSeconds(1);

  // Add resolver entry.
  AddStartConnectJobEntries(observer, 1);
  NetLog::Source connect_source(NetLog::SOURCE_CONNECT_JOB, 1);
  AddStartEntry(observer,
                connect_source,
                NetLog::TYPE_HOST_RESOLVER_IMPL,
                NULL);
  current_time += TimeDelta::FromSeconds(2);
  AddEndEntry(observer, connect_source, NetLog::TYPE_HOST_RESOLVER_IMPL, NULL);
  AddEndConnectJobEntries(observer, 1);

  NetLog::Source http_stream_job_source(NetLog::SOURCE_HTTP_STREAM_JOB, 2);
  AddStartHTTPStreamJobEntries(observer, 2);

  BindHTTPStreamJobToConnectJob(observer, http_stream_job_source,
                                connect_source);
  BindURLRequestToHTTPStreamJob(observer, source, http_stream_job_source);

  LoadTimingObserver::URLRequestRecord* record =
      observer.GetURLRequestRecord(0);
  ASSERT_EQ(1000, record->timing.dns_start);
  ASSERT_EQ(3000, record->timing.dns_end);
}

// Test send time detection.
TEST_F(LoadTimingObserverTest, SendTime) {
  LoadTimingObserver observer;

  // Start request.
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);
  AddStartURLRequestEntries(observer, 0, true);
  current_time += TimeDelta::FromSeconds(2);

  // Add send request entry.
  AddStartEntry(observer,
                source,
                NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST,
                NULL);
  current_time += TimeDelta::FromSeconds(5);
  AddEndEntry(observer,
              source,
              NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST,
              NULL);

  LoadTimingObserver::URLRequestRecord* record =
      observer.GetURLRequestRecord(0);
  ASSERT_EQ(2000, record->timing.send_start);
  ASSERT_EQ(7000, record->timing.send_end);
}

// Test receive time detection.
TEST_F(LoadTimingObserverTest, ReceiveTime) {
  LoadTimingObserver observer;

  // Start request.
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);
  AddStartURLRequestEntries(observer, 0, true);
  current_time += TimeDelta::FromSeconds(2);

  // Add send request entry.
  AddStartEntry(observer,
                source,
                NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS,
                NULL);
  current_time += TimeDelta::FromSeconds(5);
  AddEndEntry(observer,
              source,
              NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS,
              NULL);

  LoadTimingObserver::URLRequestRecord* record =
      observer.GetURLRequestRecord(0);
  ASSERT_EQ(2000, record->timing.receive_headers_start);
  ASSERT_EQ(7000, record->timing.receive_headers_end);
}

// Test ssl time detection.
TEST_F(LoadTimingObserverTest, SslTime) {
  LoadTimingObserver observer;

  // Start request.
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);
  AddStartURLRequestEntries(observer, 0, true);
  current_time += TimeDelta::FromSeconds(1);

  // Add resolver entry.
  AddStartSocketEntries(observer, 1);
  NetLog::Source socket_source(NetLog::SOURCE_SOCKET, 1);
  AddStartEntry(observer, socket_source, NetLog::TYPE_SSL_CONNECT, NULL);
  current_time += TimeDelta::FromSeconds(2);
  AddEndEntry(observer, socket_source, NetLog::TYPE_SSL_CONNECT, NULL);

  NetLog::Source http_stream_job_source(NetLog::SOURCE_HTTP_STREAM_JOB, 2);
  AddStartHTTPStreamJobEntries(observer, 2);

  BindHTTPStreamJobToSocket(observer, http_stream_job_source, socket_source);
  BindURLRequestToHTTPStreamJob(observer, source, http_stream_job_source);

  LoadTimingObserver::URLRequestRecord* record =
      observer.GetURLRequestRecord(0);
  ASSERT_EQ(1000, record->timing.ssl_start);
  ASSERT_EQ(3000, record->timing.ssl_end);
}
