// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <wininet.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/win/scoped_handle.h"
#include "chrome_frame/test/test_server.h"
#include "net/cookies/cookie_monster.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/host_resolver_proc.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestServerTest: public testing::Test {
 protected:
  virtual void SetUp() {
    PathService::Get(base::DIR_SOURCE_ROOT, &source_path_);
    source_path_ = source_path_.Append(FILE_PATH_LITERAL("chrome_frame"));
  }
  virtual void TearDown() {
  }

 public:
  const base::FilePath& source_path() const {
    return source_path_;
  }

 protected:
  base::FilePath source_path_;
};

namespace {

class ScopedInternet {
 public:
  explicit ScopedInternet(HINTERNET handle)
      : h_(handle) {
  }
  ~ScopedInternet() {
    if (h_) {
      InternetCloseHandle(h_);
    }
  }

  operator HINTERNET() {
    return h_;
  }

 protected:
  HINTERNET h_;
};

class TestURLRequest : public net::URLRequest {
 public:
  TestURLRequest(const GURL& url,
                 Delegate* delegate,
                 net::TestURLRequestContext* context)
      : net::URLRequest(url, delegate, context) {
  }
};

class UrlTaskChain {
 public:
  UrlTaskChain(const std::string& url, UrlTaskChain* next)
      : url_(url), next_(next) {
  }

  void Run() {
    EXPECT_EQ(0, delegate_.response_started_count());

    MessageLoopForIO loop;

    net::TestURLRequestContext context;
    TestURLRequest r(GURL(url_), &delegate_, &context);
    r.Start();
    EXPECT_TRUE(r.is_pending());

    MessageLoop::current()->Run();

    EXPECT_EQ(1, delegate_.response_started_count());
    EXPECT_FALSE(delegate_.received_data_before_response());
    EXPECT_NE(0, delegate_.bytes_received());
  }

  UrlTaskChain* next() const {
    return next_;
  }

  const std::string& response() const {
    return delegate_.data_received();
  }

 protected:
  std::string url_;
  net::TestDelegate delegate_;
  UrlTaskChain* next_;
};

DWORD WINAPI FetchUrl(void* param) {
  UrlTaskChain* task = reinterpret_cast<UrlTaskChain*>(param);
  while (task != NULL) {
    task->Run();
    task = task->next();
  }

  return 0;
}

struct QuitMessageHit {
  explicit QuitMessageHit(MessageLoopForUI* loop) : loop_(loop), hit_(false) {
  }

  MessageLoopForUI* loop_;
  bool hit_;
};

void QuitMessageLoop(QuitMessageHit* msg) {
  msg->hit_ = true;
  msg->loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

}  // end namespace

TEST_F(TestServerTest, TestServer) {
  // The web server needs a loop to exist on this thread during construction
  // the loop must be created before we construct the server.
  MessageLoopForUI loop;

  test_server::SimpleWebServer server(1337);
  test_server::SimpleWebServer redirected_server(server.host(), 1338);
  test_server::SimpleResponse person("/person", "Guthrie Govan!");
  server.AddResponse(&person);
  test_server::FileResponse file("/file", source_path().Append(
      FILE_PATH_LITERAL("CFInstance.js")));
  server.AddResponse(&file);
  test_server::RedirectResponse redir(
      "/redir",
      base::StringPrintf("http://%s:1338/dest",
                         redirected_server.host().c_str()));
  server.AddResponse(&redir);

  test_server::SimpleResponse dest("/dest", "Destination");
  redirected_server.AddResponse(&dest);

  // We should never hit this, but it's our way to break out of the test if
  // things start hanging.
  QuitMessageHit quit_msg(&loop);
  loop.PostDelayedTask(FROM_HERE, base::Bind(QuitMessageLoop, &quit_msg),
                       base::TimeDelta::FromSeconds(10));

  UrlTaskChain quit_task(
      base::StringPrintf("http://%s:1337/quit", server.host().c_str()), NULL);
  UrlTaskChain fnf_task(
      base::StringPrintf("http://%s:1337/404", server.host().c_str()),
      &quit_task);
  UrlTaskChain person_task(
      base::StringPrintf("http://%s:1337/person", server.host().c_str()),
      &fnf_task);
  UrlTaskChain file_task(
      base::StringPrintf("http://%s:1337/file", server.host().c_str()),
      &person_task);
  UrlTaskChain redir_task(
      base::StringPrintf("http://%s:1337/redir", server.host().c_str()),
      &file_task);

  DWORD tid = 0;
  base::win::ScopedHandle worker(::CreateThread(
      NULL, 0, FetchUrl, &redir_task, 0, &tid));
  loop.MessageLoop::Run();

  EXPECT_FALSE(quit_msg.hit_);
  if (!quit_msg.hit_) {
    EXPECT_EQ(::WaitForSingleObject(worker, 10 * 1000), WAIT_OBJECT_0);

    EXPECT_EQ(person.accessed(), 1);
    EXPECT_EQ(file.accessed(), 1);
    EXPECT_EQ(redir.accessed(), 1);

    EXPECT_TRUE(person_task.response().find("Guthrie") != std::string::npos);
    EXPECT_TRUE(file_task.response().find("function") != std::string::npos);
    EXPECT_TRUE(redir_task.response().find("Destination") != std::string::npos);
  } else {
    ::TerminateThread(worker, ~0);
  }
}
