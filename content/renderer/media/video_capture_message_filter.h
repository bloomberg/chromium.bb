// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MessageFilter that handles video capture messages and delegates them to
// video captures. VideoCaptureMessageFilter is operated on IO thread of
// render process. It intercepts video capture messages and process them on
// IO thread since these messages are time critical.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_

#include <map>

#include "base/message_loop_proxy.h"
#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/video/capture/video_capture.h"

class CONTENT_EXPORT VideoCaptureMessageFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  class CONTENT_EXPORT Delegate {
   public:
    // Called when a video frame buffer is created in the browser process.
    virtual void OnBufferCreated(base::SharedMemoryHandle handle,
                                 int length, int buffer_id) = 0;

    // Called when a video frame buffer is received from the browser process.
    virtual void OnBufferReceived(int buffer_id, base::Time timestamp) = 0;

    // Called when state of a video capture device has changed in the browser
    // process.
    virtual void OnStateChanged(const media::VideoCapture::State& state) = 0;

    // Called when device info is received from video capture device in the
    // browser process.
    virtual void OnDeviceInfoReceived(
        const media::VideoCaptureParams& device_info) = 0;

    // Called when the delegate has been added to filter's delegate list.
    // |device_id| is the device id for the delegate.
    virtual void OnDelegateAdded(int32 device_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  VideoCaptureMessageFilter();
  virtual ~VideoCaptureMessageFilter();

  // Add a delegate to the map.
  void AddDelegate(Delegate* delegate);

  // Remove a delegate from the map.
  void RemoveDelegate(Delegate* delegate);

  // Send a message asynchronously.
  virtual bool Send(IPC::Message* message);

 private:
  FRIEND_TEST_ALL_PREFIXES(VideoCaptureMessageFilterTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(VideoCaptureMessageFilterTest, Delegates);

  typedef std::map<int32, Delegate*> Delegates;

  // IPC::ChannelProxy::MessageFilter override. Called on IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Receive a newly created buffer from browser process.
  void OnBufferCreated(int device_id,
                       base::SharedMemoryHandle handle,
                       int length, int buffer_id);

  // Receive a buffer from browser process.
  void OnBufferReceived(int device_id,
                        int buffer_id, base::Time timestamp);

  // State of browser process' video capture device has changed.
  void OnDeviceStateChanged(int device_id,
                            const media::VideoCapture::State& state);

  // Receive device info from browser process.
  void OnDeviceInfoReceived(int device_id,
                            const media::VideoCaptureParams& params);

  // A map of device ids to delegates.
  Delegates delegates_;
  Delegates pending_delegates_;
  int32 last_device_id_;

  IPC::Channel* channel_;

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureMessageFilter);
};

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_
