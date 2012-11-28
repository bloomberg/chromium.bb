// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/browser/net/double_get_experiment_interceptor.h"
#include "content/public/test/mock_resource_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

const char kTestData[] = "some data";

const char kNonCacheableResponseHeader[] =
    "HTTP/1.1 200 OK\0"
    "Content-type: "
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document\0"
    "Cache-control: no-cache\0"
    "\0";

const char kCacheableResponseHeader[] =
    "HTTP/1.1 200 OK\0"
    "Content-type: "
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document\0"
    "date: Wed, 28 Nov 2011 00:40:11 GMT\0"
    "expires: Wed, 28 Nov 2011 01:00:00 GMT\0"
    "\0";

const char kHtmlResponseHeader[] =
    "HTTP/1.1 200 OK\0"
    "Content-type: text/html\0"
    "\0";

const char kRedirectResponseHeader[] =
    "HTTP/1.1 302 MOVED\0"
    "Location: http://somewhere.foo\0"
    "\0";

const char kErrorResponseHeader[] =
    "HTTP/1.1 500 GAME OVER\0"
    "\0";

class NonCacheableTestJob : public net::URLRequestTestJob {
 public:
  NonCacheableTestJob(net::URLRequest* request,
                     net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(
            request,
            network_delegate,
            std::string(kNonCacheableResponseHeader,
                        arraysize(kNonCacheableResponseHeader)),
                        std::string(kTestData),
                        true) {
  }

 private:
  virtual ~NonCacheableTestJob() {}
};

class CacheableTestJob : public net::URLRequestTestJob {
 public:
  CacheableTestJob(net::URLRequest* request,
                  net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(
            request,
            network_delegate,
            std::string(kCacheableResponseHeader,
                        arraysize(kCacheableResponseHeader)),
                        std::string(kTestData),
                        true) {
  }

 private:
  virtual ~CacheableTestJob() {}
};

class HtmlTestJob : public net::URLRequestTestJob {
 public:
  HtmlTestJob(net::URLRequest* request, net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               std::string(kHtmlResponseHeader,
                                           arraysize(kHtmlResponseHeader)),
                               std::string(kTestData),
                               true) {
  }

 private:
  virtual ~HtmlTestJob() {}
};

class RedirectTestJob : public net::URLRequestTestJob {
 public:
  RedirectTestJob(net::URLRequest* request,
                  net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               std::string(kRedirectResponseHeader,
                                           arraysize(kRedirectResponseHeader)),
                               std::string(),
                               true) {
  }

 private:
  virtual ~RedirectTestJob() {}
};

class ErrorTestJob : public net::URLRequestTestJob {
 public:
  ErrorTestJob(net::URLRequest* request, net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               std::string(kErrorResponseHeader,
                                           arraysize(kErrorResponseHeader)),
                               std::string(),
                               true) {
  }

 private:
  virtual ~ErrorTestJob() {}
};

class CancelTestJob : public net::URLRequestTestJob {
 public:
  explicit CancelTestJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate)
    : net::URLRequestTestJob(request, network_delegate, true) {}
 protected:
  virtual void StartAsync() {
    request_->Cancel();
  }

 private:
  ~CancelTestJob() {}
};

enum TestJobs {
  TEST_JOB_NON_CACHEABLE,
  TEST_JOB_CACHEABLE,
  TEST_JOB_HTML,
  TEST_JOB_REDIRECT,
  TEST_JOB_FAIL,
  TEST_JOB_CANCEL
};

// Intercepts requests started in the test and replaces them with test request
// jobs determined in interceptions_ list. The test jobs are triggered in order
// they appear in the interceptions list.
// It is an error if not all test jobs are posted.
// The number of requests encountered during the test must be equal to the
// number of test jobs in interceptions list.
class TestInterceptor : public net::URLRequestJobFactory::Interceptor {
 public:
  explicit TestInterceptor(const std::vector<TestJobs>& interceptions)
      : interceptions_(interceptions),
        next_interception_(0) {
  }

