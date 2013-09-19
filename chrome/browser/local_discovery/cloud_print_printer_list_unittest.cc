// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/local_discovery/cloud_print_printer_list.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::StrictMock;
using testing::Mock;

namespace local_discovery {

namespace {

const char kSampleSuccessResponseOAuth[] = "{"
    "   \"success\": true,"
    "   \"printers\": ["
    "     {\"id\" : \"someID\","
    "      \"displayName\": \"someDisplayName\","
    "      \"description\": \"someDescription\"}"
    "    ]"
    "}";

class TestOAuth2TokenService : public OAuth2TokenService {
 public:
  explicit TestOAuth2TokenService(net::URLRequestContextGetter* request_context)
      : request_context_(request_context) {
  }
 protected:
  virtual std::string GetRefreshToken(const std::string& account_id) OVERRIDE {
    return "SampleToken";
  }

  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE {
    return request_context_.get();
  }

 private:
  scoped_refptr<net::URLRequestContextGetter> request_context_;
};

class MockDelegate : public CloudPrintPrinterList::Delegate {
 public:
  MOCK_METHOD0(OnCloudPrintPrinterListUnavailable, void());
  MOCK_METHOD0(OnCloudPrintPrinterListReady, void());
};

class CloudPrintPrinterListTest : public testing::Test {
 public:
  CloudPrintPrinterListTest()
      : ui_thread_(content::BrowserThread::UI,
                   &loop_),
        request_context_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())),
        token_service_(request_context_.get()) {
    ui_thread_.Stop();  // HACK: Fake being on the UI thread

    printer_list_.reset(
        new CloudPrintPrinterList(request_context_.get(),
                                  "http://SoMeUrL.com/cloudprint",
                                  &token_service_,
                                  "account_id",
                                  &delegate_));

    fallback_fetcher_factory_.reset(new net::TestURLFetcherFactory());
    net::URLFetcherImpl::set_factory(NULL);
    fetcher_factory_.reset(new net::FakeURLFetcherFactory(
        fallback_fetcher_factory_.get()));
  }

  virtual ~CloudPrintPrinterListTest() {
    fetcher_factory_.reset();
    net::URLFetcherImpl::set_factory(fallback_fetcher_factory_.get());
    fallback_fetcher_factory_.reset();
  }

 protected:
  base::MessageLoopForUI loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  // Use a test factory as a fallback so we don't have to deal with OAuth2
  // requests.
  scoped_ptr<net::TestURLFetcherFactory> fallback_fetcher_factory_;
  scoped_ptr<net::FakeURLFetcherFactory> fetcher_factory_;
  TestOAuth2TokenService token_service_;
  StrictMock<MockDelegate> delegate_;
  scoped_ptr<CloudPrintPrinterList> printer_list_;
};

TEST_F(CloudPrintPrinterListTest, SuccessOAuth2) {
  fetcher_factory_->SetFakeResponse("http://SoMeUrL.com/cloudprint/search",
                                   kSampleSuccessResponseOAuth,
                                   true);


  CloudPrintBaseApiFlow* cloudprint_flow =
      printer_list_->GetOAuth2ApiFlowForTests();

  printer_list_->Start();

  cloudprint_flow->OnGetTokenSuccess(NULL, "SomeToken", base::Time());

  EXPECT_CALL(delegate_, OnCloudPrintPrinterListReady());

  base::MessageLoop::current()->RunUntilIdle();

  Mock::VerifyAndClear(&delegate_);

  std::set<std::string> ids_found;
  std::set<std::string> ids_expected;

  ids_expected.insert("someID");

  int length = 0;
  for (CloudPrintPrinterList::iterator i = printer_list_->begin();
       i != printer_list_->end(); i++, length++) {
    ids_found.insert(i->id);
  }

  EXPECT_EQ(ids_expected, ids_found);
  EXPECT_EQ(1, length);

  const CloudPrintPrinterList::PrinterDetails* found =
      printer_list_->GetDetailsFor("someID");
  ASSERT_TRUE(found != NULL);
  EXPECT_EQ("someID", found->id);
  EXPECT_EQ("someDisplayName", found->display_name);
  EXPECT_EQ("someDescription", found->description);
}

}  // namespace

}  // namespace local_discovery
