// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/load_timing_observer.h"

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_netlog_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::TimeDelta;
using content::BrowserThread;
using net::NetLog;

class TestLoadTimingObserver : public LoadTimingObserver {
 public:
  TestLoadTimingObserver() {}
  virtual ~TestLoadTimingObserver() {}

  void IncementTime(base::TimeDelta delta) {
    current_time_ += delta;
  }

  virtual base::TimeTicks GetCurrentTime() const OVERRIDE {
    return current_time_;
  }

 private:
  base::TimeTicks current_time_;

  DISALLOW_COPY_AND_ASSIGN(TestLoadTimingObserver);
};

// Serves to identify the current thread as the IO thread.
class LoadTimingObserverTest : public testing::Test {
 public:
  LoadTimingObserverTest() : io_thread_(BrowserThread::IO, &message_loop_) {
  }

 protected:
  TestLoadTimingObserver observer_;

 private:
  MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;
};

void AddStartEntry(LoadTimingObserver& observer,
                   const NetLog::Source& source,
                   NetLog::EventType type) {
  net::NetLog::Entry entry(type,
                           source,
                           NetLog::PHASE_BEGIN,
                           NULL,
                           NetLog::LOG_BASIC);
  observer.OnAddEntry(entry);
}

void AddStartEntry(LoadTimingObserver& observer,
                   const NetLog::Source& source,
                   NetLog::EventType type,
                   const NetLog::ParametersCallback& params_callback) {
  net::NetLog::Entry entry(type,
                           source,
                           NetLog::PHASE_BEGIN,
                           &params_callback,
                           NetLog::LOG_BASIC);
  observer.OnAddEntry(entry);
}

void AddEndEntry(LoadTimingObserver& observer,
                 const NetLog::Source& source,
                 NetLog::EventType type) {
  net::NetLog::Entry entry(type,
                           source,
                           NetLog::PHASE_END,
                           NULL,
                           NetLog::LOG_BASIC);
  observer.OnAddEntry(entry);
}

void AddStartURLRequestEntries(LoadTimingObserver& observer,
                               uint32 id,
                               bool request_timing) {
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, id);
  AddStartEntry(observer, source, NetLog::TYPE_REQUEST_ALIVE);

  GURL url(base::StringPrintf("http://req%d", id));
  std::string method = "GET";
  AddStartEntry(
      observer,
      source,
      NetLog::TYPE_URL_REQUEST_START_JOB,
      base::Bind(&net::NetLogURLRequestStartCallback,
                 &url,
                 &method,
                 request_timing ? net::LOAD_ENABLE_LOAD_TIMING : 0,
                 net::LOW,
                 -1));
}

void AddEndURLRequestEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, id);
  AddEndEntry(observer, source, NetLog::TYPE_REQUEST_ALIVE);
  AddEndEntry(observer, source, NetLog::TYPE_URL_REQUEST_START_JOB);
}

void AddStartHTTPStreamJobEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_HTTP_STREAM_JOB, id);
  AddStartEntry(observer, source, NetLog::TYPE_HTTP_STREAM_JOB);
}

void AddEndHTTPStreamJobEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_HTTP_STREAM_JOB, id);
  AddEndEntry(observer, source, NetLog::TYPE_HTTP_STREAM_JOB);
}

void AddStartConnectJobEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_CONNECT_JOB, id);
  AddStartEntry(observer, source, NetLog::TYPE_SOCKET_POOL_CONNECT_JOB);
}

void AddEndConnectJobEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_CONNECT_JOB, id);
  AddEndEntry(observer, source, NetLog::TYPE_SOCKET_POOL_CONNECT_JOB);
}

void AddStartSocketEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_SOCKET, id);
  AddStartEntry(observer, source, NetLog::TYPE_SOCKET_ALIVE);
}

void AddEndSocketEntries(LoadTimingObserver& observer, uint32 id) {
  NetLog::Source source(NetLog::SOURCE_SOCKET, id);
  AddEndEntry(observer, source, NetLog::TYPE_SOCKET_ALIVE);
}

void BindURLRequestToHTTPStreamJob(LoadTimingObserver& observer,
                                   NetLog::Source url_request_source,
                                   NetLog::Source http_stream_job_source) {
  AddStartEntry(observer,
                url_request_source,
                NetLog::TYPE_HTTP_STREAM_REQUEST_BOUND_TO_JOB,
                http_stream_job_source.ToEventParametersCallback());
}

void BindHTTPStreamJobToConnectJob(LoadTimingObserver& observer,
                                   NetLog::Source& http_stream_job_source,
                                   NetLog::Source& connect_source) {
  AddStartEntry(observer,
                http_stream_job_source,
                NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB,
                connect_source.ToEventParametersCallback());
}

void BindHTTPStreamJobToSocket(LoadTimingObserver& observer,
                               NetLog::Source& http_stream_job_source,
                               NetLog::Source& socket_source) {
  AddStartEntry(observer,
                http_stream_job_source,
                NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET,
                socket_source.ToEventParametersCallback());
}

}  // namespace

