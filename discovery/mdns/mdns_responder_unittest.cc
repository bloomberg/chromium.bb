// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_responder.h"

#include "discovery/mdns/mdns_probe_manager.h"
#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_receiver.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/mdns_sender.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/fake_udp_socket.h"

namespace openscreen {
namespace discovery {
namespace {

constexpr Clock::duration kMaximumSharedRecordResponseDelayMs(120 * 1000);

bool ContainsRecordType(const std::vector<MdnsRecord>& records, DnsType type) {
  return std::find_if(records.begin(), records.end(),
                      [type](const MdnsRecord& record) {
                        return record.dns_type() == type;
                      }) != records.end();
}

}  // namespace

using testing::_;
using testing::Args;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

class MockRecordHandler : public MdnsResponder::RecordHandler {
 public:
  void AddRecord(MdnsRecord record) { records_.push_back(record); }

  MOCK_METHOD3(HasRecords, bool(const DomainName&, DnsType, DnsClass));

  std::vector<MdnsRecord::ConstRef> GetRecords(const DomainName& name,
                                               DnsType type,
                                               DnsClass clazz) {
    std::vector<MdnsRecord::ConstRef> records;
    for (const auto& record : records_) {
      if (type == DnsType::kANY || record.dns_type() == type) {
        records.push_back(record);
      }
    }

    return records;
  }

 private:
  std::vector<MdnsRecord> records_;
};

class MockMdnsSender : public MdnsSender {
 public:
  MockMdnsSender(UdpSocket* socket) : MdnsSender(socket) {}

  MOCK_METHOD1(SendMulticast, Error(const MdnsMessage& message));
  MOCK_METHOD2(SendUnicast,
               Error(const MdnsMessage& message, const IPEndpoint& endpoint));
};

class MockProbeManager : public MdnsProbeManager {
 public:
  MOCK_CONST_METHOD1(IsDomainClaimed, bool(const DomainName&));
  MOCK_METHOD2(RespondToProbeQuery,
               void(const MdnsMessage&, const IPEndpoint&));
};

class MdnsResponderTest : public testing::Test {
 public:
  MdnsResponderTest()
      : socket_(FakeUdpSocket::CreateDefault()),
        sender_(socket_.get()),
        receiver_(socket_.get()),
        clock_(Clock::now()),
        task_runner_(&clock_),
        responder_(&record_handler_,
                   &probe_manager_,
                   &sender_,
                   &receiver_,
                   &task_runner_,
                   &random_) {}

 protected:
  MdnsRecord GetFakePtrRecord(const DomainName& target) {
    DomainName name(++target.labels().begin(), target.labels().end());
    PtrRecordRdata rdata(target);
    return MdnsRecord(std::move(name), DnsType::kPTR, DnsClass::kIN,
                      RecordType::kUnique, std::chrono::seconds(0), rdata);
  }

  MdnsRecord GetFakeSrvRecord(const DomainName& name) {
    SrvRecordRdata rdata(0, 0, 80, name);
    return MdnsRecord(name, DnsType::kSRV, DnsClass::kIN, RecordType::kUnique,
                      std::chrono::seconds(0), rdata);
  }

  MdnsRecord GetFakeTxtRecord(const DomainName& name) {
    TxtRecordRdata rdata;
    return MdnsRecord(name, DnsType::kTXT, DnsClass::kIN, RecordType::kUnique,
                      std::chrono::seconds(0), rdata);
  }

  MdnsRecord GetFakeARecord(const DomainName& name) {
    ARecordRdata rdata(IPAddress(192, 168, 0, 0));
    return MdnsRecord(name, DnsType::kA, DnsClass::kIN, RecordType::kUnique,
                      std::chrono::seconds(0), rdata);
  }

  MdnsRecord GetFakeAAAARecord(const DomainName& name) {
    AAAARecordRdata rdata(IPAddress(1, 2, 3, 4, 5, 6, 7, 8));
    return MdnsRecord(name, DnsType::kAAAA, DnsClass::kIN, RecordType::kUnique,
                      std::chrono::seconds(0), rdata);
  }

