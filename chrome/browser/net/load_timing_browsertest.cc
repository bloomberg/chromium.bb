// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/load_timing_info.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "url/gurl.h"

// This file tests that net::LoadTimingInfo is correctly hooked up to the
// NavigationTiming API.  It depends on behavior in a large number of files
// spread across multiple projects, so is somewhat arbitrarily put in
// chrome/browser/net.

using content::BrowserThread;

namespace {

const char kTestDomain[] = "test.com";
const char kTestUrl[] = "http://test.com/";

// Relative times need to be used because:
// 1)  ExecuteScriptAndExtractInt does not support 64-bit integers.
// 2)  Times for tests are set before the request has been started, but need to
//     be set relative to the start time.
//
// Since some tests need to test negative time deltas (preconnected sockets)
// and others need to test NULL times (events that don't apply), this class has
// to be able to handle all cases:  positive deltas, negative deltas, no
// delta, and null times.
class RelativeTime {
 public:
  // Constructor for null RelativeTimes.
  RelativeTime() : is_null_(true) {
  }

  // Constructor for non-null RelativeTimes.
  explicit RelativeTime(int delta_ms)
      : is_null_(false),
        delta_(base::TimeDelta::FromMilliseconds(delta_ms)) {
  }

  // Given a base time, returns the TimeTicks |this| identifies.
  base::TimeTicks ToTimeTicks(base::TimeTicks base_time) const {
    if (is_null_)
      return base::TimeTicks();
    return base_time + delta_;
  }

  bool is_null() const { return is_null_; }

  base::TimeDelta GetDelta() const {
    // This allows tests to compare times that shouldn't be null without
    // explicitly null-testing them all first.
    EXPECT_FALSE(is_null_);
    return delta_;
  }

 private:
  bool is_null_;

  // Must be 0 when |is_null| is true.
  base::TimeDelta delta_;

  // This class is copyable and assignable.
};

// Structure used for both setting the LoadTimingInfo used by mock requests
// and for times retrieved from the renderer process.
//
// Times used for mock requests are all expressed as TimeDeltas relative to
// when the Job starts.  Null RelativeTimes correspond to null TimeTicks().
//
// Times read from the renderer are expressed relative to fetchStart (Which is
// not the same as request_start).  Null RelativeTimes correspond to times that
// either cannot be retrieved (proxy times, send end) or times that are 0 (SSL
// time when no new SSL connection was established).
struct TimingDeltas {
  RelativeTime proxy_resolve_start;
  RelativeTime proxy_resolve_end;
  RelativeTime dns_start;
  RelativeTime dns_end;
  RelativeTime connect_start;
  RelativeTime ssl_start;
  RelativeTime connect_end;
  RelativeTime send_start;
  RelativeTime send_end;

  // Must be non-negative and greater than all other times.  May only be null if
  // all other times are as well.
  RelativeTime receive_headers_end;
};

// Mock UrlRequestJob that returns the contents of a specified file and
// provides the specified load timing information when queried.
class MockUrlRequestJobWithTiming : public net::URLRequestFileJob {
 public:
  MockUrlRequestJobWithTiming(net::URLRequest* request,
                              net::NetworkDelegate* network_delegate,
                              const base::FilePath& path,
                              const TimingDeltas& load_timing_deltas)
      : net::URLRequestFileJob(
            request, network_delegate, path,
            content::BrowserThread::GetBlockingPool()->
                GetTaskRunnerWithShutdownBehavior(
                    base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
        load_timing_deltas_(load_timing_deltas),
        weak_factory_(this) {}

  // net::URLRequestFileJob implementation:
  virtual void Start() OVERRIDE {
    base::TimeDelta time_to_wait;
    start_time_ = base::TimeTicks::Now();
    if (!load_timing_deltas_.receive_headers_end.is_null()) {
      // Need to delay starting until the largest of the times has elapsed.
      // Wait a little longer than necessary, to be on the safe side.
      time_to_wait = load_timing_deltas_.receive_headers_end.GetDelta() +
                         base::TimeDelta::FromMilliseconds(100);
    }

    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MockUrlRequestJobWithTiming::DelayedStart,
                   weak_factory_.GetWeakPtr()),
        time_to_wait);
  }

