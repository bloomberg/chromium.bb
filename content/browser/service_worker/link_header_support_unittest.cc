// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/link_header_support.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

TEST(LinkHeaderTest, SplitEmpty) {
  std::vector<std::string> values;
  SplitLinkHeaderForTesting("", &values);
  ASSERT_EQ(0u, values.size());
}

TEST(LinkHeaderTest, SplitSimple) {
  std::vector<std::string> values;
  SplitLinkHeaderForTesting("hello", &values);
  ASSERT_EQ(1u, values.size());
  EXPECT_EQ("hello", values[0]);

  SplitLinkHeaderForTesting("foo, bar", &values);
  ASSERT_EQ(2u, values.size());
  EXPECT_EQ("foo", values[0]);
  EXPECT_EQ("bar", values[1]);

  SplitLinkHeaderForTesting(" 1\t,\t2,3", &values);
  ASSERT_EQ(3u, values.size());
  EXPECT_EQ("1", values[0]);
  EXPECT_EQ("2", values[1]);
  EXPECT_EQ("3", values[2]);
}

TEST(LinkHeaderTest, SplitSkipsEmpty) {
  std::vector<std::string> values;
  SplitLinkHeaderForTesting(", foo, , \t, bar", &values);
  ASSERT_EQ(2u, values.size());
  EXPECT_EQ("foo", values[0]);
  EXPECT_EQ("bar", values[1]);
}

TEST(LinkHeaderTest, SplitQuotes) {
  std::vector<std::string> values;
  SplitLinkHeaderForTesting("\"foo,bar\", 'bar,foo', <hel,lo>", &values);
  ASSERT_EQ(3u, values.size());
  EXPECT_EQ("\"foo,bar\"", values[0]);
  EXPECT_EQ("'bar,foo'", values[1]);
  EXPECT_EQ("<hel,lo>", values[2]);
}

TEST(LinkHeaderTest, SplitEscapedQuotes) {
  std::vector<std::string> values;
  SplitLinkHeaderForTesting("\"f\\\"oo,bar\", 'b\\'ar,foo', <hel\\>,lo>",
                            &values);
  ASSERT_EQ(4u, values.size());
  EXPECT_EQ("\"f\\\"oo,bar\"", values[0]);
  EXPECT_EQ("'b\\'ar,foo'", values[1]);
  EXPECT_EQ("<hel\\>", values[2]);
  EXPECT_EQ("lo>", values[3]);
}

struct SimpleParseTestData {
  const char* link;
  bool valid;
  const char* url;
  const char* rel;
  const char* as;
};

void PrintTo(const SimpleParseTestData& test, std::ostream* os) {
  *os << ::testing::PrintToString(test.link);
}

class SimpleParseTest : public ::testing::TestWithParam<SimpleParseTestData> {};

TEST_P(SimpleParseTest, Simple) {
  const SimpleParseTestData test = GetParam();

  std::string url;
  std::unordered_map<std::string, std::string> params;
  EXPECT_EQ(test.valid,
            ParseLinkHeaderValueForTesting(test.link, &url, &params));
  if (test.valid) {
    EXPECT_EQ(test.url, url);
    EXPECT_EQ(test.rel, params["rel"]);
    EXPECT_EQ(test.as, params["as"]);
  }
}

