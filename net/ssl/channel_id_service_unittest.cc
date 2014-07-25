// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/channel_id_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/task_runner.h"
#include "crypto/ec_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/default_channel_id_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void FailTest(int /* result */) {
  FAIL();
}

// Simple task runner that refuses to actually post any tasks. This simulates
// a TaskRunner that has been shutdown, by returning false for any attempt to
// add new tasks.
class FailingTaskRunner : public base::TaskRunner {
 public:
  FailingTaskRunner() {}

  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE {
    return false;
  }

  virtual bool RunsTasksOnCurrentThread() const OVERRIDE { return true; }

 protected:
  virtual ~FailingTaskRunner() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FailingTaskRunner);
};

class MockChannelIDStoreWithAsyncGet
    : public DefaultChannelIDStore {
 public:
  MockChannelIDStoreWithAsyncGet()
      : DefaultChannelIDStore(NULL), channel_id_count_(0) {}

  virtual int GetChannelID(const std::string& server_identifier,
                           base::Time* expiration_time,
                           std::string* private_key_result,
                           std::string* cert_result,
                           const GetChannelIDCallback& callback) OVERRIDE;

  virtual void SetChannelID(const std::string& server_identifier,
                            base::Time creation_time,
                            base::Time expiration_time,
                            const std::string& private_key,
                            const std::string& cert) OVERRIDE {
    channel_id_count_ = 1;
  }

  virtual int GetChannelIDCount() OVERRIDE { return channel_id_count_; }

  void CallGetChannelIDCallbackWithResult(int err,
                                          base::Time expiration_time,
                                          const std::string& private_key,
                                          const std::string& cert);

 private:
  GetChannelIDCallback callback_;
  std::string server_identifier_;
  int channel_id_count_;
};

int MockChannelIDStoreWithAsyncGet::GetChannelID(
    const std::string& server_identifier,
    base::Time* expiration_time,
    std::string* private_key_result,
    std::string* cert_result,
    const GetChannelIDCallback& callback) {
  server_identifier_ = server_identifier;
  callback_ = callback;
  // Reset the cert count, it'll get incremented in either SetChannelID or
  // CallGetChannelIDCallbackWithResult.
  channel_id_count_ = 0;
  // Do nothing else: the results to be provided will be specified through
  // CallGetChannelIDCallbackWithResult.
  return ERR_IO_PENDING;
}

void
MockChannelIDStoreWithAsyncGet::CallGetChannelIDCallbackWithResult(
    int err,
    base::Time expiration_time,
    const std::string& private_key,
    const std::string& cert) {
  if (err == OK)
    channel_id_count_ = 1;
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback_,
                                                    err,
                                                    server_identifier_,
                                                    expiration_time,
                                                    private_key,
                                                    cert));
}

class ChannelIDServiceTest : public testing::Test {
 public:
  ChannelIDServiceTest()
      : service_(new ChannelIDService(
            new DefaultChannelIDStore(NULL),
            base::MessageLoopProxy::current())) {
  }

 protected:
  scoped_ptr<ChannelIDService> service_;
};

TEST_F(ChannelIDServiceTest, GetDomainForHost) {
  EXPECT_EQ("google.com",
            ChannelIDService::GetDomainForHost("google.com"));
  EXPECT_EQ("google.com",
            ChannelIDService::GetDomainForHost("www.google.com"));
  EXPECT_EQ("foo.appspot.com",
            ChannelIDService::GetDomainForHost("foo.appspot.com"));
  EXPECT_EQ("bar.appspot.com",
            ChannelIDService::GetDomainForHost("foo.bar.appspot.com"));
  EXPECT_EQ("appspot.com",
            ChannelIDService::GetDomainForHost("appspot.com"));
  EXPECT_EQ("google.com",
            ChannelIDService::GetDomainForHost("www.mail.google.com"));
  EXPECT_EQ("goto",
            ChannelIDService::GetDomainForHost("goto"));
  EXPECT_EQ("127.0.0.1",
            ChannelIDService::GetDomainForHost("127.0.0.1"));
}