  virtual ~TestInterceptor() {
    EXPECT_EQ(interceptions_.size(), next_interception_);
  }

  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    // Verify that not all test jobs have been used.
    EXPECT_LT(next_interception_, interceptions_.size());
    if (next_interception_ >= interceptions_.size())
      return NULL;

    // Select the next test job.
    TestJobs next_job = interceptions_[next_interception_];
    ++next_interception_;

    // Intercept the request with the next test job.
    switch (next_job) {
      case TEST_JOB_NON_CACHEABLE:
        return new NonCacheableTestJob(request, network_delegate);
      case TEST_JOB_CACHEABLE:
        return new CacheableTestJob(request, network_delegate);
      case TEST_JOB_HTML:
        return new HtmlTestJob(request, network_delegate);
      case TEST_JOB_REDIRECT:
        return new RedirectTestJob(request, network_delegate);
      case TEST_JOB_FAIL:
        return new ErrorTestJob(request, network_delegate);
      case TEST_JOB_CANCEL:
        return new CancelTestJob(request, network_delegate);
      default:
        EXPECT_FALSE(true) << "SHOULD NOT BE REACHED";
        return NULL;
    }
  }

  // NO_OP.
  virtual net::URLRequestJob* MaybeInterceptRedirect(
      const GURL& url,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return NULL;
  }

  // No-op.
  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    return NULL;
  }

 private:
  // List of test jobs.
  std::vector<TestJobs> interceptions_;
  mutable size_t next_interception_;
};

class MockMetricsRecorder
    : public DoubleGetExperimentInterceptor::MetricsRecorder {
 public:
  ~MockMetricsRecorder() {}
  MOCK_CONST_METHOD0(RecordPost, void());
  MOCK_CONST_METHOD1(RecordGet, void(bool cacheable));
  MOCK_CONST_METHOD1(RecordRepeatedGet, void(int response_code));
};

class DoubleGetExperimentTest : public testing::Test {
 public:
  DoubleGetExperimentTest() : test_interceptor_(NULL),
                              default_context_(false) {
  }

  virtual void SetUp() {
    default_context_.set_job_factory(&job_factory_);
  }

 protected:
  void InitTestInterceptor(const std::vector<TestJobs>& interceptions) {
    ASSERT_FALSE(test_interceptor_);
    test_interceptor_ = new TestInterceptor(interceptions);
    job_factory_.AddInterceptor(test_interceptor_);
  }

  MessageLoopForIO message_loop_;
  net::TestNetworkDelegate test_network_delegate_;
  TestInterceptor* test_interceptor_;
  net::TestURLRequestContext default_context_;
  net::URLRequestJobFactoryImpl job_factory_;
};

TEST_F(DoubleGetExperimentTest, Success) {
  // Initialize the test interceptor.
  std::vector<TestJobs> interceptions;
  interceptions.push_back(TEST_JOB_NON_CACHEABLE);  // Initial GET succeeds.
  interceptions.push_back(TEST_JOB_NON_CACHEABLE);  // Repeated GET succeeds.
  InitTestInterceptor(interceptions);

  // Create the interceptor to be tested.
  // |job_factory_| takes the ownership of |interceptor| which takes the
  // ownership of |mock_metrice_recorder|.
  MockMetricsRecorder* mock_metrics_recorder = new MockMetricsRecorder();
  DoubleGetExperimentInterceptor* interceptor =
      new DoubleGetExperimentInterceptor(mock_metrics_recorder);
  job_factory_.AddInterceptor(interceptor);

  // Set expectations for mock_metrics_recorder.
  EXPECT_CALL(*mock_metrics_recorder, RecordPost()).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordGet(false)).Times(1);
  EXPECT_CALL(*mock_metrics_recorder, RecordRepeatedGet(200)).Times(1);

  // Create and initialize test request.
  net::TestDelegate test_delegate;
  net::URLRequest request(GURL("http://test_interceptor/foo"), &test_delegate,
                               &default_context_);
  request.set_referrer("http://the-referrer.foo");

  // Remember the request parameters so it can be verified the request hasn't
  // unexpectedly changed during the test.
  const std::string initial_referrer = request.referrer();
  int initial_load_flags = request.load_flags();
  GURL initial_original_url = request.original_url();

  // Kick off the request and wait for it to finish.
  request.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(200, request.GetResponseCode());

  // There should be two identical entries in the URL chain.
  ASSERT_EQ(2u, request.url_chain().size());
  EXPECT_EQ(request.url_chain()[0], request.url_chain()[1]);

  // The requests parameters should not have changed due to redirect.
  EXPECT_EQ(initial_referrer, request.referrer());
  EXPECT_EQ(initial_original_url, request.original_url());
  EXPECT_EQ(request.original_url(), request.url());
  EXPECT_EQ(initial_load_flags, request.load_flags());
}

