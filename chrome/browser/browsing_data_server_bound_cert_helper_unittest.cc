// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_server_bound_cert_helper.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "net/base/server_bound_cert_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class BrowsingDataServerBoundCertHelperTest : public testing::Test {
 public:
  virtual void SetUp() {
    ui_thread_.reset(new content::TestBrowserThread(BrowserThread::UI,
                                                    &message_loop_));
    io_thread_.reset(new content::TestBrowserThread(BrowserThread::IO,
                                                    &message_loop_));
    testing_profile_.reset(new TestingProfile());
    testing_profile_->CreateRequestContext();
  }

  void CreateCertsForTest() {
    net::URLRequestContext* context =
        testing_profile_->GetRequestContext()->GetURLRequestContext();
    net::ServerBoundCertStore* cert_store =
        context->server_bound_cert_service()->GetCertStore();
    cert_store->SetServerBoundCert("https://www.google.com:443",
                                   net::CLIENT_CERT_RSA_SIGN,
                                   base::Time(), base::Time(),
                                   "key", "cert");
    cert_store->SetServerBoundCert("https://www.youtube.com:443",
                                   net::CLIENT_CERT_RSA_SIGN,
                                   base::Time(), base::Time(),
                                   "key", "cert");
  }

  void FetchCallback(
      const net::ServerBoundCertStore::ServerBoundCertList& certs) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    server_bound_cert_list_ = certs;
    MessageLoop::current()->Quit();
  }

 protected:
  MessageLoop message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
  scoped_ptr<content::TestBrowserThread> io_thread_;
  scoped_ptr<TestingProfile> testing_profile_;

  net::ServerBoundCertStore::ServerBoundCertList server_bound_cert_list_;
};

TEST_F(BrowsingDataServerBoundCertHelperTest, FetchData) {
  CreateCertsForTest();
  scoped_refptr<BrowsingDataServerBoundCertHelper> helper(
      BrowsingDataServerBoundCertHelper::Create(testing_profile_.get()));

  helper->StartFetching(
      base::Bind(&BrowsingDataServerBoundCertHelperTest::FetchCallback,
                 base::Unretained(this)));

  // Blocks until BrowsingDataServerBoundCertHelperTest::FetchCallback is
  // notified.
  MessageLoop::current()->Run();

  ASSERT_EQ(2UL, server_bound_cert_list_.size());
  net::ServerBoundCertStore::ServerBoundCertList::const_iterator it =
      server_bound_cert_list_.begin();

  // Correct because fetching server_bound_cert_list_ will get them out in the
  // same order CreateCertsForTest put them in.
  ASSERT_TRUE(it != server_bound_cert_list_.end());
  EXPECT_EQ("https://www.google.com:443", it->server_identifier());

  ASSERT_TRUE(++it != server_bound_cert_list_.end());
  EXPECT_EQ("https://www.youtube.com:443", it->server_identifier());

  ASSERT_TRUE(++it == server_bound_cert_list_.end());
}

TEST_F(BrowsingDataServerBoundCertHelperTest, DeleteCert) {
  CreateCertsForTest();
  scoped_refptr<BrowsingDataServerBoundCertHelper> helper(
      BrowsingDataServerBoundCertHelper::Create(testing_profile_.get()));

  helper->DeleteServerBoundCert("https://www.google.com:443");

  helper->StartFetching(
      base::Bind(&BrowsingDataServerBoundCertHelperTest::FetchCallback,
                 base::Unretained(this)));
  MessageLoop::current()->Run();

  ASSERT_EQ(1UL, server_bound_cert_list_.size());
  net::ServerBoundCertStore::ServerBoundCertList::const_iterator it =
      server_bound_cert_list_.begin();

  ASSERT_TRUE(it != server_bound_cert_list_.end());
  EXPECT_EQ("https://www.youtube.com:443", it->server_identifier());

  ASSERT_TRUE(++it == server_bound_cert_list_.end());

  helper->DeleteServerBoundCert("https://www.youtube.com:443");

  helper->StartFetching(
      base::Bind(&BrowsingDataServerBoundCertHelperTest::FetchCallback,
                 base::Unretained(this)));
  MessageLoop::current()->Run();
  ASSERT_EQ(0UL, server_bound_cert_list_.size());
}

TEST_F(BrowsingDataServerBoundCertHelperTest, CannedUnique) {
  std::string origin = "https://www.google.com:443";

  scoped_refptr<CannedBrowsingDataServerBoundCertHelper> helper(
      new CannedBrowsingDataServerBoundCertHelper());

  ASSERT_TRUE(helper->empty());
  helper->AddServerBoundCert(net::ServerBoundCertStore::ServerBoundCert(
      origin, net::CLIENT_CERT_RSA_SIGN, base::Time(), base::Time(), "key",
      "cert"));
  helper->AddServerBoundCert(net::ServerBoundCertStore::ServerBoundCert(
      origin, net::CLIENT_CERT_ECDSA_SIGN, base::Time(), base::Time(), "key",
      "cert"));

  helper->StartFetching(
      base::Bind(&BrowsingDataServerBoundCertHelperTest::FetchCallback,
                 base::Unretained(this)));
  MessageLoop::current()->Run();

  ASSERT_EQ(1UL, server_bound_cert_list_.size());
  net::ServerBoundCertStore::ServerBoundCert& cert =
      server_bound_cert_list_.front();

  EXPECT_EQ("https://www.google.com:443", cert.server_identifier());
  EXPECT_EQ(net::CLIENT_CERT_ECDSA_SIGN, cert.type());
}

TEST_F(BrowsingDataServerBoundCertHelperTest, CannedEmpty) {
  std::string origin = "https://www.google.com";

  scoped_refptr<CannedBrowsingDataServerBoundCertHelper> helper(
      new CannedBrowsingDataServerBoundCertHelper());

  ASSERT_TRUE(helper->empty());
  helper->AddServerBoundCert(net::ServerBoundCertStore::ServerBoundCert(
      origin, net::CLIENT_CERT_RSA_SIGN, base::Time(), base::Time(), "key",
      "cert"));
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}
