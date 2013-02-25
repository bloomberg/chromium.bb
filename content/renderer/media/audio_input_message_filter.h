// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_

#include "base/id_map.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/audio/audio_input_ipc.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// MessageFilter that handles audio input messages and delegates them to
// audio capturers. Created on render thread, AudioMessageFilter is operated on
// IO thread (secondary thread of render process), it intercepts audio messages
// and process them on IO thread since these messages are time critical.
class CONTENT_EXPORT AudioInputMessageFilter
    : public IPC::ChannelProxy::MessageFilter,
      public NON_EXPORTED_BASE(media::AudioInputIPC) {
 public:
  explicit AudioInputMessageFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  // Getter for the one AudioInputMessageFilter object.
  static AudioInputMessageFilter* Get();

  // Associates |render_view_id| as the destination of audio for a stream.
  void AssociateStreamWithConsumer(int stream_id, int render_view_id);

  // Implementation of AudioInputIPC.  All methods must be called on the
  // provided |io_message_loop|.
  virtual int AddDelegate(
      media::AudioInputIPCDelegate* delegate) OVERRIDE;
  virtual void RemoveDelegate(int id) OVERRIDE;
  virtual void CreateStream(int stream_id, const media::AudioParameters& params,
      const std::string& device_id, bool automatic_gain_control) OVERRIDE;
  virtual void StartDevice(int stream_id, int session_id) OVERRIDE;
  virtual void RecordStream(int stream_id) OVERRIDE;
  virtual void CloseStream(int stream_id) OVERRIDE;
  virtual void SetVolume(int stream_id, double volume) OVERRIDE;

  scoped_refptr<base::MessageLoopProxy> io_message_loop() const {
    return io_message_loop_;
  }

 private:
  virtual ~AudioInputMessageFilter();

  // Sends an IPC message using |channel_|.
  void Send(IPC::Message* message);

  // IPC::ChannelProxy::MessageFilter override. Called on |io_message_loop_|.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Received when browser process has created an audio input stream.
  void OnStreamCreated(int stream_id, base::SharedMemoryHandle handle,
#if defined(OS_WIN)
                       base::SyncSocket::Handle socket_handle,
#else
                       base::FileDescriptor socket_descriptor,
#endif
                       uint32 length);

  // Notification of volume property of an audio input stream.
  void OnStreamVolume(int stream_id, double volume);

  // Received when internal state of browser process' audio input stream has
  // changed.
  void OnStreamStateChanged(int stream_id,
                            media::AudioInputIPCDelegate::State state);

  // Notification of the opened device of an audio session.
  void OnDeviceStarted(int stream_id, const std::string& device_id);

  // A map of stream ids to delegates.
  IDMap<media::AudioInputIPCDelegate> delegates_;

  // IPC channel for Send(), must only be accesed on |io_message_loop_|.
  IPC::Channel* channel_;

  // The singleton instance for this filter.
  static AudioInputMessageFilter* filter_;

  // Message loop on which IPC calls are driven.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_INPUT_MESSAGE_FILTER_H_
