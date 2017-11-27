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
    : public mojom::MediaStreamDispatcherHost {
 public:
  MediaStreamDispatcherHost(int render_process_id,
                            MediaStreamManager* media_stream_manager);
  ~MediaStreamDispatcherHost() override;

  void BindRequest(mojom::MediaStreamDispatcherHostRequest request);

  void set_salt_and_origin_callback_for_testing(
      MediaDeviceSaltAndOriginCallback callback) {
    salt_and_origin_callback_ = std::move(callback);
  }
  void SetMediaStreamDeviceObserverForTesting(
      int render_frame_id,
      mojom::MediaStreamDeviceObserverPtr observer) {
    observers_[render_frame_id] = std::move(observer);
  }

 private:
  friend class MockMediaStreamDispatcherHost;

  mojom::MediaStreamDeviceObserver* GetMediaStreamDeviceObserverForFrame(
      int render_frame_id);
  void OnMediaStreamDeviceObserverConnectionError(int render_frame_id);
  void CancelAllRequests();

  // mojom::MediaStreamDispatcherHost implementation
  void GenerateStream(int32_t render_frame_id,
                      int32_t request_id,
                      const StreamControls& controls,
                      bool user_gesture,
                      GenerateStreamCallback callback) override;
  void CancelRequest(int32_t render_frame_id, int32_t request_id) override;
  void StopStreamDevice(int32_t render_frame_id,
                        const std::string& device_id) override;
  void OpenDevice(int32_t render_frame_id,
                  int32_t request_id,
                  const std::string& device_id,
                  MediaStreamType type,
                  OpenDeviceCallback callback) override;
  void CloseDevice(const std::string& label) override;
  void SetCapturingLinkSecured(int32_t session_id,
                               MediaStreamType type,
                               bool is_secure) override;
  void OnStreamStarted(const std::string& label) override;

  void DoGenerateStream(
      int32_t render_frame_id,
      int32_t request_id,
      const StreamControls& controls,
      bool user_gesture,
      GenerateStreamCallback callback,
      const std::pair<std::string, url::Origin>& salt_and_origin);
  void DoOpenDevice(int32_t render_frame_id,
                    int32_t request_id,
                    const std::string& device_id,
                    MediaStreamType type,
                    OpenDeviceCallback callback,
                    const std::pair<std::string, url::Origin>& salt_and_origin);

  void OnDeviceStopped(int render_frame_id,
                       const std::string& label,
                       const MediaStreamDevice& device);

  const int render_process_id_;
  MediaStreamManager* media_stream_manager_;
  std::map<int, mojom::MediaStreamDeviceObserverPtr> observers_;
  mojo::BindingSet<mojom::MediaStreamDispatcherHost> bindings_;
  MediaDeviceSaltAndOriginCallback salt_and_origin_callback_;

  base::WeakPtrFactory<MediaStreamDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
