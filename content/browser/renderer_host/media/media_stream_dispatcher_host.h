// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_

#include <map>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream.mojom.h"
#include "content/common/media/media_stream_controls.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace url {
class Origin;
}

namespace content {

class MediaStreamManager;

// MediaStreamDispatcherHost is a delegate for Media Stream API messages used by
// MediaStreamImpl.  There is one MediaStreamDispatcherHost per
// RenderProcessHost, the former owned by the latter.
class CONTENT_EXPORT MediaStreamDispatcherHost
    : public mojom::MediaStreamDispatcherHost,
      public MediaStreamRequester {
 public:
  MediaStreamDispatcherHost(int render_process_id,
                            MediaStreamManager* media_stream_manager);
  ~MediaStreamDispatcherHost() override;

  void BindRequest(mojom::MediaStreamDispatcherHostRequest request);

  // MediaStreamRequester implementation.
  void StreamGenerated(int render_frame_id,
                       int page_request_id,
                       const std::string& label,
                       const MediaStreamDevices& audio_devices,
                       const MediaStreamDevices& video_devices) override;
  void StreamGenerationFailed(int render_frame_id,
                              int page_request_id,
                              MediaStreamRequestResult result) override;
  void DeviceStopped(int render_frame_id,
                     const std::string& label,
                     const MediaStreamDevice& device) override;
  void DeviceOpened(int render_frame_id,
                    int page_request_id,
                    const std::string& label,
                    const MediaStreamDevice& device) override;

  void set_salt_and_origin_callback_for_testing(
      MediaDeviceSaltAndOriginCallback callback) {
    salt_and_origin_callback_ = std::move(callback);
  }
  void SetMediaStreamDispatcherForTesting(
      int render_frame_id,
      mojom::MediaStreamDispatcherPtr dispatcher) {
    dispatchers_[render_frame_id] = std::move(dispatcher);
  }

 private:
  friend class MockMediaStreamDispatcherHost;

  mojom::MediaStreamDispatcher* GetMediaStreamDispatcherForFrame(
      int render_frame_id);
  void OnMediaStreamDispatcherConnectionError(int render_frame_id);
  void CancelAllRequests();
  void DeviceOpenFailed(int render_frame_id, int page_request_id);

  // mojom::MediaStreamDispatcherHost implementation
  void GenerateStream(int32_t render_frame_id,
                      int32_t request_id,
                      const StreamControls& controls,
                      bool user_gesture) override;
  void CancelGenerateStream(int32_t render_frame_id,
                            int32_t request_id) override;
  void StopStreamDevice(int32_t render_frame_id,
                        const std::string& device_id) override;
  void OpenDevice(int32_t render_frame_id,
                  int32_t request_id,
                  const std::string& device_id,
                  MediaStreamType type) override;
  void CloseDevice(const std::string& label) override;
  void SetCapturingLinkSecured(int32_t session_id,
                               MediaStreamType type,
                               bool is_secure) override;
  void StreamStarted(const std::string& label) override;

  void DoGenerateStream(
      int32_t render_frame_id,
      int32_t request_id,
      const StreamControls& controls,
      bool user_gesture,
      const std::pair<std::string, url::Origin>& salt_and_origin);
  void DoOpenDevice(int32_t render_frame_id,
                    int32_t request_id,
                    const std::string& device_id,
                    MediaStreamType type,
                    const std::pair<std::string, url::Origin>& salt_and_origin);

  const int render_process_id_;
  MediaStreamManager* media_stream_manager_;
  std::map<int, mojom::MediaStreamDispatcherPtr> dispatchers_;
  mojo::BindingSet<mojom::MediaStreamDispatcherHost> bindings_;
  MediaDeviceSaltAndOriginCallback salt_and_origin_callback_;

  base::WeakPtrFactory<MediaStreamDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
