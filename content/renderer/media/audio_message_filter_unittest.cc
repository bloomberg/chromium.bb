// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

  virtual void OnStateChanged(AudioStreamState state) {
    state_changed_received_ = true;
    state_ = state;
  }

  virtual void OnStreamCreated(base::SharedMemoryHandle handle,
                               base::SyncSocket::Handle,
                               uint32 length) {
    created_received_ = true;
    handle_ = handle;
    length_ = length;
  }

  virtual void OnVolume(double volume) {
    volume_received_ = true;
    volume_ = volume;
  }

  void Reset() {
    state_changed_received_ = false;
    state_ = kAudioStreamError;

    created_received_ = false;
    handle_ = base::SharedMemory::NULLHandle();
    length_ = 0;

    volume_received_ = false;
    volume_ = 0;
  }

  bool state_changed_received() { return state_changed_received_; }
  AudioStreamState state() { return state_; }

  bool created_received() { return created_received_; }
  base::SharedMemoryHandle handle() { return handle_; }
  uint32 length() { return length_; }

  bool volume_received() { return volume_received_; }
  double volume() { return volume_; }

 private:
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

  scoped_refptr<AudioMessageFilter> filter(new AudioMessageFilter());

  MockAudioDelegate delegate;
  int stream_id = filter->AddDelegate(&delegate);

  // AudioMsg_NotifyStreamCreated
#if defined(OS_WIN)
  base::SyncSocket::Handle socket_handle;
#else
  base::FileDescriptor socket_handle;
#endif
  const uint32 kLength = 1024;
  EXPECT_FALSE(delegate.created_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamCreated(
          stream_id, base::SharedMemory::NULLHandle(),
          socket_handle, kLength));
  EXPECT_TRUE(delegate.created_received());
  EXPECT_FALSE(base::SharedMemory::IsHandleValid(delegate.handle()));
  EXPECT_EQ(kLength, delegate.length());
  delegate.Reset();

  // AudioMsg_NotifyStreamStateChanged
  EXPECT_FALSE(delegate.state_changed_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamStateChanged(stream_id, kAudioStreamPlaying));
  EXPECT_TRUE(delegate.state_changed_received());
  EXPECT_TRUE(kAudioStreamPlaying == delegate.state());
  delegate.Reset();

  message_loop.RunAllPending();
}

TEST(AudioMessageFilterTest, Delegates) {
  MessageLoop message_loop(MessageLoop::TYPE_IO);

  scoped_refptr<AudioMessageFilter> filter(new AudioMessageFilter());

  MockAudioDelegate delegate1;
  MockAudioDelegate delegate2;

  int stream_id1 = filter->AddDelegate(&delegate1);
  int stream_id2 = filter->AddDelegate(&delegate2);

  // Send an IPC message. Make sure the correct delegate gets called.
  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamStateChanged(stream_id1, kAudioStreamPlaying));
  EXPECT_TRUE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  delegate1.Reset();

  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamStateChanged(stream_id2, kAudioStreamPlaying));
  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_TRUE(delegate2.state_changed_received());
  delegate2.Reset();

  // Remove the delegates. Make sure they won't get called.
  filter->RemoveDelegate(stream_id1);
  EXPECT_FALSE(delegate1.state_changed_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamStateChanged(stream_id1, kAudioStreamPlaying));
  EXPECT_FALSE(delegate1.state_changed_received());

  filter->RemoveDelegate(stream_id2);
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamStateChanged(stream_id2, kAudioStreamPlaying));
  EXPECT_FALSE(delegate2.state_changed_received());

  message_loop.RunAllPending();
}
