// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/http/http_response_headers.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_NSS)
#include "net/ocsp/nss_ocsp.h"
#endif

using base::Time;
using base::TimeDelta;

// TODO(eroman): Add a regression test for http://crbug.com/40505.

namespace {

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

class TestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  explicit TestURLRequestContextGetter(
      base::MessageLoopProxy* io_message_loop_proxy)
          : io_message_loop_proxy_(io_message_loop_proxy) {
  }
  virtual net::URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestURLRequestContext();
    return context_;
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return io_message_loop_proxy_;
  }

 protected:
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

 private:
  ~TestURLRequestContextGetter() {}

  scoped_refptr<net::URLRequestContext> context_;
};

class URLFetcherTest : public testing::Test, public URLFetcher::Delegate {
 public:
  URLFetcherTest() : fetcher_(NULL) { }

  // Creates a URLFetcher, using the program's main thread to do IO.
  virtual void CreateFetcher(const GURL& url);

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy() {
    return io_message_loop_proxy_;
  }

 protected:
  virtual void SetUp() {
    testing::Test::SetUp();

    io_message_loop_proxy_ = base::MessageLoopProxy::CreateForCurrentThread();

    // Ensure that any plugin operations done by other tests are cleaned up.
    ChromePluginLib::UnloadAllPlugins();
#if defined(USE_NSS)
    net::EnsureOCSPInit();
#endif
  }

  virtual void TearDown() {
#if defined(USE_NSS)
    net::ShutdownOCSP();
#endif
  }

  // URLFetcher is designed to run on the main UI thread, but in our tests
  // we assume that the current thread is the IO thread where the URLFetcher
  // dispatches its requests to.  When we wish to simulate being used from
  // a UI thread, we dispatch a worker thread to do so.
  MessageLoopForIO io_loop_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  URLFetcher* fetcher_;
};

// Version of URLFetcherTest that does a POST instead
class URLFetcherPostTest : public URLFetcherTest {
 public:
  virtual void CreateFetcher(const GURL& url);

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
};

// Version of URLFetcherTest that tests headers.
class URLFetcherHeadersTest : public URLFetcherTest {
 public:
  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
};

// Version of URLFetcherTest that tests overload protection.
class URLFetcherProtectTest : public URLFetcherTest {
 public:
  virtual void CreateFetcher(const GURL& url);
  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
 private:
  Time start_time_;
};

// Version of URLFetcherTest that tests overload protection, when responses
// passed through.
class URLFetcherProtectTestPassedThrough : public URLFetcherTest {
 public:
  virtual void CreateFetcher(const GURL& url);
  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
 private:
  Time start_time_;
};

// Version of URLFetcherTest that tests bad HTTPS requests.
class URLFetcherBadHTTPSTest : public URLFetcherTest {
 public:
  URLFetcherBadHTTPSTest();

  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 private:
  FilePath cert_dir_;
};

// Version of URLFetcherTest that tests request cancellation on shutdown.
class URLFetcherCancelTest : public URLFetcherTest {
 public:
  virtual void CreateFetcher(const GURL& url);
  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  void CancelRequest();
};

// Version of TestURLRequestContext that posts a Quit task to the IO
// thread once it is deleted.
class CancelTestURLRequestContext : public TestURLRequestContext {
  virtual ~CancelTestURLRequestContext() {
    // The d'tor should execute in the IO thread. Post the quit task to the
    // current thread.
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }
};

class CancelTestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  explicit CancelTestURLRequestContextGetter(
      base::MessageLoopProxy* io_message_loop_proxy)
      : io_message_loop_proxy_(io_message_loop_proxy),
        context_created_(false, false) {
  }
  virtual net::URLRequestContext* GetURLRequestContext() {
    if (!context_) {
      context_ = new CancelTestURLRequestContext();
      context_created_.Signal();
    }
    return context_;
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return io_message_loop_proxy_;
  }
  void WaitForContextCreation() {
    context_created_.Wait();
  }

 private:
  ~CancelTestURLRequestContextGetter() {}

  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  base::WaitableEvent context_created_;
  scoped_refptr<net::URLRequestContext> context_;
};