TEST_F(ChannelIDServiceTest, GetCacheMiss) {
  std::string host("encrypted.google.com");

  int error;
  TestCompletionCallback callback;
  ChannelIDService::RequestHandle request_handle;

  // Synchronous completion, because the store is initialized.
  std::string private_key, der_cert;
  EXPECT_EQ(0, service_->cert_count());
  error = service_->GetChannelID(
      host, &private_key, &der_cert, callback.callback(), &request_handle);
  EXPECT_EQ(ERR_FILE_NOT_FOUND, error);
  EXPECT_FALSE(request_handle.is_active());
  EXPECT_EQ(0, service_->cert_count());
  EXPECT_TRUE(der_cert.empty());
}

TEST_F(ChannelIDServiceTest, CacheHit) {
  std::string host("encrypted.google.com");

  int error;
  TestCompletionCallback callback;
  ChannelIDService::RequestHandle request_handle;

  // Asynchronous completion.
  std::string private_key_info1, der_cert1;
  EXPECT_EQ(0, service_->cert_count());
  error = service_->GetOrCreateChannelID(
      host, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service_->cert_count());
  EXPECT_FALSE(private_key_info1.empty());
  EXPECT_FALSE(der_cert1.empty());
  EXPECT_FALSE(request_handle.is_active());

  // Synchronous completion.
  std::string private_key_info2, der_cert2;
  error = service_->GetOrCreateChannelID(
      host, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_FALSE(request_handle.is_active());
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service_->cert_count());
  EXPECT_EQ(private_key_info1, private_key_info2);
  EXPECT_EQ(der_cert1, der_cert2);

  // Synchronous get.
  std::string private_key_info3, der_cert3;
  error = service_->GetChannelID(
      host, &private_key_info3, &der_cert3, callback.callback(),
      &request_handle);
  EXPECT_FALSE(request_handle.is_active());
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service_->cert_count());
  EXPECT_EQ(der_cert1, der_cert3);
  EXPECT_EQ(private_key_info1, private_key_info3);

  EXPECT_EQ(3u, service_->requests());
  EXPECT_EQ(2u, service_->cert_store_hits());
  EXPECT_EQ(0u, service_->inflight_joins());
}

TEST_F(ChannelIDServiceTest, StoreChannelIDs) {
  int error;
  TestCompletionCallback callback;
  ChannelIDService::RequestHandle request_handle;

  std::string host1("encrypted.google.com");
  std::string private_key_info1, der_cert1;
  EXPECT_EQ(0, service_->cert_count());
  error = service_->GetOrCreateChannelID(
      host1, &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service_->cert_count());

  std::string host2("www.verisign.com");
  std::string private_key_info2, der_cert2;
  error = service_->GetOrCreateChannelID(
      host2, &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(2, service_->cert_count());

  std::string host3("www.twitter.com");
  std::string private_key_info3, der_cert3;
  error = service_->GetOrCreateChannelID(
      host3, &private_key_info3, &der_cert3,
      callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(3, service_->cert_count());

  EXPECT_NE(private_key_info1, private_key_info2);
  EXPECT_NE(der_cert1, der_cert2);
  EXPECT_NE(private_key_info1, private_key_info3);
  EXPECT_NE(der_cert1, der_cert3);
  EXPECT_NE(private_key_info2, private_key_info3);
  EXPECT_NE(der_cert2, der_cert3);
}

// Tests an inflight join.
TEST_F(ChannelIDServiceTest, InflightJoin) {
  std::string host("encrypted.google.com");
  int error;

  std::string private_key_info1, der_cert1;
  TestCompletionCallback callback1;
  ChannelIDService::RequestHandle request_handle1;

  std::string private_key_info2, der_cert2;
  TestCompletionCallback callback2;
  ChannelIDService::RequestHandle request_handle2;

  error = service_->GetOrCreateChannelID(
      host, &private_key_info1, &der_cert1,
      callback1.callback(), &request_handle1);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle1.is_active());
  // Should join with the original request.
  error = service_->GetOrCreateChannelID(
      host, &private_key_info2, &der_cert2,
      callback2.callback(), &request_handle2);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle2.is_active());

  error = callback1.WaitForResult();
  EXPECT_EQ(OK, error);
  error = callback2.WaitForResult();
  EXPECT_EQ(OK, error);

  EXPECT_EQ(2u, service_->requests());
  EXPECT_EQ(0u, service_->cert_store_hits());
  EXPECT_EQ(1u, service_->inflight_joins());
  EXPECT_EQ(1u, service_->workers_created());
}

