// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/renderer/audio_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockAudioDelegate : public AudioMessageFilter::Delegate {
 public:
  MockAudioDelegate() {
    Reset();
  }

  virtual void OnRequestPacket(uint32 bytes_in_buffer,
                               const base::Time& message_timestamp) {
    request_packet_received_ = true;
    bytes_in_buffer_ = bytes_in_buffer;
    message_timestamp_ = message_timestamp;
  }

  virtual void OnStateChanged(const ViewMsg_AudioStreamState_Params& state) {
    state_changed_received_ = true;
    state_ = state;
  }

  virtual void OnCreated(base::SharedMemoryHandle handle, uint32 length) {
    created_received_ = true;
    handle_ = handle;
    length_ = length;
  }

  virtual void OnLowLatencyCreated(base::SharedMemoryHandle handle,
                                   base::SyncSocket::Handle,
                                   uint32 length) {
  }

  virtual void OnVolume(double volume) {
    volume_received_ = true;
    volume_ = volume;
  }

  void Reset() {
    request_packet_received_ = false;
    bytes_in_buffer_ = 0;
    message_timestamp_ = base::Time();

    state_changed_received_ = false;
    state_.state = ViewMsg_AudioStreamState_Params::kError;

    created_received_ = false;
    handle_ = base::SharedMemory::NULLHandle();
    length_ = 0;

    volume_received_ = false;
    volume_ = 0;
  }

  bool request_packet_received() { return request_packet_received_; }
  uint32 bytes_in_buffer() { return bytes_in_buffer_; }
  const base::Time& message_timestamp() { return message_timestamp_; }

  bool state_changed_received() { return state_changed_received_; }
  ViewMsg_AudioStreamState_Params state() { return state_; }

  bool created_received() { return created_received_; }
  base::SharedMemoryHandle handle() { return handle_; }
  uint32 length() { return length_; }

  bool volume_received() { return volume_received_; }
  double volume() { return volume_; }

 private:
  bool request_packet_received_;
  uint32 bytes_in_buffer_;
  base::Time message_timestamp_;

  bool state_changed_received_;
  ViewMsg_AudioStreamState_Params state_;

  bool created_received_;
  base::SharedMemoryHandle handle_;
  uint32 length_;

  bool volume_received_;
  double volume_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioDelegate);
};

}  // namespace

TEST(AudioMessageFilterTest, Basic) {
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  const int kRouteId = 0;
  scoped_refptr<AudioMessageFilter> filter = new AudioMessageFilter(kRouteId);

  MockAudioDelegate delegate;
  int stream_id = filter->AddDelegate(&delegate);

  // ViewMsg_RequestAudioPacket
  const uint32 kSizeInBuffer = 1024;
  const int64 kMessageTimestamp = 99;
  EXPECT_FALSE(delegate.request_packet_received());
  filter->OnMessageReceived(ViewMsg_RequestAudioPacket(kRouteId,
                                                       stream_id,
                                                       kSizeInBuffer,
                                                       kMessageTimestamp));
  EXPECT_TRUE(delegate.request_packet_received());
  EXPECT_EQ(kSizeInBuffer, delegate.bytes_in_buffer());
  EXPECT_EQ(kMessageTimestamp, delegate.message_timestamp().ToInternalValue());
  delegate.Reset();

  // ViewMsg_NotifyAudioStreamStateChanged
  const ViewMsg_AudioStreamState_Params kState(
      ViewMsg_AudioStreamState_Params::kPlaying);
  EXPECT_FALSE(delegate.state_changed_received());
  filter->OnMessageReceived(
      ViewMsg_NotifyAudioStreamStateChanged(kRouteId, stream_id, kState));
  EXPECT_TRUE(delegate.state_changed_received());
  EXPECT_TRUE(kState.state == delegate.state().state);
  delegate.Reset();

  // ViewMsg_NotifyAudioStreamCreated
  const uint32 kLength = 1024;
  EXPECT_FALSE(delegate.created_received());
  filter->OnMessageReceived(
      ViewMsg_NotifyAudioStreamCreated(kRouteId,
                                       stream_id,
                                       base::SharedMemory::NULLHandle(),
                                       kLength));
  EXPECT_TRUE(delegate.created_received());
  EXPECT_FALSE(base::SharedMemory::IsHandleValid(delegate.handle()));
  EXPECT_EQ(kLength, delegate.length());
  delegate.Reset();

  // ViewMsg_NotifyAudioStreamVolume
  const double kVolume = 1.0;
  EXPECT_FALSE(delegate.volume_received());
  filter->OnMessageReceived(
      ViewMsg_NotifyAudioStreamVolume(kRouteId, stream_id, kVolume));
  EXPECT_TRUE(delegate.volume_received());
  EXPECT_EQ(kVolume, delegate.volume());
  delegate.Reset();

  message_loop.RunAllPending();
}

TEST(AudioMessageFilterTest, Delegates) {
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  const int kRouteId = 0;
  scoped_refptr<AudioMessageFilter> filter = new AudioMessageFilter(kRouteId);

  MockAudioDelegate delegate1;
  MockAudioDelegate delegate2;

  int stream_id1 = filter->AddDelegate(&delegate1);
  int stream_id2 = filter->AddDelegate(&delegate2);

  // Send an IPC message. Make sure the correct delegate gets called.
  EXPECT_FALSE(delegate1.request_packet_received());
  EXPECT_FALSE(delegate2.request_packet_received());
  filter->OnMessageReceived(
      ViewMsg_RequestAudioPacket(kRouteId, stream_id1, 0, 0));
  EXPECT_TRUE(delegate1.request_packet_received());
  EXPECT_FALSE(delegate2.request_packet_received());
  delegate1.Reset();

  EXPECT_FALSE(delegate1.request_packet_received());
  EXPECT_FALSE(delegate2.request_packet_received());
  filter->OnMessageReceived(
      ViewMsg_RequestAudioPacket(kRouteId, stream_id2, 0, 0));
  EXPECT_FALSE(delegate1.request_packet_received());
  EXPECT_TRUE(delegate2.request_packet_received());
  delegate2.Reset();

  // Send a message of a different route id, a message is not received.
  EXPECT_FALSE(delegate1.request_packet_received());
  filter->OnMessageReceived(
      ViewMsg_RequestAudioPacket(kRouteId + 1, stream_id1, 0, 0));
  EXPECT_FALSE(delegate1.request_packet_received());

  // Remove the delegates. Make sure they won't get called.
  filter->RemoveDelegate(stream_id1);
  EXPECT_FALSE(delegate1.request_packet_received());
  filter->OnMessageReceived(
      ViewMsg_RequestAudioPacket(kRouteId, stream_id1, 0, 0));
  EXPECT_FALSE(delegate1.request_packet_received());

  filter->RemoveDelegate(stream_id2);
  EXPECT_FALSE(delegate2.request_packet_received());
  filter->OnMessageReceived(
      ViewMsg_RequestAudioPacket(kRouteId, stream_id2, 0, 0));
  EXPECT_FALSE(delegate2.request_packet_received());

  message_loop.RunAllPending();
}
