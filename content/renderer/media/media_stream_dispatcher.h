// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream.mojom.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {

class MediaStreamDispatcherEventHandler;

// MediaStreamDispatcher is a delegate for the Media Stream API messages.
// MediaStreams are used by WebKit to open media devices such as Video Capture
// and Audio input devices.
// It's the complement of MediaStreamDispatcherHost (owned by
// RenderProcessHostImpl).
class CONTENT_EXPORT MediaStreamDispatcher
    : public RenderFrameObserver,
      public mojom::MediaStreamDispatcher {
 public:
  explicit MediaStreamDispatcher(RenderFrame* render_frame);
  ~MediaStreamDispatcher() override;

  // Request a new media stream to be created.
  // This can be used either by WebKit or a plugin.
  // Note: The event_handler must be valid for as long as the stream exists.
  virtual void GenerateStream(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
      const StreamControls& controls,
      bool is_processing_user_gesture);

  // Cancel the request for a new media stream to be created.
  virtual void CancelGenerateStream(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler);

  // Stop a started device that has been requested by calling GenerateStream.
  virtual void StopStreamDevice(const MediaStreamDevice& device);

  // Request to open a device.
  void OpenDevice(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
      const std::string& device_id,
      MediaStreamType type);

  // Cancel the request to open a device.
  virtual void CancelOpenDevice(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler);

  // Close a started device. |label| is provided in OnDeviceOpened.
  void CloseDevice(const std::string& label);

  // This method is called when the stream is started successfully.
  void OnStreamStarted(const std::string& label);

  // Get all the media devices of video capture, e.g. webcam. This is the set
  // of devices that should be suspended when the content frame is no longer
  // being shown to the user.
  MediaStreamDevices GetNonScreenCaptureDevices();

  // Check if the label is a valid stream.
  virtual bool IsStream(const std::string& label);
  // Get the video session_id given a label. The label identifies a stream.
  // index is the index in the video_device_array of the stream.
  virtual int video_session_id(const std::string& label, int index);
  // Returns an audio session_id given a label and an index.
  virtual int audio_session_id(const std::string& label, int index);

 private:
  friend class MediaStreamDispatcherTest;
  friend class UserMediaClientImplTest;
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, BasicVideoDevice);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, TestFailure);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, CancelGenerateStream);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest,
                           GetNonScreenCaptureDevices);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, DeviceClosed);

  struct Request;

  // Private class for keeping track of opened devices and who have
  // opened it.
  struct Stream;

  // RenderFrameObserver override.
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  void OnDestruct() override;

  // mojom::MediaStreamDispatcher implementation.
  void OnStreamGenerated(int32_t request_id,
                         const std::string& label,
                         const MediaStreamDevices& audio_devices,
                         const MediaStreamDevices& video_devices) override;
  void OnStreamGenerationFailed(int32_t request_id,
                                MediaStreamRequestResult result) override;
  void OnDeviceOpened(int32_t request_id,
                      const std::string& label,
                      const MediaStreamDevice& device) override;
  void OnDeviceOpenFailed(int32_t request_id) override;
  void OnDeviceStopped(const std::string& label,
                       const MediaStreamDevice& device) override;

  void BindMediaStreamDispatcherRequest(
      mojom::MediaStreamDispatcherRequest request);

  const mojom::MediaStreamDispatcherHostPtr& GetMediaStreamDispatcherHost();

  mojom::MediaStreamDispatcherHostPtr dispatcher_host_;
  mojo::Binding<mojom::MediaStreamDispatcher> binding_;

  // Used for DCHECKs so methods calls won't execute in the wrong thread.
  base::ThreadChecker thread_checker_;

  int next_ipc_id_;
  typedef std::map<std::string, Stream> LabelStreamMap;
  LabelStreamMap label_stream_map_;

  // List of calls made to the browser process that have not yet completed or
  // been canceled.
  typedef std::list<Request> RequestList;
  RequestList requests_;

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_H_