TEST_F(DoubleGetExperimentTest, RepeatedGetFails) {
  // Initialize the test interceptor.
  std::vector<TestJobs> interceptions;
  interceptions.push_back(TEST_JOB_NON_CACHEABLE);  // Initial GET succeeds.
  interceptions.push_back(TEST_JOB_FAIL);  // Repeated GET fails.
  InitTestInterceptor(interceptions);

  // Create the interceptor to be tested.
  // |job_factory_| takes the ownership of |interceptor| which takes the
  // ownership of |mock_metrice_recorder|.
  MockMetricsRecorder* mock_metrics_recorder = new MockMetricsRecorder();
  DoubleGetExperimentInterceptor* interceptor =
      new DoubleGetExperimentInterceptor(mock_metrics_recorder);
  job_factory_.AddInterceptor(interceptor);

  // Set expectations for mock_metrics_recorder.
  EXPECT_CALL(*mock_metrics_recorder, RecordPost()).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordGet(false)).Times(1);
  EXPECT_CALL(*mock_metrics_recorder, RecordRepeatedGet(500)).Times(1);

  // Create and initialize test request.
  net::TestDelegate test_delegate;
  net::URLRequest request(GURL("http://test_interceptor/foo"), &test_delegate,
                               &default_context_);

  // Kick off the request and wait for it to finish.
  request.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(500, request.GetResponseCode());
}

TEST_F(DoubleGetExperimentTest, DontInterceptPost) {
  // Initialize the test interceptor.
  std::vector<TestJobs> interceptions;
  interceptions.push_back(TEST_JOB_NON_CACHEABLE);  // Not really important, as
                                                   // long as the request is
                                                   // POST.
  InitTestInterceptor(interceptions);

  // Create the interceptor to be tested.
  // |job_factory_| takes the ownership of |interceptor| which takes the
  // ownership of |mock_metrice_recorder|.
  MockMetricsRecorder* mock_metrics_recorder = new MockMetricsRecorder();
  DoubleGetExperimentInterceptor* interceptor =
      new DoubleGetExperimentInterceptor(mock_metrics_recorder);
  job_factory_.AddInterceptor(interceptor);

  // Set expectations for mock_metrics_recorder.
  EXPECT_CALL(*mock_metrics_recorder, RecordPost()).Times(1);
  EXPECT_CALL(*mock_metrics_recorder, RecordGet(_)).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordRepeatedGet(_)).Times(0);

  // Create and initialize test request.
  net::TestDelegate test_delegate;
  net::URLRequest request(GURL("http://test_interceptor/foo"), &test_delegate,
                               &default_context_);
  request.set_method("POST");

  // Kick off the request and wait for it to finish.
  request.Start();
  MessageLoop::current()->Run();

  // The request response shouldn't have been intercepted.
  EXPECT_EQ(1u, request.url_chain().size());
}

