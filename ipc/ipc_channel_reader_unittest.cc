// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <set>

#include "ipc/attachment_broker.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_channel_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

#if USE_ATTACHMENT_BROKER
namespace IPC {
namespace internal {

namespace {

class MockAttachment : public BrokerableAttachment {
 public:
  MockAttachment(int internal_state) : internal_state_(internal_state) {}
  MockAttachment(BrokerableAttachment::AttachmentId id)
      : BrokerableAttachment(id, true), internal_state_(-1) {}

  void PopulateWithAttachment(const BrokerableAttachment* attachment) override {
    const MockAttachment* mock_attachment =
        static_cast<const MockAttachment*>(attachment);
    internal_state_ = mock_attachment->internal_state_;
  }

#if defined(OS_POSIX)
  base::PlatformFile TakePlatformFile() override {
    return base::PlatformFile();
  }
#endif  // OS_POSIX

  BrokerableType GetBrokerableType() const override { return WIN_HANDLE; }

 private:
  ~MockAttachment() override {}
  // Internal state differentiates MockAttachments.
  int internal_state_;
};

class MockAttachmentBroker : public AttachmentBroker {
 public:
  typedef std::set<scoped_refptr<BrokerableAttachment>> AttachmentSet;

  bool SendAttachmentToProcess(const BrokerableAttachment* attachment,
                               base::ProcessId destination_process) override {
    return false;
  }

  bool OnMessageReceived(const Message& message) override { return false; }

  void AddAttachment(scoped_refptr<BrokerableAttachment> attachment) {
    get_attachments()->push_back(attachment);
    NotifyObservers(attachment->GetIdentifier());
  }
};

class MockChannelReader : public ChannelReader {
 public:
  MockChannelReader()
      : ChannelReader(nullptr), last_dispatched_message_(nullptr) {}

  ReadState ReadData(char* buffer, int buffer_len, int* bytes_read) override {
    return READ_FAILED;
  }

  bool ShouldDispatchInputMessage(Message* msg) override { return true; }

  bool GetNonBrokeredAttachments(Message* msg) override { return true; }

  bool DidEmptyInputBuffers() override { return true; }

  void HandleInternalMessage(const Message& msg) override {}

  void DispatchMessage(Message* m) override { last_dispatched_message_ = m; }

  base::ProcessId GetSenderPID() override { return base::kNullProcessId; }

  AttachmentBroker* GetAttachmentBroker() override { return broker_; }

  // This instance takes ownership of |m|.
  void AddMessageForDispatch(Message* m) {
    get_queued_messages()->push_back(m);
  }

  Message* get_last_dispatched_message() { return last_dispatched_message_; }

  void set_broker(AttachmentBroker* broker) { broker_ = broker; }

 private:
  Message* last_dispatched_message_;
  AttachmentBroker* broker_;
};

}  // namespace

TEST(ChannelReaderTest, AttachmentAlreadyBrokered) {
  MockAttachmentBroker broker;
  MockChannelReader reader;
  reader.set_broker(&broker);
  scoped_refptr<MockAttachment> attachment(new MockAttachment(5));
  broker.AddAttachment(attachment);

  Message* m = new Message;
  MockAttachment* needs_brokering_attachment =
      new MockAttachment(attachment->GetIdentifier());
  EXPECT_TRUE(m->WriteAttachment(needs_brokering_attachment));
  reader.AddMessageForDispatch(m);
  EXPECT_EQ(ChannelReader::DISPATCH_FINISHED, reader.DispatchMessages());
  EXPECT_EQ(m, reader.get_last_dispatched_message());
}

TEST(ChannelReaderTest, AttachmentNotYetBrokered) {
  MockAttachmentBroker broker;
  MockChannelReader reader;
  reader.set_broker(&broker);
  scoped_refptr<MockAttachment> attachment(new MockAttachment(5));

  Message* m = new Message;
  MockAttachment* needs_brokering_attachment =
      new MockAttachment(attachment->GetIdentifier());
  EXPECT_TRUE(m->WriteAttachment(needs_brokering_attachment));
  reader.AddMessageForDispatch(m);
  EXPECT_EQ(ChannelReader::DISPATCH_WAITING_ON_BROKER,
            reader.DispatchMessages());
  EXPECT_EQ(nullptr, reader.get_last_dispatched_message());

  broker.AddAttachment(attachment);
  EXPECT_EQ(m, reader.get_last_dispatched_message());
}

}  // namespace internal
}  // namespace IPC
#endif  // USE_ATTACHMENT_BROKER
