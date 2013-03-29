// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_loader.h"

#include "base/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/mock_resource_context.h"
#include "content/test/test_content_browser_client.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// Stub client certificate store that returns a preset list of certificates for
// each request and records the arguments of the most recent request for later
// inspection.
class ClientCertStoreStub : public net::ClientCertStore {
 public:
  ClientCertStoreStub(const net::CertificateList& certs)
      : response_(certs),
        request_count_(0) {}

  virtual ~ClientCertStoreStub() {}

  // Returns |cert_authorities| field of the certificate request passed in the
  // most recent call to GetClientCerts().
  // TODO(ppi): Make the stub independent from the internal representation of
  // SSLCertRequestInfo. For now it seems that we cannot neither save the
  // scoped_refptr<> (since it is never passed to us) nor copy the entire
  // CertificateRequestInfo (since there is no copy constructor).
  std::vector<std::string> requested_authorities() {
    return requested_authorities_;
  }

  // Returns the number of calls to GetClientCerts().
  int request_count() {
    return request_count_;
  }

  // net::ClientCertStore:
  virtual bool GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                              net::CertificateList* selected_certs) OVERRIDE {
    ++request_count_;
    requested_authorities_ = cert_request_info.cert_authorities;
    *selected_certs = response_;
    return true;
  }

 private:
  const net::CertificateList response_;
  int request_count_;
  std::vector<std::string> requested_authorities_;
};

// Dummy implementation of ResourceHandler, instance of which is needed to
// initialize ResourceLoader.
class ResourceHandlerStub : public ResourceHandler {
 public:
  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE {
    return true;
  }

  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) OVERRIDE {
    return true;
  }

  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response,
                                 bool* defer) OVERRIDE { return true; }

  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE {
    return true;
  }

  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE {
    return true;
  }

  virtual bool OnReadCompleted(int request_id,
                               int bytes_read,
                               bool* defer) OVERRIDE {
    return true;
  }

  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE {
    return true;
  }

  virtual void OnDataDownloaded(int request_id,
                                int bytes_downloaded) OVERRIDE {}
};

// Test browser client that captures calls to SelectClientCertificates and
// records the arguments of the most recent call for later inspection.
class SelectCertificateBrowserClient : public TestContentBrowserClient {
 public:
  SelectCertificateBrowserClient() : call_count_(0) {}

  virtual void SelectClientCertificate(
      int render_process_id,
      int render_view_id,
      const net::HttpNetworkSession* network_session,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback) OVERRIDE {
    ++call_count_;
    passed_certs_ = cert_request_info->client_certs;
  }

  int call_count() {
    return call_count_;
  }

  net::CertificateList passed_certs() {
    return passed_certs_;
  }

 private:
  net::CertificateList passed_certs_;
  int call_count_;
};

}  // namespace

class ResourceLoaderTest : public testing::Test,
                           public ResourceLoaderDelegate {
 protected:
  // testing::Test:
  virtual void SetUp() OVERRIDE {
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI,
                                           message_loop_.get()));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));
  }

  // ResourceLoaderDelegate:
  virtual ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      ResourceLoader* loader,
      net::AuthChallengeInfo* auth_info) OVERRIDE {
    return NULL;
  }
  virtual bool AcceptAuthRequest(
      ResourceLoader* loader,
      net::AuthChallengeInfo* auth_info) OVERRIDE {
    return false;
  };
  virtual bool AcceptSSLClientCertificateRequest(
      ResourceLoader* loader,
      net::SSLCertRequestInfo* cert_info) OVERRIDE {
    return true;
  }
  virtual bool HandleExternalProtocol(ResourceLoader* loader,
                                      const GURL& url) OVERRIDE {
    return false;
  }
  virtual void DidStartRequest(ResourceLoader* loader) OVERRIDE {}
  virtual void DidReceiveRedirect(ResourceLoader* loader,
                                  const GURL& new_url) OVERRIDE {}
  virtual void DidReceiveResponse(ResourceLoader* loader) OVERRIDE {}
  virtual void DidFinishLoading(ResourceLoader* loader) OVERRIDE {}

  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> ui_thread_;
  scoped_ptr<BrowserThreadImpl> io_thread_;

  content::MockResourceContext resource_context_;
};

// When OpenSSL is used, client cert store is not being queried in
// ResourceLoader.
#if !defined(USE_OPENSSL)
// Verifies if a call to net::UrlRequest::Delegate::OnCertificateRequested()
// causes client cert store to be queried for certificates and if the returned
// certificates are correctly passed to the content browser client for
// selection.
TEST_F(ResourceLoaderTest, ClientCertStoreLookup) {
  const int kRenderProcessId = 1;
  const int kRenderViewId = 2;

  scoped_ptr<net::URLRequest> request(new net::URLRequest(
      GURL("dummy"), NULL,
      resource_context_.GetRequestContext()));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          ResourceType::MAIN_FRAME,
                                          &resource_context_,
                                          kRenderProcessId,
                                          kRenderViewId);

  // Set up the test client cert store.
  net::CertificateList dummy_certs(1, scoped_refptr<net::X509Certificate>(
      new net::X509Certificate("test", "test", base::Time(), base::Time())));
  scoped_ptr<ClientCertStoreStub> test_store(
      new ClientCertStoreStub(dummy_certs));
  EXPECT_EQ(0, test_store->request_count());

  // Ownership of the |request| and |test_store| is about to be turned over to
  // ResourceLoader. We need to keep raw pointer copies to access these objects
  // later.
  net::URLRequest* raw_ptr_to_request = request.get();
  ClientCertStoreStub* raw_ptr_to_store = test_store.get();

  scoped_ptr<ResourceHandler> resource_handler(new ResourceHandlerStub());
  ResourceLoader loader(request.Pass(), resource_handler.Pass(), this,
                        test_store.PassAs<net::ClientCertStore>());

  // Prepare a dummy certificate request.
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info(
      new net::SSLCertRequestInfo());
  std::vector<std::string> dummy_authority(1, "dummy");
  cert_request_info->cert_authorities = dummy_authority;

  // Plug in test content browser client.
  ContentBrowserClient* old_client = GetContentClient()->browser();
  SelectCertificateBrowserClient test_client;
  GetContentClient()->set_browser_for_testing(&test_client);

  // Everything is set up. Trigger the resource loader certificate request event
  // and run the message loop.
  loader.OnCertificateRequested(raw_ptr_to_request, cert_request_info.get());
  message_loop_->RunUntilIdle();

  // Restore the original content browser client.
  GetContentClient()->set_browser_for_testing(old_client);

  // Check if the test store was queried against correct |cert_authorities|.
  EXPECT_EQ(1, raw_ptr_to_store->request_count());
  EXPECT_EQ(dummy_authority, raw_ptr_to_store->requested_authorities());

  // Check if the retrieved certificates were passed to the content browser
  // client.
  EXPECT_EQ(1, test_client.call_count());
  EXPECT_EQ(dummy_certs, test_client.passed_certs());
}
#endif  // !defined(OPENSSL)

}  // namespace content
