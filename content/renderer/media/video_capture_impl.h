// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_

#include <list>
#include <map>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/base/video_capture_types.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gpu {
struct MailboxHolder;
}  // namespace gpu

namespace content {

// VideoCaptureImpl represents a capture device in renderer process. It provides
// an interface for a single client to Start/Stop capture. It also communicates
// to the client when buffer is ready, when state of capture device is changed.

// VideoCaptureImpl is also a delegate of VideoCaptureMessageFilter which relays
// operation of a capture device to the browser process and receives responses
// from browser process.
//
// VideoCaptureImpl is an IO thread only object. See the comments in
// video_capture_impl_manager.cc for the lifetime of this object.
// All methods must be called on the IO thread.
//
// This is an internal class used by VideoCaptureImplManager only. Do not access
// this directly.
class CONTENT_EXPORT VideoCaptureImpl
    : public VideoCaptureMessageFilter::Delegate {
 public:
  ~VideoCaptureImpl() override;

  VideoCaptureImpl(media::VideoCaptureSessionId session_id,
                   VideoCaptureMessageFilter* filter);

  // Start listening to IPC messages.
  void Init();

  // Stop listening to IPC messages.
  void DeInit();

  // Stop/resume delivering video frames to clients, based on flag |suspend|.
  void SuspendCapture(bool suspend);

  // Start capturing using the provided parameters.
  // |state_update_cb| will be called when state changes.
  // |deliver_frame_cb| will be called when a frame is ready.
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureStateUpdateCB& state_update_cb,
                    const VideoCaptureDeliverFrameCB& deliver_frame_cb);

  // Stop capturing.
  void StopCapture();

  // Get capturing formats supported by this device.
  // |callback| will be invoked with the results.
  void GetDeviceSupportedFormats(const VideoCaptureDeviceFormatsCB& callback);

  // Get capturing formats currently in use by this device.
  // |callback| will be invoked with the results.
  void GetDeviceFormatsInUse(const VideoCaptureDeviceFormatsCB& callback);

  media::VideoCaptureSessionId session_id() const { return session_id_; }

 private:
  friend class VideoCaptureImplTest;
  friend class MockVideoCaptureImpl;

  // Carries a shared memory for transferring video frames from browser to
  // renderer.
  class ClientBuffer;

  // VideoCaptureMessageFilter::Delegate interface.
  void OnBufferCreated(base::SharedMemoryHandle handle,
                       int length,
                       int buffer_id) override;
  void OnBufferDestroyed(int buffer_id) override;
  void OnBufferReceived(int buffer_id,
                        base::TimeTicks timestamp,
                        const base::DictionaryValue& metadata,
                        media::VideoPixelFormat pixel_format,
                        media::VideoFrame::StorageType storage_type,
                        const gfx::Size& coded_size,
                        const gfx::Rect& visible_rect,
                        const gpu::MailboxHolder& mailbox_holder) override;
  void OnStateChanged(VideoCaptureState state) override;
  void OnDeviceSupportedFormatsEnumerated(
      const media::VideoCaptureFormats& supported_formats) override;
  void OnDeviceFormatsInUseReceived(
      const media::VideoCaptureFormats& formats_in_use) override;
  void OnDelegateAdded(int32 device_id) override;

  // Sends an IPC message to browser process when all clients are done with the
  // buffer.
  void OnClientBufferFinished(int buffer_id,
                              const scoped_refptr<ClientBuffer>& buffer,
                              uint32 release_sync_point,
                              double consumer_resource_utilization);

  void StopDevice();
  void RestartCapture();
  void StartCaptureInternal();

  virtual void Send(IPC::Message* message);

  // Helpers.
  // Called (by an unknown thread) when all consumers are done with a VideoFrame
  // and its ref-count has gone to zero.  This helper function grabs the
  // RESOURCE_UTILIZATION value from the |metadata| and then runs the given
  // callback, to trampoline back to the IO thread with the values.
  static void DidFinishConsumingFrame(
    const media::VideoFrameMetadata* metadata,
    uint32* release_sync_point_storage,  // Takes ownership.
    const base::Callback<void(uint32, double)>& callback_to_io_thread);

  // Use |state_update_cb_| and |deliver_frame_cb_| to determine whether
  // the VideoCaptureImpl has been initialized.
  bool IsInitialized() const;

  // Reset to a "fresh" client state.
  void ResetClient();

  const scoped_refptr<VideoCaptureMessageFilter> message_filter_;
  int device_id_;
  const int session_id_;

  // Vector of callbacks to be notified of device format enumerations, used only
  // on IO Thread.
  std::vector<VideoCaptureDeviceFormatsCB> device_formats_cb_queue_;
  // Vector of callbacks to be notified of a device's in use capture format(s),
  // used only on IO Thread.
  std::vector<VideoCaptureDeviceFormatsCB> device_formats_in_use_cb_queue_;

  // Buffers available for sending to the client.
  typedef std::map<int32, scoped_refptr<ClientBuffer> > ClientBufferMap;
  ClientBufferMap client_buffers_;

  // Track information for the video capture client, consisting of parameters
  // for capturing and callbacks to the client.
  media::VideoCaptureParams client_params_;
  VideoCaptureStateUpdateCB state_update_cb_;
  VideoCaptureDeliverFrameCB deliver_frame_cb_;

  // The device's first captured frame timestamp sent from browser process side.
  base::TimeTicks first_frame_timestamp_;

  // State of this VideoCaptureImpl.
  VideoCaptureState state_;

  // IO message loop reference for checking correct class operation.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // WeakPtrFactory pointing back to |this| object, for use with
  // media::VideoFrames constructed in OnBufferReceived() from buffers cached
  // in |client_buffers_|.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoCaptureImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