  virtual void GetLoadTimingInfo(
      net::LoadTimingInfo* load_timing_info) const OVERRIDE {
    // Make sure enough time has elapsed since start was called.  If this
    // fails, the test fixture itself is flaky.
    if (!load_timing_deltas_.receive_headers_end.is_null()) {
      EXPECT_LE(
          start_time_ + load_timing_deltas_.receive_headers_end.GetDelta(),
          base::TimeTicks::Now());
    }

    // If there are no connect times, but there is a receive headers end time,
    // then assume the socket is reused.  This shouldn't affect the load timing
    // information the test checks, just done for completeness.
    load_timing_info->socket_reused = false;
    if (load_timing_deltas_.connect_start.is_null() &&
        !load_timing_deltas_.receive_headers_end.is_null()) {
      load_timing_info->socket_reused = true;
    }

    load_timing_info->proxy_resolve_start =
        load_timing_deltas_.proxy_resolve_start.ToTimeTicks(start_time_);
    load_timing_info->proxy_resolve_end =
        load_timing_deltas_.proxy_resolve_end.ToTimeTicks(start_time_);

    load_timing_info->connect_timing.dns_start =
        load_timing_deltas_.dns_start.ToTimeTicks(start_time_);
    load_timing_info->connect_timing.dns_end =
        load_timing_deltas_.dns_end.ToTimeTicks(start_time_);
    load_timing_info->connect_timing.connect_start =
        load_timing_deltas_.connect_start.ToTimeTicks(start_time_);
    load_timing_info->connect_timing.ssl_start =
        load_timing_deltas_.ssl_start.ToTimeTicks(start_time_);
    load_timing_info->connect_timing.connect_end =
        load_timing_deltas_.connect_end.ToTimeTicks(start_time_);

    // If there's an SSL start time, use connect end as the SSL end time.
    // The NavigationTiming API does not have a corresponding field, and there's
    // no need to test the case when the values are both non-NULL and different.
    if (!load_timing_deltas_.ssl_start.is_null()) {
      load_timing_info->connect_timing.ssl_end =
          load_timing_info->connect_timing.connect_end;
    }

    load_timing_info->send_start =
        load_timing_deltas_.send_start.ToTimeTicks(start_time_);
    load_timing_info->send_end=
        load_timing_deltas_.send_end.ToTimeTicks(start_time_);
    load_timing_info->receive_headers_end =
        load_timing_deltas_.receive_headers_end.ToTimeTicks(start_time_);
  }

 private:
  // Parent class is reference counted, so need to have a private destructor.
  virtual ~MockUrlRequestJobWithTiming() {}

  void DelayedStart() {
    net::URLRequestFileJob::Start();
  }

  // Load times to use, relative to |start_time_|.
  const TimingDeltas load_timing_deltas_;
  base::TimeTicks start_time_;

  base::WeakPtrFactory<MockUrlRequestJobWithTiming> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockUrlRequestJobWithTiming);
};