// Tests an inflight join of a Get request to a GetOrCreate request.
TEST_F(ChannelIDServiceTest, InflightJoinGetOrCreateAndGet) {
  std::string host("encrypted.google.com");
  int error;

  std::string private_key_info1, der_cert1;
  TestCompletionCallback callback1;
  ChannelIDService::RequestHandle request_handle1;

  std::string private_key_info2;
  std::string der_cert2;
  TestCompletionCallback callback2;
  ChannelIDService::RequestHandle request_handle2;

  error = service_->GetOrCreateChannelID(
      host, &private_key_info1, &der_cert1,
      callback1.callback(), &request_handle1);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle1.is_active());
  // Should join with the original request.
  error = service_->GetChannelID(
      host, &private_key_info2, &der_cert2, callback2.callback(),
      &request_handle2);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle2.is_active());

  error = callback1.WaitForResult();
  EXPECT_EQ(OK, error);
  error = callback2.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(der_cert1, der_cert2);

  EXPECT_EQ(2u, service_->requests());
  EXPECT_EQ(0u, service_->cert_store_hits());
  EXPECT_EQ(1u, service_->inflight_joins());
  EXPECT_EQ(1u, service_->workers_created());
}

TEST_F(ChannelIDServiceTest, ExtractValuesFromBytesEC) {
  std::string host("encrypted.google.com");
  std::string private_key_info, der_cert;
  int error;
  TestCompletionCallback callback;
  ChannelIDService::RequestHandle request_handle;

  error = service_->GetOrCreateChannelID(
      host, &private_key_info, &der_cert, callback.callback(),
      &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());
  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);

  base::StringPiece spki_piece;
  ASSERT_TRUE(asn1::ExtractSPKIFromDERCert(der_cert, &spki_piece));
  std::vector<uint8> spki(
      spki_piece.data(),
      spki_piece.data() + spki_piece.size());

  // Check that we can retrieve the key from the bytes.
  std::vector<uint8> key_vec(private_key_info.begin(), private_key_info.end());
  scoped_ptr<crypto::ECPrivateKey> private_key(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          ChannelIDService::kEPKIPassword, key_vec, spki));
  EXPECT_TRUE(private_key != NULL);

  // Check that we can retrieve the cert from the bytes.
  scoped_refptr<X509Certificate> x509cert(
      X509Certificate::CreateFromBytes(der_cert.data(), der_cert.size()));
  EXPECT_TRUE(x509cert.get() != NULL);
}

// Tests that the callback of a canceled request is never made.
TEST_F(ChannelIDServiceTest, CancelRequest) {
  std::string host("encrypted.google.com");
  std::string private_key_info, der_cert;
  int error;
  ChannelIDService::RequestHandle request_handle;

  error = service_->GetOrCreateChannelID(host,
                                         &private_key_info,
                                         &der_cert,
                                         base::Bind(&FailTest),
                                         &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());
  request_handle.Cancel();
  EXPECT_FALSE(request_handle.is_active());

  // Wait for reply from ChannelIDServiceWorker to be posted back to the
  // ChannelIDService.
  base::MessageLoop::current()->RunUntilIdle();

  // Even though the original request was cancelled, the service will still
  // store the result, it just doesn't call the callback.
  EXPECT_EQ(1, service_->cert_count());
}

// Tests that destructing the RequestHandle cancels the request.
TEST_F(ChannelIDServiceTest, CancelRequestByHandleDestruction) {
  std::string host("encrypted.google.com");
  std::string private_key_info, der_cert;
  int error;
  {
    ChannelIDService::RequestHandle request_handle;

    error = service_->GetOrCreateChannelID(host,
                                           &private_key_info,
                                           &der_cert,
                                           base::Bind(&FailTest),
                                           &request_handle);
    EXPECT_EQ(ERR_IO_PENDING, error);
    EXPECT_TRUE(request_handle.is_active());
  }

  // Wait for reply from ChannelIDServiceWorker to be posted back to the
  // ChannelIDService.
  base::MessageLoop::current()->RunUntilIdle();

  // Even though the original request was cancelled, the service will still
  // store the result, it just doesn't call the callback.
  EXPECT_EQ(1, service_->cert_count());
}

