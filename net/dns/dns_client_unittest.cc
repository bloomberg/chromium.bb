// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_client.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/big_endian.h"
#include "net/base/net_log.h"
#include "net/base/sys_addrinfo.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_test_util.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(szym): test DnsClient::Request::Start with synchronous failure
// TODO(szym): test suffix search and server fallback once implemented

namespace net {

namespace {

class DnsClientTest : public testing::Test {
 public:
  class TestRequestHelper {
   public:
    // If |answer_count| < 0, it is the expected error code.
    TestRequestHelper(const char* name,
                      uint16 type,
                      const MockWrite& write,
                      const MockRead& read,
                      int answer_count) {
      // Must include the terminating \x00.
      qname = std::string(name, strlen(name) + 1);
      qtype = type;
      expected_answer_count = answer_count;
      completed = false;
      writes.push_back(write);
      reads.push_back(read);
      ReadBigEndian<uint16>(write.data, &transaction_id);
      data.reset(new StaticSocketDataProvider(&reads[0], reads.size(),
                                              &writes[0], writes.size()));
    }

    void MakeRequest(DnsClient* client) {
      EXPECT_EQ(NULL, request.get());
      request.reset(client->CreateRequest(
          qname,
          qtype,
          base::Bind(&TestRequestHelper::OnRequestComplete,
                     base::Unretained(this)),
          BoundNetLog()));
      EXPECT_EQ(qname, request->qname());
      EXPECT_EQ(qtype, request->qtype());
      EXPECT_EQ(ERR_IO_PENDING, request->Start());
    }

    void Cancel() {
      ASSERT_TRUE(request.get() != NULL);
      request.reset(NULL);
    }

    void OnRequestComplete(DnsClient::Request* req,
                           int rv,
                           const DnsResponse* response) {
      EXPECT_FALSE(completed);
      EXPECT_EQ(request.get(), req);

      if (expected_answer_count >= 0) {
        EXPECT_EQ(OK, rv);
        EXPECT_EQ(expected_answer_count, response->answer_count());

        DnsRecordParser parser = response->Parser();
        DnsResourceRecord record;
        for (int i = 0; i < expected_answer_count; ++i) {
          EXPECT_TRUE(parser.ParseRecord(&record));
        }
        EXPECT_TRUE(parser.AtEnd());

      } else {
        EXPECT_EQ(expected_answer_count, rv);
        EXPECT_EQ(NULL, response);
      }

      completed = true;
    }

    void CancelOnRequestComplete(DnsClient::Request* req,
                                 int rv,
                                 const DnsResponse* response) {
      EXPECT_FALSE(completed);
      Cancel();
    }

    std::string qname;
    uint16 qtype;
    std::vector<MockWrite> writes;
    std::vector<MockRead> reads;
    uint16 transaction_id;  // Id from first write.
    scoped_ptr<StaticSocketDataProvider> data;
    scoped_ptr<DnsClient::Request> request;
    int expected_answer_count;

    bool completed;
  };

  virtual void SetUp() OVERRIDE {
    helpers_.push_back(new TestRequestHelper(
        kT0DnsName,
        kT0Qtype,
        MockWrite(true, reinterpret_cast<const char*>(kT0QueryDatagram),
                  arraysize(kT0QueryDatagram)),
        MockRead(true, reinterpret_cast<const char*>(kT0ResponseDatagram),
                  arraysize(kT0ResponseDatagram)),
        arraysize(kT0IpAddresses) + 1));  // +1 for CNAME RR

    helpers_.push_back(new TestRequestHelper(
        kT1DnsName,
        kT1Qtype,
        MockWrite(true, reinterpret_cast<const char*>(kT1QueryDatagram),
                  arraysize(kT1QueryDatagram)),
        MockRead(true, reinterpret_cast<const char*>(kT1ResponseDatagram),
                  arraysize(kT1ResponseDatagram)),
        arraysize(kT1IpAddresses) + 1));  // +1 for CNAME RR

    helpers_.push_back(new TestRequestHelper(
        kT2DnsName,
        kT2Qtype,
        MockWrite(true, reinterpret_cast<const char*>(kT2QueryDatagram),
                  arraysize(kT2QueryDatagram)),
        MockRead(true, reinterpret_cast<const char*>(kT2ResponseDatagram),
                  arraysize(kT2ResponseDatagram)),
        arraysize(kT2IpAddresses) + 1));  // +1 for CNAME RR

    helpers_.push_back(new TestRequestHelper(
        kT3DnsName,
        kT3Qtype,
        MockWrite(true, reinterpret_cast<const char*>(kT3QueryDatagram),
                  arraysize(kT3QueryDatagram)),
        MockRead(true, reinterpret_cast<const char*>(kT3ResponseDatagram),
                  arraysize(kT3ResponseDatagram)),
        arraysize(kT3IpAddresses) + 2));  // +2 for CNAME RR

    CreateClient();
  }