// AURLRequestInterceptor that returns mock URLRequestJobs that return the
// specified file with the given timings.  Constructed on the UI thread, but
// after that, lives and is destroyed on the IO thread.
class TestInterceptor : public net::URLRequestInterceptor {
 public:
  TestInterceptor(const base::FilePath& path,
                  const TimingDeltas& load_timing_deltas)
      : path_(path), load_timing_deltas_(load_timing_deltas) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  virtual ~TestInterceptor() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
  }

  // Registers |this| with the URLRequestFilter, which takes ownership of it.
  void Register() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", kTestDomain, scoped_ptr<net::URLRequestInterceptor>(this));
  }

  // Unregisters |this| with the URLRequestFilter, which should then delete
  // |this|.
  void Unregister() {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(
        "http", kTestDomain);
  }

  // net::URLRequestJobFactory::ProtocolHandler implementation:
  virtual net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));

    return new MockUrlRequestJobWithTiming(request, network_delegate, path_,
                                           load_timing_deltas_);
  }

 private:
  // Path of the file to use as the response body.
  const base::FilePath path_;

  // Load times for each request to use, relative to when the Job starts.
  const TimingDeltas load_timing_deltas_;

  DISALLOW_COPY_AND_ASSIGN(TestInterceptor);
};

class LoadTimingBrowserTest : public InProcessBrowserTest {
 public:
  LoadTimingBrowserTest() {
  }

  virtual ~LoadTimingBrowserTest() {
  }

  // Navigates to |url| and writes the resulting navigation timings to
  // |navigation_deltas|.
  void RunTestWithUrl(const GURL& url, TimingDeltas* navigation_deltas) {
    ui_test_utils::NavigateToURL(browser(), url);
    GetResultDeltas(navigation_deltas);
  }

  // Navigates to a url that returns the timings indicated by
  // |load_timing_deltas| and writes the resulting navigation timings to
  // |navigation_deltas|.  Uses a generic test page.
  void RunTest(const TimingDeltas& load_timing_deltas,
               TimingDeltas* navigation_deltas) {
    // None of the tests care about the contents of the test page.  Just do
    // this here because PathService has thread restrictions on some platforms.
    base::FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("title1.html");

    // Create and register request interceptor.
    TestInterceptor* test_interceptor =
        new TestInterceptor(path, load_timing_deltas);
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&TestInterceptor::Register,
                                       base::Unretained(test_interceptor)));

    // Navigate to the page.
    RunTestWithUrl(GURL(kTestUrl), navigation_deltas);

    // Once navigation is complete, unregister the protocol handler.
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&TestInterceptor::Unregister,
                                        base::Unretained(test_interceptor)));
  }

 private:
  // Reads applicable times from performance.timing and writes them to
  // |navigation_deltas|.  Proxy times and send end cannot be read from the
  // Navigation Timing API, so those are all left as null.
  void GetResultDeltas(TimingDeltas* navigation_deltas) {
    *navigation_deltas = TimingDeltas();

    navigation_deltas->dns_start = GetResultDelta("domainLookupStart");
    navigation_deltas->dns_end = GetResultDelta("domainLookupEnd");
    navigation_deltas->connect_start = GetResultDelta("connectStart");
    navigation_deltas->connect_end = GetResultDelta("connectEnd");
    navigation_deltas->send_start = GetResultDelta("requestStart");
    navigation_deltas->receive_headers_end = GetResultDelta("responseStart");

    // Unlike the above times, secureConnectionStart will be zero when not
    // applicable.  In that case, leave ssl_start as null.
    bool ssl_start_zero = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
                    browser()->tab_strip_model()->GetActiveWebContents(),
                    "window.domAutomationController.send("
                        "performance.timing.secureConnectionStart == 0);",
                    &ssl_start_zero));
    if (!ssl_start_zero)
      navigation_deltas->ssl_start = GetResultDelta("secureConnectionStart");

    // Simple sanity checks.  Make sure times that correspond to LoadTimingInfo
    // occur between fetchStart and loadEventEnd.  Relationships between
    // intervening times are handled by the test bodies.

    RelativeTime fetch_start = GetResultDelta("fetchStart");
    // While the input dns_start is sometimes null, when read from the
    // NavigationTiming API, it's always non-null.
    EXPECT_LE(fetch_start.GetDelta(), navigation_deltas->dns_start.GetDelta());

    RelativeTime load_event_end = GetResultDelta("loadEventEnd");
    EXPECT_LE(navigation_deltas->receive_headers_end.GetDelta(),
              load_event_end.GetDelta());
  }

  // Returns the time between performance.timing.fetchStart and the time with
  // the specified name.  This time must be non-negative.
  RelativeTime GetResultDelta(const std::string& name) {
    int time_ms = 0;
    std::string command(base::StringPrintf(
        "window.domAutomationController.send("
            "performance.timing.%s - performance.timing.fetchStart);",
        name.c_str()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
                    browser()->tab_strip_model()->GetActiveWebContents(),
                    command.c_str(),
                    &time_ms));

    // Basic sanity check.
    EXPECT_GE(time_ms, 0);

    return RelativeTime(time_ms);
  }
};

