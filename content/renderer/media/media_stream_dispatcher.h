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

// TODO(c.padhi): Rename this class to
// MediaStreamDeviceObserver/MediaStreamDeviceListener, see
// https://crbug.com/742682.
// This class implements a Mojo object that receives device stopped
// notifications and forwards them to MediaStreamDispatcherEventHandler.
class CONTENT_EXPORT MediaStreamDispatcher
    : public RenderFrameObserver,
      public mojom::MediaStreamDispatcher {
 public:
  explicit MediaStreamDispatcher(RenderFrame* render_frame);
  ~MediaStreamDispatcher() override;

  // Get all the media devices of video capture, e.g. webcam. This is the set
  // of devices that should be suspended when the content frame is no longer
  // being shown to the user.
  MediaStreamDevices GetNonScreenCaptureDevices();

  void AddStream(
      const std::string& label,
      const MediaStreamDevices& audio_devices,
      const MediaStreamDevices& video_devices,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler);
  void AddStream(const std::string& label, const MediaStreamDevice& device);
  bool RemoveStream(const std::string& label);
  void RemoveStreamDevice(const MediaStreamDevice& device);

  // Get the video session_id given a label. The label identifies a stream.
  int video_session_id(const std::string& label);
  // Returns an audio session_id given a label.
  int audio_session_id(const std::string& label);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest,
                           GetNonScreenCaptureDevices);

  // Private class for keeping track of opened devices and who have
  // opened it.
  struct Stream;

  // RenderFrameObserver override.
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  void OnDestruct() override;

  // mojom::MediaStreamDispatcher implementation.
  void OnDeviceStopped(const std::string& label,
                       const MediaStreamDevice& device) override;

  void BindMediaStreamDispatcherRequest(
      mojom::MediaStreamDispatcherRequest request);

  mojo::Binding<mojom::MediaStreamDispatcher> binding_;

  // Used for DCHECKs so methods calls won't execute in the wrong thread.
  base::ThreadChecker thread_checker_;

  typedef std::map<std::string, Stream> LabelStreamMap;
  LabelStreamMap label_stream_map_;

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_H_