TEST_F(ChannelIDServiceTest, DestructionWithPendingRequest) {
  std::string host("encrypted.google.com");
  std::string private_key_info, der_cert;
  int error;
  ChannelIDService::RequestHandle request_handle;

  error = service_->GetOrCreateChannelID(host,
                                         &private_key_info,
                                         &der_cert,
                                         base::Bind(&FailTest),
                                         &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());

  // Cancel request and destroy the ChannelIDService.
  request_handle.Cancel();
  service_.reset();

  // ChannelIDServiceWorker should not post anything back to the
  // non-existent ChannelIDService, but run the loop just to be sure it
  // doesn't.
  base::MessageLoop::current()->RunUntilIdle();

  // If we got here without crashing or a valgrind error, it worked.
}

// Tests that shutting down the sequenced worker pool and then making new
// requests gracefully fails.
// This is a regression test for http://crbug.com/236387
TEST_F(ChannelIDServiceTest, RequestAfterPoolShutdown) {
  scoped_refptr<FailingTaskRunner> task_runner(new FailingTaskRunner);
  service_.reset(new ChannelIDService(
      new DefaultChannelIDStore(NULL), task_runner));

  // Make a request that will force synchronous completion.
  std::string host("encrypted.google.com");
  std::string private_key_info, der_cert;
  int error;
  ChannelIDService::RequestHandle request_handle;

  error = service_->GetOrCreateChannelID(host,
                                         &private_key_info,
                                         &der_cert,
                                         base::Bind(&FailTest),
                                         &request_handle);
  // If we got here without crashing or a valgrind error, it worked.
  ASSERT_EQ(ERR_INSUFFICIENT_RESOURCES, error);
  EXPECT_FALSE(request_handle.is_active());
}

// Tests that simultaneous creation of different certs works.
TEST_F(ChannelIDServiceTest, SimultaneousCreation) {
  int error;

  std::string host1("encrypted.google.com");
  std::string private_key_info1, der_cert1;
  TestCompletionCallback callback1;
  ChannelIDService::RequestHandle request_handle1;

  std::string host2("foo.com");
  std::string private_key_info2, der_cert2;
  TestCompletionCallback callback2;
  ChannelIDService::RequestHandle request_handle2;

  std::string host3("bar.com");
  std::string private_key_info3, der_cert3;
  TestCompletionCallback callback3;
  ChannelIDService::RequestHandle request_handle3;

  error = service_->GetOrCreateChannelID(host1,
                                         &private_key_info1,
                                         &der_cert1,
                                         callback1.callback(),
                                         &request_handle1);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle1.is_active());

  error = service_->GetOrCreateChannelID(host2,
                                         &private_key_info2,
                                         &der_cert2,
                                         callback2.callback(),
                                         &request_handle2);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle2.is_active());

  error = service_->GetOrCreateChannelID(host3,
                                         &private_key_info3,
                                         &der_cert3,
                                         callback3.callback(),
                                         &request_handle3);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle3.is_active());

  error = callback1.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_FALSE(private_key_info1.empty());
  EXPECT_FALSE(der_cert1.empty());

  error = callback2.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_FALSE(private_key_info2.empty());
  EXPECT_FALSE(der_cert2.empty());

  error = callback3.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_FALSE(private_key_info3.empty());
  EXPECT_FALSE(der_cert3.empty());

  EXPECT_NE(private_key_info1, private_key_info2);
  EXPECT_NE(der_cert1, der_cert2);

  EXPECT_NE(private_key_info1, private_key_info3);
  EXPECT_NE(der_cert1, der_cert3);

  EXPECT_NE(private_key_info2, private_key_info3);
  EXPECT_NE(der_cert2, der_cert3);

  EXPECT_EQ(3, service_->cert_count());
}

