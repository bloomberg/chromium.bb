// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MessageFilter that handles audio messages and delegates them to audio
// renderers. Created on render thread, AudioMessageFilter is operated on
// IO thread (secondary thread of render process) it intercepts audio messages
// and process them on IO thread since these messages are time critical.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_

#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_output_ipc.h"

class CONTENT_EXPORT AudioMessageFilter
    : public IPC::ChannelProxy::MessageFilter,
      public NON_EXPORTED_BASE(media::AudioOutputIPC) {
 public:
  AudioMessageFilter();

  // Getter for the one AudioMessageFilter object.
  static AudioMessageFilter* Get();

  // media::AudioOutputIPCDelegate implementation.
  virtual int AddDelegate(media::AudioOutputIPCDelegate* delegate) OVERRIDE;
  virtual void RemoveDelegate(int id) OVERRIDE;
  virtual void CreateStream(int stream_id,
      const media::AudioParameters& params, int input_channels) OVERRIDE;
  virtual void PlayStream(int stream_id) OVERRIDE;
  virtual void PauseStream(int stream_id) OVERRIDE;
  virtual void FlushStream(int stream_id) OVERRIDE;
  virtual void CloseStream(int stream_id) OVERRIDE;
  virtual void SetVolume(int stream_id, double volume) OVERRIDE;

  // IPC::ChannelProxy::MessageFilter override. Called on IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

 protected:
  virtual ~AudioMessageFilter();

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioMessageFilterTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(AudioMessageFilterTest, Delegates);

  // Sends an IPC message using |channel_|.
  bool Send(IPC::Message* message);

  // Received when browser process has created an audio output stream.
  void OnStreamCreated(int stream_id, base::SharedMemoryHandle handle,
#if defined(OS_WIN)
                       base::SyncSocket::Handle socket_handle,
#else
                       base::FileDescriptor socket_descriptor,
#endif
                       uint32 length);

  // Received when internal state of browser process' audio output device has
  // changed.
  void OnStreamStateChanged(int stream_id,
                            media::AudioOutputIPCDelegate::State state);

  // The singleton instance for this filter.
  static AudioMessageFilter* filter_;

  // A map of stream ids to delegates.
  IDMap<media::AudioOutputIPCDelegate> delegates_;

  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(AudioMessageFilter);
};

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_MESSAGE_FILTER_H_