// Test case when no times are given, except the request start times.  This
// happens with FTP, cached responses, responses handled by something other than
// the network stack, RedirectJobs, HSTs, etc.
IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, NoTimes) {
  TimingDeltas load_timing_deltas;
  TimingDeltas navigation_deltas;
  RunTest(load_timing_deltas, &navigation_deltas);

  // When there are no times, all read times should be the same as fetchStart,
  // except SSL start, which should be 0.
  EXPECT_EQ(base::TimeDelta(), navigation_deltas.dns_start.GetDelta());
  EXPECT_EQ(base::TimeDelta(), navigation_deltas.dns_end.GetDelta());
  EXPECT_EQ(base::TimeDelta(), navigation_deltas.connect_start.GetDelta());
  EXPECT_EQ(base::TimeDelta(), navigation_deltas.connect_end.GetDelta());
  EXPECT_EQ(base::TimeDelta(), navigation_deltas.send_start.GetDelta());
  EXPECT_EQ(base::TimeDelta(),
            navigation_deltas.receive_headers_end.GetDelta());

  EXPECT_TRUE(navigation_deltas.ssl_start.is_null());
}

// Standard case - new socket, no PAC, no preconnect, no SSL.
IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, Basic) {
  TimingDeltas load_timing_deltas;
  load_timing_deltas.dns_start = RelativeTime(0);
  load_timing_deltas.dns_end = RelativeTime(100);
  load_timing_deltas.connect_start = RelativeTime(200);
  load_timing_deltas.connect_end = RelativeTime(300);
  load_timing_deltas.send_start = RelativeTime(400);
  load_timing_deltas.send_end = RelativeTime(500);
  load_timing_deltas.receive_headers_end = RelativeTime(600);

  TimingDeltas navigation_deltas;
  RunTest(load_timing_deltas, &navigation_deltas);

  // Due to potential roundoff issues, never check exact differences.
  EXPECT_LT(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.dns_end.GetDelta());
  EXPECT_LT(navigation_deltas.dns_end.GetDelta(),
            navigation_deltas.connect_start.GetDelta());
  EXPECT_LT(navigation_deltas.connect_start.GetDelta(),
            navigation_deltas.connect_end.GetDelta());
  EXPECT_LT(navigation_deltas.connect_end.GetDelta(),
            navigation_deltas.send_start.GetDelta());
  EXPECT_LT(navigation_deltas.send_start.GetDelta(),
            navigation_deltas.receive_headers_end.GetDelta());

  EXPECT_TRUE(navigation_deltas.ssl_start.is_null());
}