// Version of URLFetcherTest that tests retying the same request twice.
class URLFetcherMultipleAttemptTest : public URLFetcherTest {
 public:
  // URLFetcher::Delegate
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
 private:
  std::string data_;
};

// Wrapper that lets us call CreateFetcher() on a thread of our choice.  We
// could make URLFetcherTest refcounted and use PostTask(FROM_HERE.. ) to call
// CreateFetcher() directly, but the ownership of the URLFetcherTest is a bit
// confusing in that case because GTest doesn't know about the refcounting.
// It's less confusing to just do it this way.
class FetcherWrapperTask : public Task {
 public:
  FetcherWrapperTask(URLFetcherTest* test, const GURL& url)
      : test_(test), url_(url) { }
  virtual void Run() {
    test_->CreateFetcher(url_);
  }

 private:
  URLFetcherTest* test_;
  GURL url_;
};

void URLFetcherTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcher(url, URLFetcher::GET, this);
  fetcher_->set_request_context(new TestURLRequestContextGetter(
      io_message_loop_proxy()));
  fetcher_->Start();
}

void URLFetcherTest::OnURLFetchComplete(const URLFetcher* source,
                                        const GURL& url,
                                        const net::URLRequestStatus& status,
                                        int response_code,
                                        const ResponseCookies& cookies,
                                        const std::string& data) {
  EXPECT_TRUE(status.is_success());
  EXPECT_EQ(200, response_code);  // HTTP OK
  EXPECT_FALSE(data.empty());

  delete fetcher_;  // Have to delete this here and not in the destructor,
                    // because the destructor won't necessarily run on the
                    // same thread that CreateFetcher() did.

  io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  // If the current message loop is not the IO loop, it will be shut down when
  // the main loop returns and this thread subsequently goes out of scope.
}

void URLFetcherPostTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcher(url, URLFetcher::POST, this);
  fetcher_->set_request_context(new TestURLRequestContextGetter(
      io_message_loop_proxy()));
  fetcher_->set_upload_data("application/x-www-form-urlencoded",
                            "bobsyeruncle");
  fetcher_->Start();
}

void URLFetcherPostTest::OnURLFetchComplete(const URLFetcher* source,
                                            const GURL& url,
                                            const net::URLRequestStatus& status,
                                            int response_code,
                                            const ResponseCookies& cookies,
                                            const std::string& data) {
  EXPECT_EQ(std::string("bobsyeruncle"), data);
  URLFetcherTest::OnURLFetchComplete(source, url, status, response_code,
                                     cookies, data);
}

void URLFetcherHeadersTest::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  std::string header;
  EXPECT_TRUE(source->response_headers()->GetNormalizedHeader("cache-control",
                                                              &header));
  EXPECT_EQ("private", header);
  URLFetcherTest::OnURLFetchComplete(source, url, status, response_code,
                                     cookies, data);
}

void URLFetcherProtectTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcher(url, URLFetcher::GET, this);
  fetcher_->set_request_context(new TestURLRequestContextGetter(
      io_message_loop_proxy()));
  start_time_ = Time::Now();
  fetcher_->set_max_retries(11);
  fetcher_->Start();
}

void URLFetcherProtectTest::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  const TimeDelta one_second = TimeDelta::FromMilliseconds(1000);
  if (response_code >= 500) {
    // Now running ServerUnavailable test.
    // It takes more than 1 second to finish all 11 requests.
    EXPECT_TRUE(Time::Now() - start_time_ >= one_second);
    EXPECT_TRUE(status.is_success());
    EXPECT_FALSE(data.empty());
    delete fetcher_;
    io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  } else {
    // Now running Overload test.
    static int count = 0;
    count++;
    if (count < 20) {
      fetcher_->Start();
    } else {
      // We have already sent 20 requests continuously. And we expect that
      // it takes more than 1 second due to the overload protection settings.
      EXPECT_TRUE(Time::Now() - start_time_ >= one_second);
      URLFetcherTest::OnURLFetchComplete(source, url, status, response_code,
                                         cookies, data);
    }
  }
}