// Test that net::URLRequest with no load timing flag is not processed.
TEST_F(LoadTimingObserverTest, NoLoadTimingEnabled) {
  AddStartURLRequestEntries(observer_, 0, false);
  LoadTimingObserver::URLRequestRecord* record =
      observer_.GetURLRequestRecord(0);
  ASSERT_TRUE(record == NULL);
}

// Test that URLRequestRecord is created, deleted and is not growing unbound.
TEST_F(LoadTimingObserverTest, URLRequestRecord) {
  // Create record.
  AddStartURLRequestEntries(observer_, 0, true);
  LoadTimingObserver::URLRequestRecord* record =
      observer_.GetURLRequestRecord(0);
  ASSERT_FALSE(record == NULL);

  // Collect record.
  AddEndURLRequestEntries(observer_, 0);
  record = observer_.GetURLRequestRecord(0);
  ASSERT_TRUE(record == NULL);

  // Check unbound growth.
  for (size_t i = 1; i < 1100; ++i)
    AddStartURLRequestEntries(observer_, i, true);
  record = observer_.GetURLRequestRecord(1);
  ASSERT_TRUE(record == NULL);
}

// Test that HTTPStreamJobRecord is created, deleted and is not growing unbound.
TEST_F(LoadTimingObserverTest, HTTPStreamJobRecord) {
  // Create record.
  AddStartHTTPStreamJobEntries(observer_, 0);
  ASSERT_FALSE(observer_.http_stream_job_to_record_.find(0) ==
                   observer_.http_stream_job_to_record_.end());

  // Collect record.
  AddEndHTTPStreamJobEntries(observer_, 0);
  ASSERT_TRUE(observer_.http_stream_job_to_record_.find(0) ==
                   observer_.http_stream_job_to_record_.end());

  // Check unbound growth.
  for (size_t i = 1; i < 1100; ++i)
    AddStartHTTPStreamJobEntries(observer_, i);
  ASSERT_TRUE(observer_.http_stream_job_to_record_.find(1) ==
                  observer_.http_stream_job_to_record_.end());
}

// Test that ConnectJobRecord is created, deleted and is not growing unbound.
TEST_F(LoadTimingObserverTest, ConnectJobRecord) {
  // Create record.
  AddStartConnectJobEntries(observer_, 0);
  ASSERT_FALSE(observer_.connect_job_to_record_.find(0) ==
                   observer_.connect_job_to_record_.end());

  // Collect record.
  AddEndConnectJobEntries(observer_, 0);
  ASSERT_TRUE(observer_.connect_job_to_record_.find(0) ==
                   observer_.connect_job_to_record_.end());

  // Check unbound growth.
  for (size_t i = 1; i < 1100; ++i)
    AddStartConnectJobEntries(observer_, i);
  ASSERT_TRUE(observer_.connect_job_to_record_.find(1) ==
                  observer_.connect_job_to_record_.end());
}

// Test that SocketRecord is created, deleted and is not growing unbound.
TEST_F(LoadTimingObserverTest, SocketRecord) {
  // Create record.
  AddStartSocketEntries(observer_, 0);
  ASSERT_FALSE(observer_.socket_to_record_.find(0) ==
                   observer_.socket_to_record_.end());

  // Collect record.
  AddEndSocketEntries(observer_, 0);
  ASSERT_TRUE(observer_.socket_to_record_.find(0) ==
                   observer_.socket_to_record_.end());

  // Check unbound growth.
  for (size_t i = 1; i < 1100; ++i)
    AddStartSocketEntries(observer_, i);
  ASSERT_TRUE(observer_.socket_to_record_.find(1) ==
                  observer_.socket_to_record_.end());
}

// Test that basic time is set to the request.
TEST_F(LoadTimingObserverTest, BaseTicks) {
  observer_.IncementTime(TimeDelta::FromSeconds(1));
  AddStartURLRequestEntries(observer_, 0, true);

  LoadTimingObserver::URLRequestRecord* record =
      observer_.GetURLRequestRecord(0);
  ASSERT_EQ(1000000, record->base_ticks.ToInternalValue());
}

// Test proxy time detection.
TEST_F(LoadTimingObserverTest, ProxyTime) {
  observer_.IncementTime(TimeDelta::FromSeconds(1));

  AddStartURLRequestEntries(observer_, 0, true);
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);

  observer_.IncementTime(TimeDelta::FromSeconds(2));
  AddStartEntry(observer_, source, NetLog::TYPE_PROXY_SERVICE);
  observer_.IncementTime(TimeDelta::FromSeconds(3));
  AddEndEntry(observer_, source, NetLog::TYPE_PROXY_SERVICE);

  LoadTimingObserver::URLRequestRecord* record =
      observer_.GetURLRequestRecord(0);
  ASSERT_EQ(2000, record->timing.proxy_start);
  ASSERT_EQ(5000, record->timing.proxy_end);
}

