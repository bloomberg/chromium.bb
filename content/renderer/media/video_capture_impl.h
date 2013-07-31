// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureImpl represents a capture device in renderer process. It provides
// interfaces for clients to Start/Stop capture. It also communicates to clients
// when buffer is ready, state of capture device is changed.

// VideoCaptureImpl is also a delegate of VideoCaptureMessageFilter which
// relays operation of capture device to browser process and receives response
// from browser process.

// The media::VideoCapture and VideoCaptureMessageFilter::Delegate are
// asynchronous interfaces, which means callers can call those interfaces
// from any threads without worrying about thread safety.
// The |capture_message_loop_proxy_| is the working thread of VideoCaptureImpl.
// All non-const members are accessed only on that working thread.
//
// Implementation note: tasks are posted bound to Unretained(this) to both the
// I/O and Capture threads and this is safe (even though the I/O thread is
// scoped to the renderer process and the capture_message_loop_proxy_ thread is
// scoped to the VideoCaptureImplManager) because VideoCaptureImplManager only
// triggers deletion of its VideoCaptureImpl's by calling DeInit which detours
// through the capture & I/O threads, so as long as nobody posts tasks after the
// DeInit() call is made, it is guaranteed none of these Unretained posted tasks
// will dangle after the delete goes through.  The "as long as" is guaranteed by
// clients of VideoCaptureImplManager not using devices after they've
// RemoveDevice'd them.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_

#include <list>
#include <map>