TEST_F(DoubleGetExperimentTest, DontInterceptCacheable) {
  // Initialize the test interceptor.
  std::vector<TestJobs> interceptions;
  interceptions.push_back(TEST_JOB_CACHEABLE);
  InitTestInterceptor(interceptions);

  // Create the interceptor to be tested.
  // |job_factory_| takes the ownership of |interceptor| which takes the
  // ownership of |mock_metrice_recorder|.
  MockMetricsRecorder* mock_metrics_recorder = new MockMetricsRecorder();
  DoubleGetExperimentInterceptor* interceptor =
      new DoubleGetExperimentInterceptor(mock_metrics_recorder);
  job_factory_.AddInterceptor(interceptor);

  // Set expectations for mock_metrics_recorder.
  EXPECT_CALL(*mock_metrics_recorder, RecordPost()).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordGet(true)).Times(1);
  EXPECT_CALL(*mock_metrics_recorder, RecordRepeatedGet(_)).Times(0);

  // Create and initialize test request.
  net::TestDelegate test_delegate;
  net::URLRequest request(GURL("http://test_interceptor/foo"), &test_delegate,
                               &default_context_);

  // Kick off the request and wait for it to finish.
  request.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(200, request.GetResponseCode());

  // The request response should not have been intercepted.
  EXPECT_EQ(1u, request.url_chain().size());
}


TEST_F(DoubleGetExperimentTest, IgnoreInvalidMethod) {
  // Initialize the test interceptor.
  std::vector<TestJobs> interceptions;
  interceptions.push_back(TEST_JOB_NON_CACHEABLE);
  InitTestInterceptor(interceptions);

  // Create the interceptor to be tested.
  // |job_factory_| takes the ownership of |interceptor| which takes the
  // ownership of |mock_metrice_recorder|.
  MockMetricsRecorder* mock_metrics_recorder = new MockMetricsRecorder();
  DoubleGetExperimentInterceptor* interceptor =
      new DoubleGetExperimentInterceptor(mock_metrics_recorder);
  job_factory_.AddInterceptor(interceptor);

  // Set expectations for mock_metrics_recorder.
  EXPECT_CALL(*mock_metrics_recorder, RecordPost()).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordGet(_)).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordRepeatedGet(_)).Times(0);

  // Create and initialize test request.
  net::TestDelegate test_delegate;
  net::URLRequest request(GURL("http://test_interceptor/foo"), &test_delegate,
                               &default_context_);
  request.set_method("FOO");

  // Kick off the request and wait for it to finish.
  request.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(200, request.GetResponseCode());

  // The request response should not have been intercepted.
  EXPECT_EQ(1u, request.url_chain().size());
}

TEST_F(DoubleGetExperimentTest, IgnoreUninterestingMimeType) {
  // Initialize the test interceptor.
  std::vector<TestJobs> interceptions;
  interceptions.push_back(TEST_JOB_HTML);  // Non office file should be ignored.
  InitTestInterceptor(interceptions);

  // Create the interceptor to be tested.
  // |job_factory_| takes the ownership of |interceptor| which takes the
  // ownership of |mock_metrice_recorder|.
  MockMetricsRecorder* mock_metrics_recorder = new MockMetricsRecorder();
  DoubleGetExperimentInterceptor* interceptor =
      new DoubleGetExperimentInterceptor(mock_metrics_recorder);
  job_factory_.AddInterceptor(interceptor);

  // Create and initialize test request.
  net::TestDelegate test_delegate;
  net::URLRequest request(GURL("http://test_interceptor/foo"), &test_delegate,
                               &default_context_);

  // Set expectations for mock_metrics_recorder.
  EXPECT_CALL(*mock_metrics_recorder, RecordPost()).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordGet(_)).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordRepeatedGet(_)).Times(0);

  // Kick off the request and wait for it to finish.
  request.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(200, request.GetResponseCode());

  // The request response should not have been intercepted.
  EXPECT_EQ(1u, request.url_chain().size());
}

TEST_F(DoubleGetExperimentTest, IgnoreFileScheme) {
  // Initialize the test interceptor.
  std::vector<TestJobs> interceptions;
  interceptions.push_back(TEST_JOB_NON_CACHEABLE);
  InitTestInterceptor(interceptions);

  // Create the interceptor to be tested.
  // |job_factory_| takes the ownership of |interceptor| which takes the
  // ownership of |mock_metrice_recorder|.
  MockMetricsRecorder* mock_metrics_recorder = new MockMetricsRecorder();
  DoubleGetExperimentInterceptor* interceptor =
      new DoubleGetExperimentInterceptor(mock_metrics_recorder);
  job_factory_.AddInterceptor(interceptor);

  // Create and initialize test request.
  net::TestDelegate test_delegate;
  net::URLRequest request(GURL("file://test_interceptor/foo"), &test_delegate,
                               &default_context_);

  // Set expectations for mock_metrics_recorder.
  // Schemes different from http and https should be ignored.
  EXPECT_CALL(*mock_metrics_recorder, RecordPost()).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordGet(_)).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordRepeatedGet(_)).Times(0);

  // Kick off the request and wait for it to finish.
  request.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(200, request.GetResponseCode());

  // The request response should not have been intercepted.
  EXPECT_EQ(1u, request.url_chain().size());
}