// Test connect time detection.
TEST_F(LoadTimingObserverTest, ConnectTime) {
  observer_.IncementTime(TimeDelta::FromSeconds(1));

  AddStartURLRequestEntries(observer_, 0, true);
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);

  NetLog::Source http_stream_job_source(NetLog::SOURCE_HTTP_STREAM_JOB, 1);
  AddStartHTTPStreamJobEntries(observer_, 1);

  observer_.IncementTime(TimeDelta::FromSeconds(2));
  AddStartEntry(observer_, http_stream_job_source, NetLog::TYPE_SOCKET_POOL);
  observer_.IncementTime(TimeDelta::FromSeconds(3));
  AddEndEntry(observer_, http_stream_job_source, NetLog::TYPE_SOCKET_POOL);

  BindURLRequestToHTTPStreamJob(observer_, source, http_stream_job_source);

  LoadTimingObserver::URLRequestRecord* record =
      observer_.GetURLRequestRecord(0);
  ASSERT_EQ(2000, record->timing.connect_start);
  ASSERT_EQ(5000, record->timing.connect_end);
}

// Test dns time detection.
TEST_F(LoadTimingObserverTest, DnsTime) {
  // Start request.
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);
  AddStartURLRequestEntries(observer_, 0, true);
  observer_.IncementTime(TimeDelta::FromSeconds(1));

  // Add resolver entry.
  AddStartConnectJobEntries(observer_, 1);
  NetLog::Source connect_source(NetLog::SOURCE_CONNECT_JOB, 1);
  AddStartEntry(observer_, connect_source, NetLog::TYPE_HOST_RESOLVER_IMPL);
  observer_.IncementTime(TimeDelta::FromSeconds(2));
  AddEndEntry(observer_, connect_source, NetLog::TYPE_HOST_RESOLVER_IMPL);
  AddEndConnectJobEntries(observer_, 1);

  NetLog::Source http_stream_job_source(NetLog::SOURCE_HTTP_STREAM_JOB, 2);
  AddStartHTTPStreamJobEntries(observer_, 2);

  BindHTTPStreamJobToConnectJob(observer_, http_stream_job_source,
                                connect_source);
  BindURLRequestToHTTPStreamJob(observer_, source, http_stream_job_source);

  LoadTimingObserver::URLRequestRecord* record =
      observer_.GetURLRequestRecord(0);
  ASSERT_EQ(1000, record->timing.dns_start);
  ASSERT_EQ(3000, record->timing.dns_end);
}

// Test send time detection.
TEST_F(LoadTimingObserverTest, SendTime) {
  // Start request.
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);
  AddStartURLRequestEntries(observer_, 0, true);
  observer_.IncementTime(TimeDelta::FromSeconds(2));

  // Add send request entry.
  AddStartEntry(observer_, source, NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST);
  observer_.IncementTime(TimeDelta::FromSeconds(5));
  AddEndEntry(observer_, source, NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST);

  LoadTimingObserver::URLRequestRecord* record =
      observer_.GetURLRequestRecord(0);
  ASSERT_EQ(2000, record->timing.send_start);
  ASSERT_EQ(7000, record->timing.send_end);
}

// Test receive time detection.
TEST_F(LoadTimingObserverTest, ReceiveTime) {
  // Start request.
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);
  AddStartURLRequestEntries(observer_, 0, true);
  observer_.IncementTime(TimeDelta::FromSeconds(2));

  // Add send request entry.
  AddStartEntry(observer_, source, NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS);
  observer_.IncementTime(TimeDelta::FromSeconds(5));
  AddEndEntry(observer_, source, NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS);

  LoadTimingObserver::URLRequestRecord* record =
      observer_.GetURLRequestRecord(0);
  ASSERT_EQ(2000, record->timing.receive_headers_start);
  ASSERT_EQ(7000, record->timing.receive_headers_end);
}

// Test ssl time detection.
TEST_F(LoadTimingObserverTest, SslTime) {
  // Start request.
  NetLog::Source source(NetLog::SOURCE_URL_REQUEST, 0);
  AddStartURLRequestEntries(observer_, 0, true);
  observer_.IncementTime(TimeDelta::FromSeconds(1));

  // Add resolver entry.
  AddStartSocketEntries(observer_, 1);
  NetLog::Source socket_source(NetLog::SOURCE_SOCKET, 1);
  AddStartEntry(observer_, socket_source, NetLog::TYPE_SSL_CONNECT);
  observer_.IncementTime(TimeDelta::FromSeconds(2));
  AddEndEntry(observer_, socket_source, NetLog::TYPE_SSL_CONNECT);

  NetLog::Source http_stream_job_source(NetLog::SOURCE_HTTP_STREAM_JOB, 2);
  AddStartHTTPStreamJobEntries(observer_, 2);

  BindHTTPStreamJobToSocket(observer_, http_stream_job_source, socket_source);
  BindURLRequestToHTTPStreamJob(observer_, source, http_stream_job_source);

  LoadTimingObserver::URLRequestRecord* record =
      observer_.GetURLRequestRecord(0);
  ASSERT_EQ(1000, record->timing.ssl_start);
  ASSERT_EQ(3000, record->timing.ssl_end);
}