  void CreateClient() {
    MockClientSocketFactory* factory = new MockClientSocketFactory();

    transaction_ids_.clear();
    for (unsigned i = 0; i < helpers_.size(); ++i) {
      factory->AddSocketDataProvider(helpers_[i]->data.get());
      transaction_ids_.push_back(static_cast<int>(helpers_[i]->transaction_id));
    }

    DnsConfig config;

    IPEndPoint dns_server;
    {
      bool rv = CreateDnsAddress(kDnsIp, kDnsPort, &dns_server);
      EXPECT_TRUE(rv);
    }
    config.nameservers.push_back(dns_server);

    DnsSession* session = new DnsSession(
        config,
        factory,
        base::Bind(&DnsClientTest::GetNextId, base::Unretained(this)),
        NULL /* NetLog */);

    client_.reset(DnsClient::CreateClient(session));
  }

  virtual void TearDown() OVERRIDE {
    STLDeleteElements(&helpers_);
  }

  int GetNextId(int min, int max) {
    EXPECT_FALSE(transaction_ids_.empty());
    int id = transaction_ids_.front();
    transaction_ids_.pop_front();
    EXPECT_GE(id, min);
    EXPECT_LE(id, max);
    return id;
  }

 protected:
  std::vector<TestRequestHelper*> helpers_;
  std::deque<int> transaction_ids_;
  scoped_ptr<DnsClient> client_;
};

TEST_F(DnsClientTest, Lookup) {
  helpers_[0]->MakeRequest(client_.get());

  // Wait until result.
  MessageLoop::current()->RunAllPending();

  EXPECT_TRUE(helpers_[0]->completed);
}

TEST_F(DnsClientTest, ConcurrentLookup) {
  for (unsigned i = 0; i < helpers_.size(); ++i) {
    helpers_[i]->MakeRequest(client_.get());
  }

  MessageLoop::current()->RunAllPending();

  for (unsigned i = 0; i < helpers_.size(); ++i) {
    EXPECT_TRUE(helpers_[i]->completed);
  }
}

TEST_F(DnsClientTest, CancelLookup) {
  for (unsigned i = 0; i < helpers_.size(); ++i) {
    helpers_[i]->MakeRequest(client_.get());
  }

  helpers_[0]->Cancel();
  helpers_[2]->Cancel();

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(helpers_[0]->completed);
  EXPECT_TRUE(helpers_[1]->completed);
  EXPECT_FALSE(helpers_[2]->completed);
  EXPECT_TRUE(helpers_[3]->completed);
}

TEST_F(DnsClientTest, DestroyClient) {
  for (unsigned i = 0; i < helpers_.size(); ++i) {
    helpers_[i]->MakeRequest(client_.get());
  }

  // Destroying the client does not affect running requests.
  client_.reset(NULL);

  MessageLoop::current()->RunAllPending();

  for (unsigned i = 0; i < helpers_.size(); ++i) {
    EXPECT_TRUE(helpers_[i]->completed);
  }
}

TEST_F(DnsClientTest, DestroyRequestFromCallback) {
  // Custom callback to cancel the completing request.
  helpers_[0]->request.reset(client_->CreateRequest(
      helpers_[0]->qname,
      helpers_[0]->qtype,
      base::Bind(&TestRequestHelper::CancelOnRequestComplete,
                 base::Unretained(helpers_[0])),
      BoundNetLog()));
  helpers_[0]->request->Start();

  for (unsigned i = 1; i < helpers_.size(); ++i) {
    helpers_[i]->MakeRequest(client_.get());
  }

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(helpers_[0]->completed);
  for (unsigned i = 1; i < helpers_.size(); ++i) {
    EXPECT_TRUE(helpers_[i]->completed);
  }
}

TEST_F(DnsClientTest, HandleFailure) {
  STLDeleteElements(&helpers_);
  // Wrong question.
  helpers_.push_back(new TestRequestHelper(
      kT0DnsName,
      kT0Qtype,
      MockWrite(true, reinterpret_cast<const char*>(kT0QueryDatagram),
                arraysize(kT0QueryDatagram)),
      MockRead(true, reinterpret_cast<const char*>(kT1ResponseDatagram),
                arraysize(kT1ResponseDatagram)),
      ERR_DNS_MALFORMED_RESPONSE));

  // Response with NXDOMAIN.
  uint8 nxdomain_response[arraysize(kT0QueryDatagram)];
  memcpy(nxdomain_response, kT0QueryDatagram, arraysize(nxdomain_response));
  nxdomain_response[2] &= 0x80;  // Response bit.
  nxdomain_response[3] &= 0x03;  // NXDOMAIN bit.
  helpers_.push_back(new TestRequestHelper(
      kT0DnsName,
      kT0Qtype,
      MockWrite(true, reinterpret_cast<const char*>(kT0QueryDatagram),
                arraysize(kT0QueryDatagram)),
      MockRead(true, reinterpret_cast<const char*>(nxdomain_response),
                arraysize(nxdomain_response)),
      ERR_NAME_NOT_RESOLVED));

  CreateClient();

  for (unsigned i = 0; i < helpers_.size(); ++i) {
    helpers_[i]->MakeRequest(client_.get());
  }

  MessageLoop::current()->RunAllPending();

  for (unsigned i = 0; i < helpers_.size(); ++i) {
    EXPECT_TRUE(helpers_[i]->completed);
  }
}

}  // namespace

}  // namespace net

