// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/audio_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockAudioDelegate : public AudioMessageFilter::Delegate {
 public:
  MockAudioDelegate() {
    Reset();
  }

  virtual void OnRequestPacket(AudioBuffersState buffers_state) {
    request_packet_received_ = true;
    buffers_state_ = buffers_state;
  }

  virtual void OnStateChanged(AudioStreamState state) {
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
    buffers_state_ = AudioBuffersState();
    buffers_state_.timestamp = base::Time();

    state_changed_received_ = false;
    state_ = kAudioStreamError;

    created_received_ = false;
    handle_ = base::SharedMemory::NULLHandle();
    length_ = 0;

    volume_received_ = false;
    volume_ = 0;
  }

  bool request_packet_received() { return request_packet_received_; }
  AudioBuffersState buffers_state() { return buffers_state_; }

  bool state_changed_received() { return state_changed_received_; }
  AudioStreamState state() { return state_; }

  bool created_received() { return created_received_; }
  base::SharedMemoryHandle handle() { return handle_; }
  uint32 length() { return length_; }

  bool volume_received() { return volume_received_; }
  double volume() { return volume_; }

 private:
  bool request_packet_received_;
  AudioBuffersState buffers_state_;

  bool state_changed_received_;
  AudioStreamState state_;

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
  scoped_refptr<AudioMessageFilter> filter(new AudioMessageFilter(kRouteId));

  MockAudioDelegate delegate;
  int stream_id = filter->AddDelegate(&delegate);

  // AudioMsg_RequestPacket
  const int kSizeInBuffer = 1024;
  AudioBuffersState buffers_state(kSizeInBuffer, 0);

  EXPECT_FALSE(delegate.request_packet_received());
  filter->OnMessageReceived(AudioMsg_RequestPacket(
      kRouteId, stream_id, buffers_state));
  EXPECT_TRUE(delegate.request_packet_received());
  EXPECT_EQ(kSizeInBuffer, delegate.buffers_state().pending_bytes);
  EXPECT_EQ(0, delegate.buffers_state().hardware_delay_bytes);
  EXPECT_TRUE(buffers_state.timestamp == delegate.buffers_state().timestamp);
  delegate.Reset();

  // AudioMsg_NotifyStreamStateChanged
  EXPECT_FALSE(delegate.state_changed_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamStateChanged(kRouteId, stream_id,
      kAudioStreamPlaying));
  EXPECT_TRUE(delegate.state_changed_received());
  EXPECT_TRUE(kAudioStreamPlaying == delegate.state());
  delegate.Reset();

  // AudioMsg_NotifyStreamCreated
  const uint32 kLength = 1024;
  EXPECT_FALSE(delegate.created_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamCreated(kRouteId,
                                   stream_id,
                                   base::SharedMemory::NULLHandle(),
                                   kLength));
  EXPECT_TRUE(delegate.created_received());
  EXPECT_FALSE(base::SharedMemory::IsHandleValid(delegate.handle()));
  EXPECT_EQ(kLength, delegate.length());
  delegate.Reset();

  // AudioMsg_NotifyStreamVolume
  const double kVolume = 1.0;
  EXPECT_FALSE(delegate.volume_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamVolume(kRouteId, stream_id, kVolume));
  EXPECT_TRUE(delegate.volume_received());
  EXPECT_EQ(kVolume, delegate.volume());
  delegate.Reset();

  message_loop.RunAllPending();
}

TEST(AudioMessageFilterTest, Delegates) {
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  const int kRouteId = 0;
  scoped_refptr<AudioMessageFilter> filter(new AudioMessageFilter(kRouteId));

  MockAudioDelegate delegate1;
  MockAudioDelegate delegate2;

  int stream_id1 = filter->AddDelegate(&delegate1);
  int stream_id2 = filter->AddDelegate(&delegate2);

  // Send an IPC message. Make sure the correct delegate gets called.
  EXPECT_FALSE(delegate1.request_packet_received());
  EXPECT_FALSE(delegate2.request_packet_received());
  filter->OnMessageReceived(
      AudioMsg_RequestPacket(kRouteId, stream_id1, AudioBuffersState()));
  EXPECT_TRUE(delegate1.request_packet_received());
  EXPECT_FALSE(delegate2.request_packet_received());
  delegate1.Reset();

  EXPECT_FALSE(delegate1.request_packet_received());
  EXPECT_FALSE(delegate2.request_packet_received());
  filter->OnMessageReceived(
      AudioMsg_RequestPacket(kRouteId, stream_id2, AudioBuffersState()));
  EXPECT_FALSE(delegate1.request_packet_received());
  EXPECT_TRUE(delegate2.request_packet_received());
  delegate2.Reset();

  // Send a message of a different route id, a message is not received.
  EXPECT_FALSE(delegate1.request_packet_received());
  filter->OnMessageReceived(
      AudioMsg_RequestPacket(kRouteId + 1, stream_id1, AudioBuffersState()));
  EXPECT_FALSE(delegate1.request_packet_received());

  // Remove the delegates. Make sure they won't get called.
  filter->RemoveDelegate(stream_id1);
  EXPECT_FALSE(delegate1.request_packet_received());
  filter->OnMessageReceived(
      AudioMsg_RequestPacket(kRouteId, stream_id1, AudioBuffersState()));
  EXPECT_FALSE(delegate1.request_packet_received());

  filter->RemoveDelegate(stream_id2);
  EXPECT_FALSE(delegate2.request_packet_received());
  filter->OnMessageReceived(
      AudioMsg_RequestPacket(kRouteId, stream_id2, AudioBuffersState()));
  EXPECT_FALSE(delegate2.request_packet_received());

  message_loop.RunAllPending();
}
