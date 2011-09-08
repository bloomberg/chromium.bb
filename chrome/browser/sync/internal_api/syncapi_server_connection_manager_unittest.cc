// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/internal_api/syncapi_server_connection_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/browser/sync/internal_api/http_post_provider_factory.h"
#include "chrome/browser/sync/internal_api/http_post_provider_interface.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using browser_sync::HttpResponse;
using browser_sync::ServerConnectionManager;
using browser_sync::ScopedServerStatusWatcher;

namespace sync_api {
namespace {

class BlockingHttpPost : public HttpPostProviderInterface {
 public:
  BlockingHttpPost() : wait_for_abort_(false, false) {}
  virtual ~BlockingHttpPost() {}

  virtual void SetUserAgent(const char* user_agent) OVERRIDE {}
  virtual void SetExtraRequestHeaders(const char* headers) OVERRIDE {}
  virtual void SetURL(const char* url, int port) OVERRIDE {}
  virtual void SetPostPayload(const char* content_type,
                              int content_length,
                              const char* content) OVERRIDE {}
  virtual bool MakeSynchronousPost(int* error_code, int* response_code)
      OVERRIDE {
    wait_for_abort_.TimedWait(TimeDelta::FromMilliseconds(
        TestTimeouts::action_max_timeout_ms()));
    *error_code = net::ERR_ABORTED;
    return false;
  }
  virtual int GetResponseContentLength() const OVERRIDE {
    return 0;
  }
  virtual const char* GetResponseContent() const OVERRIDE {
    return "";
  }
  virtual const std::string GetResponseHeaderValue(
      const std::string& name) const OVERRIDE {
    return "";
  }
  virtual void Abort() OVERRIDE {
    wait_for_abort_.Signal();
  }
 private:
  base::WaitableEvent wait_for_abort_;
};

class BlockingHttpPostFactory : public HttpPostProviderFactory {
 public:
  virtual ~BlockingHttpPostFactory() {}
  virtual HttpPostProviderInterface* Create() OVERRIDE {
    return new BlockingHttpPost();
  }
  virtual void Destroy(HttpPostProviderInterface* http) OVERRIDE {
    delete http;
  }
};

}  // namespace

TEST(SyncAPIServerConnectionManagerTest, EarlyAbortPost) {
  SyncAPIServerConnectionManager server(
      "server", 0, true, "1", new BlockingHttpPostFactory());

  ServerConnectionManager::PostBufferParams params;
  ScopedServerStatusWatcher watcher(&server, &params.response);

  server.TerminateAllIO();
  bool result = server.PostBufferToPath(
      &params, "/testpath", "testauth", &watcher);

  EXPECT_FALSE(result);
  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE,
            params.response.server_status);
}

TEST(SyncAPIServerConnectionManagerTest, EarlyAbortCheckTime) {
  SyncAPIServerConnectionManager server(
      "server", 0, true, "1", new BlockingHttpPostFactory());
  int32 time = 0;
  server.TerminateAllIO();
  bool result = server.CheckTime(&time);
  EXPECT_FALSE(result);
}

TEST(SyncAPIServerConnectionManagerTest, AbortPost) {
  SyncAPIServerConnectionManager server(
      "server", 0, true, "1", new BlockingHttpPostFactory());

  ServerConnectionManager::PostBufferParams params;
  ScopedServerStatusWatcher watcher(&server, &params.response);

  base::Thread abort_thread("Test_AbortThread");
  ASSERT_TRUE(abort_thread.Start());
  abort_thread.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ServerConnectionManager::TerminateAllIO,
                 base::Unretained(&server)),
      TestTimeouts::tiny_timeout_ms());

  bool result = server.PostBufferToPath(
      &params, "/testpath", "testauth", &watcher);

  EXPECT_FALSE(result);
  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE,
            params.response.server_status);
  abort_thread.Stop();
}

TEST(SyncAPIServerConnectionManagerTest, AbortCheckTime) {
  SyncAPIServerConnectionManager server(
      "server", 0, true, "1", new BlockingHttpPostFactory());

  base::Thread abort_thread("Test_AbortThread");
  ASSERT_TRUE(abort_thread.Start());
  abort_thread.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ServerConnectionManager::TerminateAllIO,
                 base::Unretained(&server)),
      TestTimeouts::tiny_timeout_ms());

  int32 time = 0;
  bool result = server.CheckTime(&time);
  EXPECT_FALSE(result);
  abort_thread.Stop();
}

}  // namespace sync_api