void URLFetcherProtectTestPassedThrough::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcher(url, URLFetcher::GET, this);
  fetcher_->set_request_context(new TestURLRequestContextGetter(
      io_message_loop_proxy()));
  fetcher_->set_automatically_retry_on_5xx(false);
  start_time_ = Time::Now();
  fetcher_->set_max_retries(11);
  fetcher_->Start();
}

void URLFetcherProtectTestPassedThrough::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  const TimeDelta one_minute = TimeDelta::FromMilliseconds(60000);
  if (response_code >= 500) {
    // Now running ServerUnavailable test.
    // It should get here on the first attempt, so almost immediately and
    // *not* to attempt to execute all 11 requests (2.5 minutes).
    EXPECT_TRUE(Time::Now() - start_time_ < one_minute);
    EXPECT_TRUE(status.is_success());
    // Check that suggested back off time is bigger than 0.
    EXPECT_GT(fetcher_->backoff_delay().InMicroseconds(), 0);
    EXPECT_FALSE(data.empty());
  } else {
    // We should not get here!
    ADD_FAILURE();
  }

  delete fetcher_;
  io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}


URLFetcherBadHTTPSTest::URLFetcherBadHTTPSTest() {
  PathService::Get(base::DIR_SOURCE_ROOT, &cert_dir_);
  cert_dir_ = cert_dir_.AppendASCII("chrome");
  cert_dir_ = cert_dir_.AppendASCII("test");
  cert_dir_ = cert_dir_.AppendASCII("data");
  cert_dir_ = cert_dir_.AppendASCII("ssl");
  cert_dir_ = cert_dir_.AppendASCII("certificates");
}

// The "server certificate expired" error should result in automatic
// cancellation of the request by
// net::URLRequest::Delegate::OnSSLCertificateError.
void URLFetcherBadHTTPSTest::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // This part is different from URLFetcherTest::OnURLFetchComplete
  // because this test expects the request to be cancelled.
  EXPECT_EQ(net::URLRequestStatus::CANCELED, status.status());
  EXPECT_EQ(net::ERR_ABORTED, status.os_error());
  EXPECT_EQ(-1, response_code);
  EXPECT_TRUE(cookies.empty());
  EXPECT_TRUE(data.empty());

  // The rest is the same as URLFetcherTest::OnURLFetchComplete.
  delete fetcher_;
  io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

void URLFetcherCancelTest::CreateFetcher(const GURL& url) {
  fetcher_ = new URLFetcher(url, URLFetcher::GET, this);
  CancelTestURLRequestContextGetter* context_getter =
      new CancelTestURLRequestContextGetter(io_message_loop_proxy());
  fetcher_->set_request_context(context_getter);
  fetcher_->set_max_retries(2);
  fetcher_->Start();
  // We need to wait for the creation of the net::URLRequestContext, since we
  // rely on it being destroyed as a signal to end the test.
  context_getter->WaitForContextCreation();
  CancelRequest();
}

void URLFetcherCancelTest::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // We should have cancelled the request before completion.
  ADD_FAILURE();
  delete fetcher_;
  io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

void URLFetcherCancelTest::CancelRequest() {
  delete fetcher_;
  // The URLFetcher's test context will post a Quit task once it is
  // deleted. So if this test simply hangs, it means cancellation
  // did not work.
}

