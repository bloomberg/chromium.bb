// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <limits>
#include <set>

#include "ipc/attachment_broker.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_channel_reader.h"
#include "ipc/placeholder_brokerable_attachment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {
namespace internal {

namespace {

#if USE_ATTACHMENT_BROKER

class MockAttachment : public BrokerableAttachment {
 public:
  MockAttachment() {}
  MockAttachment(BrokerableAttachment::AttachmentId id)
      : BrokerableAttachment(id) {}

#if defined(OS_POSIX)
  base::PlatformFile TakePlatformFile() override {
    return base::PlatformFile();
  }
#endif  // OS_POSIX

  BrokerableType GetBrokerableType() const override { return WIN_HANDLE; }

 private:
  ~MockAttachment() override {}
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

#endif  // USE_ATTACHMENT_BROKER

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

  bool IsAttachmentBrokerEndpoint() override { return false; }

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

class ExposedMessage: public Message {
 public:
  using Message::Header;
  using Message::header;
};

}  // namespace

#if USE_ATTACHMENT_BROKER

TEST(ChannelReaderTest, AttachmentAlreadyBrokered) {
  MockAttachmentBroker broker;
  MockChannelReader reader;
  reader.set_broker(&broker);
  scoped_refptr<MockAttachment> attachment(new MockAttachment);
  broker.AddAttachment(attachment);

  Message* m = new Message;
  PlaceholderBrokerableAttachment* needs_brokering_attachment =
      new PlaceholderBrokerableAttachment(attachment->GetIdentifier());
  EXPECT_TRUE(m->WriteAttachment(needs_brokering_attachment));
  reader.AddMessageForDispatch(m);
  EXPECT_EQ(ChannelReader::DISPATCH_FINISHED, reader.DispatchMessages());
  EXPECT_EQ(m, reader.get_last_dispatched_message());
}

TEST(ChannelReaderTest, AttachmentNotYetBrokered) {
  MockAttachmentBroker broker;
  MockChannelReader reader;
  reader.set_broker(&broker);
  scoped_refptr<MockAttachment> attachment(new MockAttachment);

  Message* m = new Message;
  PlaceholderBrokerableAttachment* needs_brokering_attachment =
      new PlaceholderBrokerableAttachment(attachment->GetIdentifier());
  EXPECT_TRUE(m->WriteAttachment(needs_brokering_attachment));
  reader.AddMessageForDispatch(m);
  EXPECT_EQ(ChannelReader::DISPATCH_WAITING_ON_BROKER,
            reader.DispatchMessages());
  EXPECT_EQ(nullptr, reader.get_last_dispatched_message());

  broker.AddAttachment(attachment);
  EXPECT_EQ(m, reader.get_last_dispatched_message());
}

#endif  // USE_ATTACHMENT_BROKER

#if !USE_ATTACHMENT_BROKER

// We can determine message size from its header (and hence resize the buffer)
// only when attachment broker is not used, see IPC::Message::FindNext().

TEST(ChannelReaderTest, ResizeOverflowBuffer) {
  MockChannelReader reader;

  ExposedMessage::Header header = {};

  header.payload_size = 128 * 1024;
  EXPECT_LT(reader.input_overflow_buf_.capacity(), header.payload_size);
  EXPECT_TRUE(reader.TranslateInputData(
      reinterpret_cast<const char*>(&header), sizeof(header)));

  // Once message header is available we resize overflow buffer to
  // fit the entire message.
  EXPECT_GE(reader.input_overflow_buf_.capacity(), header.payload_size);
}

TEST(ChannelReaderTest, InvalidMessageSize) {
  MockChannelReader reader;

  ExposedMessage::Header header = {};

  size_t capacity_before = reader.input_overflow_buf_.capacity();

  // Message is slightly larger than maximum allowed size
  header.payload_size = Channel::kMaximumMessageSize + 1;
  EXPECT_FALSE(reader.TranslateInputData(
      reinterpret_cast<const char*>(&header), sizeof(header)));
  EXPECT_LE(reader.input_overflow_buf_.capacity(), capacity_before);

  // Payload size is negative, overflow is detected by Pickle::PeekNext()
  header.payload_size = static_cast<uint32_t>(-1);
  EXPECT_FALSE(reader.TranslateInputData(
      reinterpret_cast<const char*>(&header), sizeof(header)));
  EXPECT_LE(reader.input_overflow_buf_.capacity(), capacity_before);

  // Payload size is maximum int32 value
  header.payload_size = std::numeric_limits<int32_t>::max();
  EXPECT_FALSE(reader.TranslateInputData(
      reinterpret_cast<const char*>(&header), sizeof(header)));
  EXPECT_LE(reader.input_overflow_buf_.capacity(), capacity_before);
}

#endif  // !USE_ATTACHMENT_BROKER

}  // namespace internal
}  // namespace IPC