// Basic SSL case.
IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, Ssl) {
  TimingDeltas load_timing_deltas;
  load_timing_deltas.dns_start = RelativeTime(0);
  load_timing_deltas.dns_end = RelativeTime(100);
  load_timing_deltas.connect_start = RelativeTime(200);
  load_timing_deltas.ssl_start = RelativeTime(300);
  load_timing_deltas.connect_end = RelativeTime(400);
  load_timing_deltas.send_start = RelativeTime(500);
  load_timing_deltas.send_end = RelativeTime(600);
  load_timing_deltas.receive_headers_end = RelativeTime(700);

  TimingDeltas navigation_deltas;
  RunTest(load_timing_deltas, &navigation_deltas);

  // Due to potential roundoff issues, never check exact differences.
  EXPECT_LT(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.dns_end.GetDelta());
  EXPECT_LT(navigation_deltas.dns_end.GetDelta(),
            navigation_deltas.connect_start.GetDelta());
  EXPECT_LT(navigation_deltas.connect_start.GetDelta(),
            navigation_deltas.ssl_start.GetDelta());
  EXPECT_LT(navigation_deltas.ssl_start.GetDelta(),
            navigation_deltas.connect_end.GetDelta());
  EXPECT_LT(navigation_deltas.connect_end.GetDelta(),
            navigation_deltas.send_start.GetDelta());
  EXPECT_LT(navigation_deltas.send_start.GetDelta(),
            navigation_deltas.receive_headers_end.GetDelta());
}

// All times are the same.
IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, EverythingAtOnce) {
  TimingDeltas load_timing_deltas;
  load_timing_deltas.dns_start = RelativeTime(100);
  load_timing_deltas.dns_end = RelativeTime(100);
  load_timing_deltas.connect_start = RelativeTime(100);
  load_timing_deltas.ssl_start = RelativeTime(100);
  load_timing_deltas.connect_end = RelativeTime(100);
  load_timing_deltas.send_start = RelativeTime(100);
  load_timing_deltas.send_end = RelativeTime(100);
  load_timing_deltas.receive_headers_end = RelativeTime(100);

  TimingDeltas navigation_deltas;
  RunTest(load_timing_deltas, &navigation_deltas);

  EXPECT_EQ(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.dns_end.GetDelta());
  EXPECT_EQ(navigation_deltas.dns_end.GetDelta(),
            navigation_deltas.connect_start.GetDelta());
  EXPECT_EQ(navigation_deltas.connect_start.GetDelta(),
            navigation_deltas.ssl_start.GetDelta());
  EXPECT_EQ(navigation_deltas.ssl_start.GetDelta(),
            navigation_deltas.connect_end.GetDelta());
  EXPECT_EQ(navigation_deltas.connect_end.GetDelta(),
            navigation_deltas.send_start.GetDelta());
  EXPECT_EQ(navigation_deltas.send_start.GetDelta(),
            navigation_deltas.receive_headers_end.GetDelta());
}

// Reuse case.
IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, ReuseSocket) {
  TimingDeltas load_timing_deltas;
  load_timing_deltas.send_start = RelativeTime(0);
  load_timing_deltas.send_end = RelativeTime(100);
  load_timing_deltas.receive_headers_end = RelativeTime(200);

  TimingDeltas navigation_deltas;
  RunTest(load_timing_deltas, &navigation_deltas);

  // Connect times should all be the same as fetchStart.
  EXPECT_EQ(base::TimeDelta(), navigation_deltas.dns_start.GetDelta());
  EXPECT_EQ(base::TimeDelta(), navigation_deltas.dns_end.GetDelta());
  EXPECT_EQ(base::TimeDelta(), navigation_deltas.connect_start.GetDelta());
  EXPECT_EQ(base::TimeDelta(), navigation_deltas.connect_end.GetDelta());

  // Connect end may be less than send start, since connect end defaults to
  // fetchStart, which is often less than request_start.
  EXPECT_LE(navigation_deltas.connect_end.GetDelta(),
            navigation_deltas.send_start.GetDelta());

  EXPECT_LT(navigation_deltas.send_start.GetDelta(),
            navigation_deltas.receive_headers_end.GetDelta());

  EXPECT_TRUE(navigation_deltas.ssl_start.is_null());
}