  void OnMessageReceived(const MdnsMessage& message, const IPEndpoint& src) {
    responder_.OnMessageReceived(message, src);
  }

  std::unique_ptr<FakeUdpSocket> socket_;
  StrictMock<MockRecordHandler> record_handler_;
  StrictMock<MockMdnsSender> sender_;
  StrictMock<MockProbeManager> probe_manager_;
  MdnsReceiver receiver_;
  FakeClock clock_;
  FakeTaskRunner task_runner_;
  MdnsRandom random_;
  MdnsResponder responder_;

  DomainName domain_{"instance", "_googlecast", "_tcp", "local"};
  IPEndpoint endpoint_{IPAddress(192, 168, 0, 0), 80};
};

// Validate that when records may be sent from multiple receivers, the broadcast
// is delayed and it is not delayed otherwise.
TEST_F(MdnsResponderTest, OwnedRecordsSentImmediately) {
  MdnsQuestion question(domain_, DnsType::kANY, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillOnce(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  EXPECT_CALL(sender_, SendMulticast(_)).Times(1);
  OnMessageReceived(message, endpoint_);

  EXPECT_CALL(sender_, SendMulticast(_)).Times(0);
  clock_.Advance(Clock::duration(kMaximumSharedRecordResponseDelayMs));
}

TEST_F(MdnsResponderTest, NonOwnedRecordsDelayed) {
  MdnsQuestion question(domain_, DnsType::kANY, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillOnce(Return(false));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  EXPECT_CALL(sender_, SendMulticast(_)).Times(0);
  OnMessageReceived(message, endpoint_);

  EXPECT_CALL(sender_, SendMulticast(_)).Times(1);
  clock_.Advance(Clock::duration(kMaximumSharedRecordResponseDelayMs));
}

TEST_F(MdnsResponderTest, MultipleQuestionsProcessed) {
  MdnsQuestion question(domain_, DnsType::kANY, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsQuestion question2(domain_, DnsType::kANY, DnsClass::kANY,
                         ResponseType::kMulticast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);
  message.AddQuestion(question2);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  EXPECT_CALL(sender_, SendMulticast(_)).Times(1);
  OnMessageReceived(message, endpoint_);

  EXPECT_CALL(sender_, SendMulticast(_)).Times(1);
  clock_.Advance(Clock::duration(kMaximumSharedRecordResponseDelayMs));
}

TEST_F(MdnsResponderTest, NoRecordsNoMessageSent) {
  MdnsQuestion question(domain_, DnsType::kANY, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);

  EXPECT_CALL(sender_, SendMulticast(_)).Times(0);
  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillOnce(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  OnMessageReceived(message, endpoint_);

  clock_.Advance(Clock::duration(kMaximumSharedRecordResponseDelayMs));
}

// Validate that the correct messaging scheme (unicast vs multicast) is used.
TEST_F(MdnsResponderTest, UnicastMessageSentOverUnicast) {
  MdnsQuestion question(domain_, DnsType::kANY, DnsClass::kANY,
                        ResponseType::kUnicast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillOnce(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  EXPECT_CALL(sender_, SendUnicast(_, endpoint_)).Times(1);
  OnMessageReceived(message, endpoint_);
}

TEST_F(MdnsResponderTest, MulticastMessageSentOverMulticast) {
  MdnsQuestion question(domain_, DnsType::kANY, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillOnce(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  EXPECT_CALL(sender_, SendMulticast(_)).Times(1);
  OnMessageReceived(message, endpoint_);
}

// Validate that records are added as expected based on the query type, and that
// additional records are populated as specified in RFC 6762 and 6763.
TEST_F(MdnsResponderTest, AnyQueryResultsAllApplied) {
  MdnsQuestion question(domain_, DnsType::kANY, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillOnce(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  record_handler_.AddRecord(GetFakeTxtRecord(domain_));
  record_handler_.AddRecord(GetFakeARecord(domain_));
  record_handler_.AddRecord(GetFakeAAAARecord(domain_));
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([](const MdnsMessage& message) -> Error {
        EXPECT_EQ(message.questions().size(), size_t{0});
        EXPECT_EQ(message.authority_records().size(), size_t{0});
        EXPECT_EQ(message.additional_records().size(), size_t{0});

        EXPECT_EQ(message.answers().size(), size_t{4});
        EXPECT_TRUE(ContainsRecordType(message.answers(), DnsType::kSRV));
        EXPECT_TRUE(ContainsRecordType(message.answers(), DnsType::kTXT));
        EXPECT_TRUE(ContainsRecordType(message.answers(), DnsType::kA));
        EXPECT_TRUE(ContainsRecordType(message.answers(), DnsType::kAAAA));
        EXPECT_FALSE(ContainsRecordType(message.answers(), DnsType::kPTR));
        return Error::None();
      });

  OnMessageReceived(message, endpoint_);
}

TEST_F(MdnsResponderTest, PtrQueryResultsApplied) {
  DomainName ptr_domain{"_googlecast", "_tcp", "local"};
  MdnsQuestion question(ptr_domain, DnsType::kPTR, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillOnce(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakePtrRecord(domain_));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  record_handler_.AddRecord(GetFakeTxtRecord(domain_));
  record_handler_.AddRecord(GetFakeARecord(domain_));
  record_handler_.AddRecord(GetFakeAAAARecord(domain_));
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([](const MdnsMessage& message) -> Error {
        EXPECT_EQ(message.questions().size(), size_t{0});
        EXPECT_EQ(message.authority_records().size(), size_t{0});
        EXPECT_EQ(message.additional_records().size(), size_t{4});

        EXPECT_EQ(message.answers().size(), size_t{1});
        EXPECT_TRUE(ContainsRecordType(message.answers(), DnsType::kPTR));

        const auto& records = message.additional_records();
        EXPECT_EQ(records.size(), size_t{4});
        EXPECT_TRUE(ContainsRecordType(records, DnsType::kSRV));
        EXPECT_TRUE(ContainsRecordType(records, DnsType::kTXT));
        EXPECT_TRUE(ContainsRecordType(records, DnsType::kA));
        EXPECT_TRUE(ContainsRecordType(records, DnsType::kAAAA));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kPTR));

        return Error::None();
      });

  OnMessageReceived(message, endpoint_);
}

TEST_F(MdnsResponderTest, SrvQueryResultsApplied) {
  MdnsQuestion question(domain_, DnsType::kSRV, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillOnce(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakePtrRecord(domain_));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  record_handler_.AddRecord(GetFakeTxtRecord(domain_));
  record_handler_.AddRecord(GetFakeARecord(domain_));
  record_handler_.AddRecord(GetFakeAAAARecord(domain_));
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([](const MdnsMessage& message) -> Error {
        EXPECT_EQ(message.questions().size(), size_t{0});
        EXPECT_EQ(message.authority_records().size(), size_t{0});
        EXPECT_EQ(message.additional_records().size(), size_t{2});

        EXPECT_EQ(message.answers().size(), size_t{1});
        EXPECT_TRUE(ContainsRecordType(message.answers(), DnsType::kSRV));

        const auto& records = message.additional_records();
        EXPECT_EQ(records.size(), size_t{2});
        EXPECT_TRUE(ContainsRecordType(records, DnsType::kA));
        EXPECT_TRUE(ContainsRecordType(records, DnsType::kAAAA));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kSRV));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kTXT));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kPTR));

        return Error::None();
      });

  OnMessageReceived(message, endpoint_);
}

TEST_F(MdnsResponderTest, AQueryResultsApplied) {
  MdnsQuestion question(domain_, DnsType::kA, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillOnce(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakePtrRecord(domain_));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  record_handler_.AddRecord(GetFakeTxtRecord(domain_));
  record_handler_.AddRecord(GetFakeARecord(domain_));
  record_handler_.AddRecord(GetFakeAAAARecord(domain_));
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([](const MdnsMessage& message) -> Error {
        EXPECT_EQ(message.questions().size(), size_t{0});
        EXPECT_EQ(message.authority_records().size(), size_t{0});
        EXPECT_EQ(message.additional_records().size(), size_t{1});

        EXPECT_EQ(message.answers().size(), size_t{1});
        EXPECT_TRUE(ContainsRecordType(message.answers(), DnsType::kA));

        const auto& records = message.additional_records();
        EXPECT_EQ(records.size(), size_t{1});
        EXPECT_TRUE(ContainsRecordType(records, DnsType::kAAAA));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kA));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kSRV));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kTXT));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kPTR));

        return Error::None();
      });