TEST_F(DoubleGetExperimentTest, RepeatedRequestCanceled) {
  // Initialize the test interceptor.
  std::vector<TestJobs> interceptions;
  interceptions.push_back(TEST_JOB_NON_CACHEABLE);
  interceptions.push_back(TEST_JOB_CANCEL);  // The request is canceled before
                                             // the repeated GET responds.
  InitTestInterceptor(interceptions);

  // Create the interceptor to be tested.
  // |job_factory_| takes the ownership of |interceptor| which takes the
  // ownership of |mock_metrice_recorder|.
  MockMetricsRecorder* mock_metrics_recorder = new MockMetricsRecorder();
  DoubleGetExperimentInterceptor* interceptor =
      new DoubleGetExperimentInterceptor(mock_metrics_recorder);
  job_factory_.AddInterceptor(interceptor);

  // Set expectations for mock_metrics_recorder.
  EXPECT_CALL(*mock_metrics_recorder, RecordPost()).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordGet(false)).Times(1);
  EXPECT_CALL(*mock_metrics_recorder, RecordRepeatedGet(_)).Times(0);

  // Create and initialize test request.
  net::TestDelegate test_delegate;
  net::URLRequest request(GURL("http://test_interceptor/foo"), &test_delegate,
                               &default_context_);

  // Kick off the request and wait for it to finish.
  request.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(-1, request.GetResponseCode());
}

TEST_F(DoubleGetExperimentTest, Redirection) {
  // Initialize the test interceptor.
  std::vector<TestJobs> interceptions;
  interceptions.push_back(TEST_JOB_REDIRECT);  // First request is redirected.
  interceptions.push_back(TEST_JOB_NON_CACHEABLE);
  interceptions.push_back(TEST_JOB_NON_CACHEABLE);
  InitTestInterceptor(interceptions);

  // Create the interceptor to be tested.
  // |job_factory_| takes the ownership of |interceptor| which takes the
  // ownership of |mock_metrice_recorder|.
  MockMetricsRecorder* mock_metrics_recorder = new MockMetricsRecorder();
  DoubleGetExperimentInterceptor* interceptor =
      new DoubleGetExperimentInterceptor(mock_metrics_recorder);
  job_factory_.AddInterceptor(interceptor);

  // Set expectations for mock_metrics_recorder.
  EXPECT_CALL(*mock_metrics_recorder, RecordPost()).Times(0);
  EXPECT_CALL(*mock_metrics_recorder, RecordGet(false)).Times(1);
  EXPECT_CALL(*mock_metrics_recorder, RecordRepeatedGet(200)).Times(1);

  // Create and initialize test request.
  net::TestDelegate test_delegate;
  net::URLRequest request(GURL("http://test_interceptor/foo"), &test_delegate,
                               &default_context_);
  request.set_referrer("http://the-referrer.foo");
  std::string initial_referrer = request.referrer();

  // Kick off the request and wait for it to finish.
  request.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(200, request.GetResponseCode());

  // Verify the URL chain is as expected.
  // The second request should have been repeated.
  EXPECT_EQ(3u, request.url_chain().size());
  EXPECT_EQ(GURL("http://test_interceptor/foo"), request.url_chain()[0]);
  EXPECT_EQ(GURL("http://somewhere.foo"), request.url_chain()[1]);
  EXPECT_EQ(GURL("http://somewhere.foo"), request.url_chain()[2]);

  EXPECT_EQ(initial_referrer, request.referrer());
}

}  // namespace