// Preconnect case.  Connect times are all before the request was started.
IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, Preconnect) {
  TimingDeltas load_timing_deltas;
  load_timing_deltas.dns_start = RelativeTime(-1000300);
  load_timing_deltas.dns_end = RelativeTime(-1000200);
  load_timing_deltas.connect_start = RelativeTime(-1000100);
  load_timing_deltas.connect_end = RelativeTime(-1000000);
  load_timing_deltas.send_start = RelativeTime(0);
  load_timing_deltas.send_end = RelativeTime(100);
  load_timing_deltas.receive_headers_end = RelativeTime(200);

  TimingDeltas navigation_deltas;
  RunTest(load_timing_deltas, &navigation_deltas);

  // Connect times should all be the same as request_start.
  EXPECT_EQ(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.dns_end.GetDelta());
  EXPECT_EQ(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.connect_start.GetDelta());
  EXPECT_EQ(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.connect_end.GetDelta());

  EXPECT_LE(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.send_start.GetDelta());

  EXPECT_LT(navigation_deltas.send_start.GetDelta(),
            navigation_deltas.receive_headers_end.GetDelta());
  EXPECT_LT(navigation_deltas.send_start.GetDelta(),
            navigation_deltas.receive_headers_end.GetDelta());

  EXPECT_TRUE(navigation_deltas.ssl_start.is_null());
}

// Preconnect case with a proxy.  Connect times are all before the proxy lookup
// finished (Or at the same time).
IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, PreconnectProxySsl) {
  TimingDeltas load_timing_deltas;
  load_timing_deltas.proxy_resolve_start = RelativeTime(0);
  load_timing_deltas.proxy_resolve_end = RelativeTime(100);
  load_timing_deltas.dns_start = RelativeTime(-3000000);
  load_timing_deltas.dns_end = RelativeTime(-2000000);
  load_timing_deltas.connect_start = RelativeTime(-1000000);
  load_timing_deltas.ssl_start = RelativeTime(0);
  load_timing_deltas.connect_end = RelativeTime(100);
  load_timing_deltas.send_start = RelativeTime(100);
  load_timing_deltas.send_end = RelativeTime(200);
  load_timing_deltas.receive_headers_end = RelativeTime(300);

  TimingDeltas navigation_deltas;
  RunTest(load_timing_deltas, &navigation_deltas);

  // Connect times should all be the same as proxy_end, which is also the
  // same as send_start.
  EXPECT_EQ(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.dns_end.GetDelta());
  EXPECT_EQ(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.connect_start.GetDelta());
  EXPECT_EQ(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.ssl_start.GetDelta());
  EXPECT_EQ(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.connect_end.GetDelta());
  EXPECT_EQ(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.send_start.GetDelta());

  EXPECT_LT(navigation_deltas.send_start.GetDelta(),
            navigation_deltas.receive_headers_end.GetDelta());
  EXPECT_LT(navigation_deltas.send_start.GetDelta(),
            navigation_deltas.receive_headers_end.GetDelta());
}

// Integration test with a real network response.
IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, Integration) {
  ASSERT_TRUE(test_server()->Start());
  TimingDeltas navigation_deltas;
  RunTestWithUrl(test_server()->GetURL("chunked?waitBeforeHeaders=100"),
                 &navigation_deltas);

  // Due to potential roundoff issues, never check exact differences.
  EXPECT_LE(navigation_deltas.dns_start.GetDelta(),
            navigation_deltas.dns_end.GetDelta());
  EXPECT_LE(navigation_deltas.dns_end.GetDelta(),
            navigation_deltas.connect_start.GetDelta());
  EXPECT_LE(navigation_deltas.connect_start.GetDelta(),
            navigation_deltas.connect_end.GetDelta());
  EXPECT_LE(navigation_deltas.connect_end.GetDelta(),
            navigation_deltas.send_start.GetDelta());
  // The only times that are guaranteed to be distinct are send_start and
  // received_headers_end.
  EXPECT_LT(navigation_deltas.send_start.GetDelta(),
            navigation_deltas.receive_headers_end.GetDelta());

  EXPECT_TRUE(navigation_deltas.ssl_start.is_null());
}

}  // namespace