void URLFetcherMultipleAttemptTest::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  EXPECT_TRUE(status.is_success());
  EXPECT_EQ(200, response_code);  // HTTP OK
  EXPECT_FALSE(data.empty());
  if (!data.empty() && data_.empty()) {
    data_ = data;
    fetcher_->Start();
  } else {
    EXPECT_EQ(data, data_);
    delete fetcher_;  // Have to delete this here and not in the destructor,
                      // because the destructor won't necessarily run on the
                      // same thread that CreateFetcher() did.

    io_message_loop_proxy()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    // If the current message loop is not the IO loop, it will be shut down when
    // the main loop returns and this thread subsequently goes out of scope.
  }
}

TEST_F(URLFetcherTest, SameThreadsTest) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Create the fetcher on the main thread.  Since IO will happen on the main
  // thread, this will test URLFetcher's ability to do everything on one
  // thread.
  CreateFetcher(test_server.GetURL("defaultresponse"));

  MessageLoop::current()->Run();
}

#if defined(OS_MACOSX)
// SIGSEGV on Mac: http://crbug.com/60426
TEST_F(URLFetcherTest, DISABLED_DifferentThreadsTest) {
#else
TEST_F(URLFetcherTest, DifferentThreadsTest) {
#endif
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Create a separate thread that will create the URLFetcher.  The current
  // (main) thread will do the IO, and when the fetch is complete it will
  // terminate the main thread's message loop; then the other thread's
  // message loop will be shut down automatically as the thread goes out of
  // scope.
  base::Thread t("URLFetcher test thread");
  ASSERT_TRUE(t.Start());
  t.message_loop()->PostTask(FROM_HERE, new FetcherWrapperTask(this,
      test_server.GetURL("defaultresponse")));

  MessageLoop::current()->Run();
}

#if defined(OS_MACOSX)
// SIGSEGV on Mac: http://crbug.com/60426
TEST_F(URLFetcherPostTest, DISABLED_Basic) {
#else
TEST_F(URLFetcherPostTest, Basic) {
#endif
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  CreateFetcher(test_server.GetURL("echo"));
  MessageLoop::current()->Run();
}

TEST_F(URLFetcherHeadersTest, Headers) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP,
      FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(test_server.Start());

  CreateFetcher(test_server.GetURL("files/with-headers.html"));
  MessageLoop::current()->Run();
  // The actual tests are in the URLFetcherHeadersTest fixture.
}

TEST_F(URLFetcherProtectTest, Overload) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("defaultresponse"));

  // Registers an entry for test url. It only allows 3 requests to be sent
  // in 200 milliseconds.
  scoped_refptr<net::URLRequestThrottlerEntry> entry(
      new net::URLRequestThrottlerEntry(200, 3, 1, 0, 2.0, 0.0, 256));
  net::URLRequestThrottlerManager::GetInstance()->OverrideEntryForTests(
      url, entry);

  CreateFetcher(url);

  MessageLoop::current()->Run();

  net::URLRequestThrottlerManager::GetInstance()->EraseEntryForTests(url);
}

TEST_F(URLFetcherProtectTest, ServerUnavailable) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("files/server-unavailable.html"));

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // and maximum backoff time is 256 milliseconds.
  // Maximum retries allowed is set to 11.
  scoped_refptr<net::URLRequestThrottlerEntry> entry(
      new net::URLRequestThrottlerEntry(200, 3, 1, 0, 2.0, 0.0, 256));
  net::URLRequestThrottlerManager::GetInstance()->OverrideEntryForTests(
      url, entry);

  CreateFetcher(url);

  MessageLoop::current()->Run();

  net::URLRequestThrottlerManager::GetInstance()->EraseEntryForTests(url);
}

TEST_F(URLFetcherProtectTestPassedThrough, ServerUnavailablePropagateResponse) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("files/server-unavailable.html"));

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // and maximum backoff time is 150000 milliseconds.
  // Maximum retries allowed is set to 11.
  scoped_refptr<net::URLRequestThrottlerEntry> entry(
      new net::URLRequestThrottlerEntry(200, 3, 100, 0, 2.0, 0.0, 150000));
  // Total time if *not* for not doing automatic backoff would be 150s.
  // In reality it should be "as soon as server responds".
  net::URLRequestThrottlerManager::GetInstance()->OverrideEntryForTests(
      url, entry);

  CreateFetcher(url);

  MessageLoop::current()->Run();

  net::URLRequestThrottlerManager::GetInstance()->EraseEntryForTests(url);
}