TEST_F(ChannelIDServiceTest, Expiration) {
  ChannelIDStore* store = service_->GetChannelIDStore();
  base::Time now = base::Time::Now();
  store->SetChannelID("good",
                      now,
                      now + base::TimeDelta::FromDays(1),
                      "a",
                      "b");
  store->SetChannelID("expired",
                      now - base::TimeDelta::FromDays(2),
                      now - base::TimeDelta::FromDays(1),
                      "c",
                      "d");
  EXPECT_EQ(2, service_->cert_count());

  int error;
  TestCompletionCallback callback;
  ChannelIDService::RequestHandle request_handle;

  // Cert is valid - synchronous completion.
  std::string private_key_info1, der_cert1;
  error = service_->GetOrCreateChannelID(
      "good", &private_key_info1, &der_cert1,
      callback.callback(), &request_handle);
  EXPECT_EQ(OK, error);
  EXPECT_FALSE(request_handle.is_active());
  EXPECT_EQ(2, service_->cert_count());
  EXPECT_STREQ("a", private_key_info1.c_str());
  EXPECT_STREQ("b", der_cert1.c_str());

  // Expired cert is valid as well - synchronous completion.
  std::string private_key_info2, der_cert2;
  error = service_->GetOrCreateChannelID(
      "expired", &private_key_info2, &der_cert2,
      callback.callback(), &request_handle);
  EXPECT_EQ(OK, error);
  EXPECT_FALSE(request_handle.is_active());
  EXPECT_EQ(2, service_->cert_count());
  EXPECT_STREQ("c", private_key_info2.c_str());
  EXPECT_STREQ("d", der_cert2.c_str());
}

TEST_F(ChannelIDServiceTest, AsyncStoreGetOrCreateNoChannelIDsInStore) {
  MockChannelIDStoreWithAsyncGet* mock_store =
      new MockChannelIDStoreWithAsyncGet();
  service_ = scoped_ptr<ChannelIDService>(new ChannelIDService(
      mock_store, base::MessageLoopProxy::current()));

  std::string host("encrypted.google.com");

  int error;
  TestCompletionCallback callback;
  ChannelIDService::RequestHandle request_handle;

  // Asynchronous completion with no certs in the store.
  std::string private_key_info, der_cert;
  EXPECT_EQ(0, service_->cert_count());
  error = service_->GetOrCreateChannelID(
      host, &private_key_info, &der_cert, callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());

  mock_store->CallGetChannelIDCallbackWithResult(
      ERR_FILE_NOT_FOUND, base::Time(), std::string(), std::string());

  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service_->cert_count());
  EXPECT_FALSE(private_key_info.empty());
  EXPECT_FALSE(der_cert.empty());
  EXPECT_FALSE(request_handle.is_active());
}

TEST_F(ChannelIDServiceTest, AsyncStoreGetNoChannelIDsInStore) {
  MockChannelIDStoreWithAsyncGet* mock_store =
      new MockChannelIDStoreWithAsyncGet();
  service_ = scoped_ptr<ChannelIDService>(new ChannelIDService(
      mock_store, base::MessageLoopProxy::current()));

  std::string host("encrypted.google.com");

  int error;
  TestCompletionCallback callback;
  ChannelIDService::RequestHandle request_handle;

  // Asynchronous completion with no certs in the store.
  std::string private_key, der_cert;
  EXPECT_EQ(0, service_->cert_count());
  error = service_->GetChannelID(
      host, &private_key, &der_cert, callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());

  mock_store->CallGetChannelIDCallbackWithResult(
      ERR_FILE_NOT_FOUND, base::Time(), std::string(), std::string());

  error = callback.WaitForResult();
  EXPECT_EQ(ERR_FILE_NOT_FOUND, error);
  EXPECT_EQ(0, service_->cert_count());
  EXPECT_EQ(0u, service_->workers_created());
  EXPECT_TRUE(der_cert.empty());
  EXPECT_FALSE(request_handle.is_active());
}

TEST_F(ChannelIDServiceTest, AsyncStoreGetOrCreateOneCertInStore) {
  MockChannelIDStoreWithAsyncGet* mock_store =
      new MockChannelIDStoreWithAsyncGet();
  service_ = scoped_ptr<ChannelIDService>(new ChannelIDService(
      mock_store, base::MessageLoopProxy::current()));

  std::string host("encrypted.google.com");

  int error;
  TestCompletionCallback callback;
  ChannelIDService::RequestHandle request_handle;

  // Asynchronous completion with a cert in the store.
  std::string private_key_info, der_cert;
  EXPECT_EQ(0, service_->cert_count());
  error = service_->GetOrCreateChannelID(
      host, &private_key_info, &der_cert, callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());

  mock_store->CallGetChannelIDCallbackWithResult(
      OK, base::Time(), "ab", "cd");

  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service_->cert_count());
  EXPECT_EQ(1u, service_->requests());
  EXPECT_EQ(1u, service_->cert_store_hits());
  // Because the cert was found in the store, no new workers should have been
  // created.
  EXPECT_EQ(0u, service_->workers_created());
  EXPECT_STREQ("ab", private_key_info.c_str());
  EXPECT_STREQ("cd", der_cert.c_str());
  EXPECT_FALSE(request_handle.is_active());
}