// Test data mostly copied from blink::LinkHeaderTest. Expectations for some
// test cases are different though. Mostly because blink::LinkHeader is stricter
// about validity while parsing (primarily things like mismatched quotes), and
// factors in knowledge about semantics of Link headers (parameters that are
// required to have a value if they occur, some parameters are auto-lower-cased,
// headers with an "anchor" parameter are rejected by base::LinkHeader).
// The code this tests purely parses without actually interpreting the data, as
// it is expected that another layer on top will do more specific validations.
const SimpleParseTestData simple_parse_tests[] = {
    {"</images/cat.jpg>; rel=prefetch", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>;rel=prefetch", true, "/images/cat.jpg", "prefetch", ""},
    {"</images/cat.jpg>   ;rel=prefetch", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>   ;   rel=prefetch", true, "/images/cat.jpg",
     "prefetch", ""},
    {"< /images/cat.jpg>   ;   rel=prefetch", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/cat.jpg >   ;   rel=prefetch", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/cat.jpg wutwut>   ;   rel=prefetch", true,
     "/images/cat.jpg wutwut", "prefetch", ""},
    {"</images/cat.jpg wutwut  \t >   ;   rel=prefetch", true,
     "/images/cat.jpg wutwut", "prefetch", ""},
    {"</images/cat.jpg>; rel=prefetch   ", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>; Rel=prefetch   ", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>; Rel=PReFetCh   ", true, "/images/cat.jpg", "PReFetCh",
     ""},
    {"</images/cat.jpg>; rel=prefetch; rel=somethingelse", true,
     "/images/cat.jpg", "prefetch", ""},
    {"</images/cat.jpg>\t\t ; \trel=prefetch \t  ", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/cat.jpg>; rel= prefetch", true, "/images/cat.jpg", "prefetch",
     ""},
    {"<../images/cat.jpg?dog>; rel= prefetch", true, "../images/cat.jpg?dog",
     "prefetch", ""},
    {"</images/cat.jpg>; rel =prefetch", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>; rel pel=prefetch", false},
    {"< /images/cat.jpg>", true, "/images/cat.jpg", "", ""},
    {"</images/cat.jpg>; wut=sup; rel =prefetch", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/cat.jpg>; wut=sup ; rel =prefetch", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/cat.jpg>; wut=sup ; rel =prefetch  \t  ;", true,
     "/images/cat.jpg", "prefetch", ""},
    {"</images/cat.jpg> wut=sup ; rel =prefetch  \t  ;", false},
    {"<   /images/cat.jpg", false},
    {"<   http://wut.com/  sdfsdf ?sd>; rel=dns-prefetch", true,
     "http://wut.com/  sdfsdf ?sd", "dns-prefetch", ""},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=dns-prefetch", true,
     "http://wut.com/%20%20%3dsdfsdf?sd", "dns-prefetch", ""},
    {"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>; rel=prefetch", true,
     "http://wut.com/dfsdf?sdf=ghj&wer=rty", "prefetch", ""},
    {"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>;;;;; rel=prefetch", true,
     "http://wut.com/dfsdf?sdf=ghj&wer=rty", "prefetch", ""},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=preload;as=image", true,
     "http://wut.com/%20%20%3dsdfsdf?sd", "preload", "image"},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=preload;as=whatever", true,
     "http://wut.com/%20%20%3dsdfsdf?sd", "preload", "whatever"},
    {"</images/cat.jpg>; rel=prefetch;", true, "/images/cat.jpg", "prefetch",
     ""},
    {"</images/cat.jpg>; rel=prefetch    ;", true, "/images/cat.jpg",
     "prefetch", ""},
    {"</images/ca,t.jpg>; rel=prefetch    ;", true, "/images/ca,t.jpg",
     "prefetch", ""},
    {"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE and "
     "backslash\"",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE \\\" and "
     "backslash: \\\"",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; title=\"title with a DQUOTE \\\" and backslash: \"; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; title=\'title with a DQUOTE \\\' and backslash: \'; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; title=\"title with a DQUOTE \\\" and ;backslash,: \"; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; title=\"title with a DQUOTE \' and ;backslash,: \"; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", ""},
    {"<simple.css>; title=\"\"; rel=stylesheet; ", true, "simple.css",
     "stylesheet", ""},
    {"<simple.css>; title=\"\"; rel=\"stylesheet\"; ", true, "simple.css",
     "stylesheet", ""},
    {"<simple.css>; rel=stylesheet; title=\"", true, "simple.css", "stylesheet",
     ""},
    {"<simple.css>; rel=stylesheet; title=\"\"", true, "simple.css",
     "stylesheet", ""},
    {"<simple.css>; rel=\"stylesheet\"; title=\"", true, "simple.css",
     "stylesheet", ""},
    {"<simple.css>; rel=\";style,sheet\"; title=\"", true, "simple.css",
     ";style,sheet", ""},
    {"<simple.css>; rel=\"bla'sdf\"; title=\"", true, "simple.css", "bla'sdf",
     ""},
    {"<simple.css>; rel=\"\"; title=\"\"", true, "simple.css", "", ""},
    {"<simple.css>; rel=''; title=\"\"", true, "simple.css", "", ""},
    {"<simple.css>; rel=''; bla", true, "simple.css", "", ""},
    {"<simple.css>; rel='prefetch", true, "simple.css", "prefetch", ""},
    {"<simple.css>; rel=\"prefetch", true, "simple.css", "prefetch", ""},
    {"<simple.css>; rel=\"", true, "simple.css", "", ""},
    {"simple.css; rel=prefetch", false},
    {"<simple.css>; rel=prefetch; rel=foobar", true, "simple.css", "prefetch",
     ""},
};

INSTANTIATE_TEST_CASE_P(LinkHeaderTest,
                        SimpleParseTest,
                        testing::ValuesIn(simple_parse_tests));

void SaveFoundRegistrationsCallback(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    std::vector<ServiceWorkerRegistrationInfo>* registrations,
    ServiceWorkerStatusCode status,
    const std::vector<ServiceWorkerRegistrationInfo>& result) {
  EXPECT_EQ(expected_status, status);
  *called = true;
  *registrations = result;
}

ServiceWorkerContextWrapper::GetRegistrationsInfosCallback
SaveFoundRegistrations(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    std::vector<ServiceWorkerRegistrationInfo>* registrations) {
  *called = false;
  return base::Bind(&SaveFoundRegistrationsCallback, expected_status, called,
                    registrations);
}