#if defined(OS_MACOSX)
// SIGSEGV on Mac: http://crbug.com/60426
TEST_F(URLFetcherBadHTTPSTest, DISABLED_BadHTTPSTest) {
#else
TEST_F(URLFetcherBadHTTPSTest, BadHTTPSTest) {
#endif
  net::TestServer::HTTPSOptions https_options(
      net::TestServer::HTTPSOptions::CERT_EXPIRED);
  net::TestServer test_server(https_options, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  CreateFetcher(test_server.GetURL("defaultresponse"));
  MessageLoop::current()->Run();
}

#if defined(OS_MACOSX)
// SIGSEGV on Mac: http://crbug.com/60426
TEST_F(URLFetcherCancelTest, DISABLED_ReleasesContext) {
#else
TEST_F(URLFetcherCancelTest, ReleasesContext) {
#endif
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("files/server-unavailable.html"));

  // Registers an entry for test url. The backoff time is calculated by:
  //     new_backoff = 2.0 * old_backoff + 0
  // The initial backoff is 2 seconds and maximum backoff is 4 seconds.
  // Maximum retries allowed is set to 2.
  scoped_refptr<net::URLRequestThrottlerEntry> entry(
      new net::URLRequestThrottlerEntry(200, 3, 2000, 0, 2.0, 0.0, 4000));
  net::URLRequestThrottlerManager::GetInstance()->OverrideEntryForTests(
      url, entry);

  // Create a separate thread that will create the URLFetcher.  The current
  // (main) thread will do the IO, and when the fetch is complete it will
  // terminate the main thread's message loop; then the other thread's
  // message loop will be shut down automatically as the thread goes out of
  // scope.
  base::Thread t("URLFetcher test thread");
  ASSERT_TRUE(t.Start());
  t.message_loop()->PostTask(FROM_HERE, new FetcherWrapperTask(this, url));

  MessageLoop::current()->Run();

  net::URLRequestThrottlerManager::GetInstance()->EraseEntryForTests(url);
}

TEST_F(URLFetcherCancelTest, CancelWhileDelayedStartTaskPending) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url(test_server.GetURL("files/server-unavailable.html"));

  // Register an entry for test url.
  // Using a sliding window of 4 seconds, and max of 1 request, under a fast
  // run we expect to have a 4 second delay when posting the Start task.
  scoped_refptr<net::URLRequestThrottlerEntry> entry(
      new net::URLRequestThrottlerEntry(4000, 1, 2000, 0, 2.0, 0.0, 4000));
  net::URLRequestThrottlerManager::GetInstance()->OverrideEntryForTests(
      url, entry);
  // Fake that a request has just started.
  entry->ReserveSendingTimeForNextRequest(base::TimeTicks());

  // The next request we try to send will be delayed by ~4 seconds.
  // The slower the test runs, the less the delay will be (since it takes the
  // time difference from now).

  base::Thread t("URLFetcher test thread");
  ASSERT_TRUE(t.Start());
  t.message_loop()->PostTask(FROM_HERE, new FetcherWrapperTask(this, url));

  MessageLoop::current()->Run();

  net::URLRequestThrottlerManager::GetInstance()->EraseEntryForTests(url);
}

TEST_F(URLFetcherMultipleAttemptTest, SameData) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  // Create the fetcher on the main thread.  Since IO will happen on the main
  // thread, this will test URLFetcher's ability to do everything on one
  // thread.
  CreateFetcher(test_server.GetURL("defaultresponse"));

  MessageLoop::current()->Run();
}

}  // namespace.