TEST_F(ChannelIDServiceTest, AsyncStoreGetOneCertInStore) {
  MockChannelIDStoreWithAsyncGet* mock_store =
      new MockChannelIDStoreWithAsyncGet();
  service_ = scoped_ptr<ChannelIDService>(new ChannelIDService(
      mock_store, base::MessageLoopProxy::current()));

  std::string host("encrypted.google.com");

  int error;
  TestCompletionCallback callback;
  ChannelIDService::RequestHandle request_handle;

  // Asynchronous completion with a cert in the store.
  std::string private_key, der_cert;
  EXPECT_EQ(0, service_->cert_count());
  error = service_->GetChannelID(
      host, &private_key, &der_cert, callback.callback(), &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle.is_active());

  mock_store->CallGetChannelIDCallbackWithResult(
      OK, base::Time(), "ab", "cd");

  error = callback.WaitForResult();
  EXPECT_EQ(OK, error);
  EXPECT_EQ(1, service_->cert_count());
  EXPECT_EQ(1u, service_->requests());
  EXPECT_EQ(1u, service_->cert_store_hits());
  // Because the cert was found in the store, no new workers should have been
  // created.
  EXPECT_EQ(0u, service_->workers_created());
  EXPECT_STREQ("cd", der_cert.c_str());
  EXPECT_FALSE(request_handle.is_active());
}

TEST_F(ChannelIDServiceTest, AsyncStoreGetThenCreateNoCertsInStore) {
  MockChannelIDStoreWithAsyncGet* mock_store =
      new MockChannelIDStoreWithAsyncGet();
  service_ = scoped_ptr<ChannelIDService>(new ChannelIDService(
      mock_store, base::MessageLoopProxy::current()));

  std::string host("encrypted.google.com");

  int error;

  // Asynchronous get with no certs in the store.
  TestCompletionCallback callback1;
  ChannelIDService::RequestHandle request_handle1;
  std::string private_key1, der_cert1;
  EXPECT_EQ(0, service_->cert_count());
  error = service_->GetChannelID(
      host, &private_key1, &der_cert1, callback1.callback(), &request_handle1);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle1.is_active());

  // Asynchronous get/create with no certs in the store.
  TestCompletionCallback callback2;
  ChannelIDService::RequestHandle request_handle2;
  std::string private_key2, der_cert2;
  EXPECT_EQ(0, service_->cert_count());
  error = service_->GetOrCreateChannelID(
      host, &private_key2, &der_cert2, callback2.callback(), &request_handle2);
  EXPECT_EQ(ERR_IO_PENDING, error);
  EXPECT_TRUE(request_handle2.is_active());

  mock_store->CallGetChannelIDCallbackWithResult(
      ERR_FILE_NOT_FOUND, base::Time(), std::string(), std::string());

  // Even though the first request didn't ask to create a cert, it gets joined
  // by the second, which does, so both succeed.
  error = callback1.WaitForResult();
  EXPECT_EQ(OK, error);
  error = callback2.WaitForResult();
  EXPECT_EQ(OK, error);

  // One cert is created, one request is joined.
  EXPECT_EQ(2U, service_->requests());
  EXPECT_EQ(1, service_->cert_count());
  EXPECT_EQ(1u, service_->workers_created());
  EXPECT_EQ(1u, service_->inflight_joins());
  EXPECT_FALSE(der_cert1.empty());
  EXPECT_EQ(der_cert1, der_cert2);
  EXPECT_FALSE(private_key1.empty());
  EXPECT_EQ(private_key1, private_key2);
  EXPECT_FALSE(request_handle1.is_active());
  EXPECT_FALSE(request_handle2.is_active());
}

}  // namespace

}  // namespace net