  OnMessageReceived(message, endpoint_);
}

TEST_F(MdnsResponderTest, AAAAQueryResultsApplied) {
  MdnsQuestion question(domain_, DnsType::kAAAA, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillOnce(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakePtrRecord(domain_));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  record_handler_.AddRecord(GetFakeTxtRecord(domain_));
  record_handler_.AddRecord(GetFakeARecord(domain_));
  record_handler_.AddRecord(GetFakeAAAARecord(domain_));
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([](const MdnsMessage& message) -> Error {
        EXPECT_EQ(message.questions().size(), size_t{0});
        EXPECT_EQ(message.authority_records().size(), size_t{0});
        EXPECT_EQ(message.additional_records().size(), size_t{1});

        EXPECT_EQ(message.answers().size(), size_t{1});
        EXPECT_TRUE(ContainsRecordType(message.answers(), DnsType::kAAAA));

        const auto& records = message.additional_records();
        EXPECT_EQ(records.size(), size_t{1});
        EXPECT_TRUE(ContainsRecordType(records, DnsType::kA));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kAAAA));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kSRV));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kTXT));
        EXPECT_FALSE(ContainsRecordType(records, DnsType::kPTR));

        return Error::None();
      });

  OnMessageReceived(message, endpoint_);
}

TEST_F(MdnsResponderTest, MessageOnlySentIfAnswerNotKnown) {
  MdnsQuestion question(domain_, DnsType::kAAAA, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsRecord aaaa_record = GetFakeAAAARecord(domain_);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);
  message.AddAnswer(aaaa_record);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakePtrRecord(domain_));
  record_handler_.AddRecord(GetFakeSrvRecord(domain_));
  record_handler_.AddRecord(GetFakeTxtRecord(domain_));
  record_handler_.AddRecord(GetFakeARecord(domain_));
  record_handler_.AddRecord(aaaa_record);

  OnMessageReceived(message, endpoint_);
}