#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_types.h"
#include "media/video/encoded_video_source.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class CONTENT_EXPORT VideoCaptureImpl
    : public media::VideoCapture,
      public VideoCaptureMessageFilter::Delegate,
      public media::EncodedVideoSource {
 public:
  // media::VideoCapture interface.
  virtual void StartCapture(
      media::VideoCapture::EventHandler* handler,
      const media::VideoCaptureCapability& capability) OVERRIDE;
  virtual void StopCapture(media::VideoCapture::EventHandler* handler) OVERRIDE;
  virtual void FeedBuffer(scoped_refptr<VideoFrameBuffer> buffer) OVERRIDE;
  virtual bool CaptureStarted() OVERRIDE;
  virtual int CaptureWidth() OVERRIDE;
  virtual int CaptureHeight() OVERRIDE;
  virtual int CaptureFrameRate() OVERRIDE;

  // VideoCaptureMessageFilter::Delegate interface.
  virtual void OnBufferCreated(base::SharedMemoryHandle handle,
                               int length, int buffer_id) OVERRIDE;
  virtual void OnBufferReceived(int buffer_id, base::Time timestamp) OVERRIDE;
  virtual void OnStateChanged(VideoCaptureState state) OVERRIDE;
  virtual void OnDeviceInfoReceived(
      const media::VideoCaptureParams& device_info) OVERRIDE;
  virtual void OnDeviceInfoChanged(
      const media::VideoCaptureParams& device_info) OVERRIDE;
  virtual void OnDelegateAdded(int32 device_id) OVERRIDE;
  virtual void OnEncodingCapabilitiesAvailable(
          const media::VideoEncodingCapabilities& capabilities) OVERRIDE;
  virtual void OnEncodedBitstreamOpened(
      const media::VideoEncodingParameters& params,
      const std::vector<base::SharedMemoryHandle>& buffers,
      uint32 buffer_size) OVERRIDE;
  virtual void OnEncodedBitstreamClosed() OVERRIDE;
  virtual void OnEncodingConfigChanged(
      const media::RuntimeVideoEncodingParameters& params) OVERRIDE;
  virtual void OnEncodedBufferReady(
      int buffer_id,
      uint32 size,
      const media::BufferEncodingMetadata& metadata) OVERRIDE;

  // media::EncodedVideoSource interface.
  virtual void RequestCapabilities(
      const RequestCapabilitiesCallback& callback) OVERRIDE;
  virtual void OpenBitstream(
      media::EncodedVideoSource::Client* client,
      const media::VideoEncodingParameters& params) OVERRIDE;
  virtual void CloseBitstream() OVERRIDE;
  virtual void ReturnBitstreamBuffer(
      scoped_refptr<const media::EncodedBitstreamBuffer> buffer) OVERRIDE;
  virtual void TrySetBitstreamConfig(
      const media::RuntimeVideoEncodingParameters& params) OVERRIDE;
  virtual void RequestKeyFrame() OVERRIDE;

  // Stop/resume delivering video frames to clients, based on flag |suspend|.
  virtual void SuspendCapture(bool suspend);

 private:
  friend class VideoCaptureImplManager;
  friend class VideoCaptureImplTest;
  friend class MockVideoCaptureImpl;

  struct DIBBuffer;
  typedef std::map<media::VideoCapture::EventHandler*,
      media::VideoCaptureCapability> ClientInfo;

  VideoCaptureImpl(media::VideoCaptureSessionId id,
                   base::MessageLoopProxy* capture_message_loop_proxy,
                   VideoCaptureMessageFilter* filter);
  virtual ~VideoCaptureImpl();

  void DoStartCaptureOnCaptureThread(
      media::VideoCapture::EventHandler* handler,
      const media::VideoCaptureCapability& capability);
  void DoStopCaptureOnCaptureThread(media::VideoCapture::EventHandler* handler);
  void DoFeedBufferOnCaptureThread(scoped_refptr<VideoFrameBuffer> buffer);

  void DoBufferCreatedOnCaptureThread(base::SharedMemoryHandle handle,
                                      int length, int buffer_id);
  void DoBufferReceivedOnCaptureThread(int buffer_id, base::Time timestamp);
  void DoStateChangedOnCaptureThread(VideoCaptureState state);
  void DoDeviceInfoReceivedOnCaptureThread(
      const media::VideoCaptureParams& device_info);
  void DoDeviceInfoChangedOnCaptureThread(
      const media::VideoCaptureParams& device_info);
  void DoDelegateAddedOnCaptureThread(int32 device_id);

  void DoSuspendCaptureOnCaptureThread(bool suspend);

  void StartFetchCapabilities();
  void DoRequestCapabilitiesOnCaptureThread(
      const RequestCapabilitiesCallback& callback);
  void DoOpenBitstreamOnCaptureThread(
      media::EncodedVideoSource::Client* client,
      const media::VideoEncodingParameters& params);
  void DoCloseBitstreamOnCaptureThread();
  void DoNotifyBitstreamOpenedOnCaptureThread(
      const media::VideoEncodingParameters& params,
      const std::vector<base::SharedMemoryHandle>& buffers,
      uint32 buffer_size);
  void DoNotifyBitstreamClosedOnCaptureThread();
  void DoNotifyBitstreamConfigChangedOnCaptureThread(
      const media::RuntimeVideoEncodingParameters& params);
  void DoNotifyBitstreamBufferReadyOnCaptureThread(
      int buffer_id, uint32 size,
      const media::BufferEncodingMetadata& metadata);
  void DoNotifyCapabilitiesAvailableOnCaptureThread(
      const media::VideoEncodingCapabilities& capabilities);

  void Init();
  void DeInit(base::Closure task);
  void DoDeInitOnCaptureThread(base::Closure task);
  void StopDevice();
  void RestartCapture();
  void StartCaptureInternal();
  void AddDelegateOnIOThread();
  void RemoveDelegateOnIOThread(base::Closure task);
  virtual void Send(IPC::Message* message);

  // Helpers.
  bool ClientHasDIB() const;
  bool RemoveClient(media::VideoCapture::EventHandler* handler,
                    ClientInfo* clients);

  const scoped_refptr<VideoCaptureMessageFilter> message_filter_;
  const scoped_refptr<base::MessageLoopProxy> capture_message_loop_proxy_;
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  int device_id_;

  // Pool of DIBs. The key is buffer_id.
  typedef std::map<int, DIBBuffer*> CachedDIB;
  CachedDIB cached_dibs_;

  ClientInfo clients_;

  ClientInfo clients_pending_on_filter_;
  ClientInfo clients_pending_on_restart_;

  media::VideoCaptureCapability::Format video_type_;

  // Member capture_format_ represents the video format requested by the client
  // to this class via DoStartCaptureOnCaptureThread.
  media::VideoCaptureCapability capture_format_;

  // The device's video capture format sent from browser process side.
  media::VideoCaptureParams device_info_;
  bool device_info_available_;

  bool suspended_;
  VideoCaptureState state_;

  // Video encoding capabilities as reported by the device.
  media::VideoEncodingCapabilities encoding_caps_;
  // Callback for RequestCapabilities().
  RequestCapabilitiesCallback encoding_caps_callback_;
  // Pointer to the EVS client.
  media::EncodedVideoSource::Client* encoded_video_source_client_;
  // Bitstream buffers returned by the video capture device. Unowned.
  std::vector<base::SharedMemory*> bitstream_buffers_;
  // |bitstream_open_| is set to true when renderer receives BitstreamOpened
  // message acknowledging an OpenBitstream request, and is set to false when
  // the EVS client requests to close bitstream, or when renderer receives
  // BitstreamClosed message from the browser procses.
  bool bitstream_open_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_IMPL_H_
