// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureImpl represents a capture device in renderer process. It provides
// interfaces for clients to Start/Stop capture. It also communicates to clients
// when buffer is ready, state of capture device is changed.

// VideoCaptureImpl is also a delegate of VideoCaptureMessageFilter which relays
// operation of a capture device to the browser process and receives responses
// from browser process.
//
// All public methods of VideoCaptureImpl can be called on any thread.
// Internally it runs on the IO thread. Clients of this class implement
// interface media::VideoCapture::EventHandler which is called only on the IO
// thread.
//
// Implementation note: tasks are posted bound to Unretained(this) to the I/O
// thread and this is safe (even though the I/O thread is scoped to the renderer
// process) because VideoCaptureImplManager only triggers deletion of its
// VideoCaptureImpl's by calling DeInit which detours through the I/O thread, so
// as long as nobody posts tasks after the DeInit() call is made, it is
// guaranteed none of these Unretained posted tasks will dangle after the delete
// goes through. The "as long as" is guaranteed by clients of
// VideoCaptureImplManager not using devices after they've released
// VideoCaptureHandle, which is a wrapper of this object.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_

#include <list>
#include <map>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_types.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace gpu {
struct MailboxHolder;
}  // namespace gpu

namespace content {

class CONTENT_EXPORT VideoCaptureImpl
    : public media::VideoCapture, public VideoCaptureMessageFilter::Delegate {
 public:
  VideoCaptureImpl(media::VideoCaptureSessionId session_id,
                   VideoCaptureMessageFilter* filter);
  virtual ~VideoCaptureImpl();

  // Start listening to IPC messages.
  void Init();

  // Stop listening to IPC messages. Call |done_cb| when done.
  void DeInit(base::Closure done_cb);

  // Stop/resume delivering video frames to clients, based on flag |suspend|.
  void SuspendCapture(bool suspend);

  // media::VideoCapture interface.
  virtual void StartCapture(
      media::VideoCapture::EventHandler* handler,
      const media::VideoCaptureParams& params) OVERRIDE;
  virtual void StopCapture(media::VideoCapture::EventHandler* handler) OVERRIDE;
  virtual bool CaptureStarted() OVERRIDE;
  virtual int CaptureFrameRate() OVERRIDE;
  virtual void GetDeviceSupportedFormats(
      const DeviceFormatsCallback& callback) OVERRIDE;
  virtual void GetDeviceFormatsInUse(
      const DeviceFormatsInUseCallback& callback) OVERRIDE;

  media::VideoCaptureSessionId session_id() const { return session_id_; }

 private:
  friend class VideoCaptureImplTest;
  friend class MockVideoCaptureImpl;

  class ClientBuffer;
  typedef std::map<media::VideoCapture::EventHandler*,
                   media::VideoCaptureParams> ClientInfo;

  void InitOnIOThread();
  void DeInitOnIOThread(base::Closure done_cb);
  void SuspendCaptureOnIOThread(bool suspend);
  void StartCaptureOnIOThread(
      media::VideoCapture::EventHandler* handler,
      const media::VideoCaptureParams& params);
  void StopCaptureOnIOThread(media::VideoCapture::EventHandler* handler);
  void GetDeviceSupportedFormatsOnIOThread(
      const DeviceFormatsCallback& callback);
  void GetDeviceFormatsInUseOnIOThread(
      const DeviceFormatsInUseCallback& callback);

  // VideoCaptureMessageFilter::Delegate interface.
  virtual void OnBufferCreated(base::SharedMemoryHandle handle,
                               int length,
                               int buffer_id) OVERRIDE;
  virtual void OnBufferDestroyed(int buffer_id) OVERRIDE;
  virtual void OnBufferReceived(int buffer_id,
                                const media::VideoCaptureFormat& format,
                                base::TimeTicks) OVERRIDE;
  virtual void OnMailboxBufferReceived(int buffer_id,
                                       const gpu::MailboxHolder& mailbox_holder,
                                       const media::VideoCaptureFormat& format,
                                       base::TimeTicks timestamp) OVERRIDE;
  virtual void OnStateChanged(VideoCaptureState state) OVERRIDE;
  virtual void OnDeviceSupportedFormatsEnumerated(
      const media::VideoCaptureFormats& supported_formats) OVERRIDE;
  virtual void OnDeviceFormatsInUseReceived(
      const media::VideoCaptureFormats& formats_in_use) OVERRIDE;
  virtual void OnDelegateAdded(int32 device_id) OVERRIDE;

  // Sends an IPC message to browser process when all clients are done with the
  // buffer.
  void OnClientBufferFinished(int buffer_id,
                              const scoped_refptr<ClientBuffer>& buffer,
                              scoped_ptr<gpu::MailboxHolder> mailbox_holder);

  void StopDevice();
  void RestartCapture();
  void StartCaptureInternal();

  virtual void Send(IPC::Message* message);

  // Helpers.
  bool RemoveClient(media::VideoCapture::EventHandler* handler,
                    ClientInfo* clients);

  const scoped_refptr<VideoCaptureMessageFilter> message_filter_;
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  int device_id_;
  const int session_id_;

  // Vector of callbacks to be notified of device format enumerations, used only
  // on IO Thread.
  std::vector<DeviceFormatsCallback> device_formats_callback_queue_;
  // Vector of callbacks to be notified of a device's in use capture format(s),
  // used only on IO Thread.
  std::vector<DeviceFormatsInUseCallback> device_formats_in_use_callback_queue_;

  // Buffers available for sending to the client.
  typedef std::map<int32, scoped_refptr<ClientBuffer> > ClientBufferMap;
  ClientBufferMap client_buffers_;

  ClientInfo clients_;
  ClientInfo clients_pending_on_filter_;
  ClientInfo clients_pending_on_restart_;

  // Member params_ represents the video format requested by the
  // client to this class via StartCapture().
  media::VideoCaptureParams params_;

  // The device's video capture format sent from browser process side.
  media::VideoCaptureFormat last_frame_format_;

  // The device's first captured frame timestamp sent from browser process side.
  base::TimeTicks first_frame_timestamp_;

  bool suspended_;
  VideoCaptureState state_;

  // WeakPtrFactory pointing back to |this| object, for use with
  // media::VideoFrames constructed in OnBufferReceived() from buffers cached
  // in |client_buffers_|.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoCaptureImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