TEST_F(MdnsResponderTest, RecordOnlySentIfNotKnown) {
  MdnsQuestion question(domain_, DnsType::kANY, DnsClass::kANY,
                        ResponseType::kMulticast);
  MdnsRecord aaaa_record = GetFakeAAAARecord(domain_);
  MdnsMessage message(0, MessageType::Query);
  message.AddQuestion(question);
  message.AddAnswer(aaaa_record);

  EXPECT_CALL(probe_manager_, IsDomainClaimed(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(record_handler_, HasRecords(_, _, _))
      .WillRepeatedly(Return(true));
  record_handler_.AddRecord(GetFakeARecord(domain_));
  record_handler_.AddRecord(aaaa_record);
  EXPECT_CALL(sender_, SendMulticast(_))
      .WillOnce([](const MdnsMessage& message) -> Error {
        EXPECT_EQ(message.questions().size(), size_t{0});
        EXPECT_EQ(message.authority_records().size(), size_t{0});
        EXPECT_EQ(message.additional_records().size(), size_t{0});

        EXPECT_EQ(message.answers().size(), size_t{1});
        EXPECT_TRUE(ContainsRecordType(message.answers(), DnsType::kA));
        return Error::None();
      });

  OnMessageReceived(message, endpoint_);
}

}  // namespace discovery
}  // namespace openscreen