class LinkHeaderServiceWorkerTest : public ::testing::Test {
 public:
  LinkHeaderServiceWorkerTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        resource_context_(&request_context_) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
  ~LinkHeaderServiceWorkerTest() override {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void TearDown() override { helper_.reset(); }

  ServiceWorkerContextWrapper* context_wrapper() {
    return helper_->context_wrapper();
  }

  void ProcessLinkHeader(const GURL& request_url,
                         const std::string& link_header) {
    scoped_ptr<net::URLRequest> request = request_context_.CreateRequest(
        request_url, net::DEFAULT_PRIORITY, &request_delegate_);
    ResourceRequestInfo::AllocateForTesting(
        request.get(), RESOURCE_TYPE_SCRIPT, &resource_context_,
        -1 /* render_process_id */, -1 /* render_view_id */,
        -1 /* render_frame_id */, false /* is_main_frame */,
        false /* parent_is_main_frame */, true /* allow_download */,
        true /* is_async */, false /* is_using_lofi */);

    ProcessLinkHeaderForRequest(request.get(), link_header, context_wrapper());
    base::RunLoop().RunUntilIdle();
  }

  std::vector<ServiceWorkerRegistrationInfo> GetRegistrations() {
    bool called;
    std::vector<ServiceWorkerRegistrationInfo> registrations;
    context_wrapper()->GetAllRegistrations(
        SaveFoundRegistrations(SERVICE_WORKER_OK, &called, &registrations));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
    return registrations;
  }

 private:
  TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  net::TestURLRequestContext request_context_;
  net::TestDelegate request_delegate_;
  MockResourceContext resource_context_;
};

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_Basic) {
  ProcessLinkHeader(GURL("https://example.com/foo/bar/"),
                    "<../foo.js>; rel=serviceworker");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foo/"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foo/foo.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeWithFragment) {
  ProcessLinkHeader(GURL("https://example.com/foo/bar/"),
                    "<../bar.js>; rel=serviceworker; scope=\"scope#ref\"");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foo/bar/scope"),
            registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foo/bar.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeAbsoluteUrl) {
  ProcessLinkHeader(GURL("https://example.com/foo/bar/"),
                    "<bar.js>; rel=serviceworker; "
                    "scope=\"https://example.com:443/foo/bar/scope\"");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foo/bar/scope"),
            registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foo/bar/bar.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeDifferentOrigin) {
  ProcessLinkHeader(
      GURL("https://example.com/foobar/"),
      "<bar.js>; rel=serviceworker; scope=\"https://google.com/scope\"");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScopeUrlEncodedSlash) {
  ProcessLinkHeader(GURL("https://example.com/foobar/"),
                    "<bar.js>; rel=serviceworker; scope=\"./foo%2Fbar\"");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_ScriptUrlEncodedSlash) {
  ProcessLinkHeader(GURL("https://example.com/foobar/"),
                    "<foo%2Fbar.js>; rel=serviceworker");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_ScriptAbsoluteUrl) {
  ProcessLinkHeader(
      GURL("https://example.com/foobar/"),
      "<https://example.com/bar.js>; rel=serviceworker; scope=foo");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foobar/foo"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/bar.js"),
            registrations[0].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_ScriptDifferentOrigin) {
  ProcessLinkHeader(
      GURL("https://example.com/foobar/"),
      "<https://google.com/bar.js>; rel=serviceworker; scope=foo");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(0u, registrations.size());
}

TEST_F(LinkHeaderServiceWorkerTest, InstallServiceWorker_MultipleWorkers) {
  ProcessLinkHeader(GURL("https://example.com/foobar/"),
                    "<bar.js>; rel=serviceworker; scope=foo, <baz.js>; "
                    "rel=serviceworker; scope=scope");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(2u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foobar/foo"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foobar/bar.js"),
            registrations[0].active_version.script_url);
  EXPECT_EQ(GURL("https://example.com/foobar/scope"), registrations[1].pattern);
  EXPECT_EQ(GURL("https://example.com/foobar/baz.js"),
            registrations[1].active_version.script_url);
}

TEST_F(LinkHeaderServiceWorkerTest,
       InstallServiceWorker_ValidAndInvalidValues) {
  ProcessLinkHeader(
      GURL("https://example.com/foobar/"),
      "<https://google.com/bar.js>; rel=serviceworker; scope=foo, <baz.js>; "
      "rel=serviceworker; scope=scope");

  std::vector<ServiceWorkerRegistrationInfo> registrations = GetRegistrations();
  ASSERT_EQ(1u, registrations.size());
  EXPECT_EQ(GURL("https://example.com/foobar/scope"), registrations[0].pattern);
  EXPECT_EQ(GURL("https://example.com/foobar/baz.js"),
            registrations[0].active_version.script_url);
}

}  // namespace

}  // namespace content
