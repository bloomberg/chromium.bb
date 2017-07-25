// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_

#include <string>

#include "base/macros.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream.mojom.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"

namespace url {
class Origin;
}

namespace content {

class MediaStreamManager;

// MediaStreamDispatcherHost is a delegate for Media Stream API messages used by
// MediaStreamImpl.  There is one MediaStreamDispatcherHost per
// RenderProcessHost, the former owned by the latter.
class CONTENT_EXPORT MediaStreamDispatcherHost
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<mojom::MediaStreamDispatcherHost>,
      public mojom::MediaStreamDispatcherHost,
      public MediaStreamRequester {
 public:
  MediaStreamDispatcherHost(int render_process_id,
                            const std::string& salt,
                            MediaStreamManager* media_stream_manager);

  // MediaStreamRequester implementation.
  void StreamGenerated(int render_frame_id,
                       int page_request_id,
                       const std::string& label,
                       const StreamDeviceInfoArray& audio_devices,
                       const StreamDeviceInfoArray& video_devices) override;
  void StreamGenerationFailed(int render_frame_id,
                              int page_request_id,
                              MediaStreamRequestResult result) override;
  void DeviceStopped(int render_frame_id,
                     const std::string& label,
                     const StreamDeviceInfo& device) override;
  void DeviceOpened(int render_frame_id,
                    int page_request_id,
                    const std::string& label,
                    const StreamDeviceInfo& video_device) override;

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelClosing() override;

 protected:
  ~MediaStreamDispatcherHost() override;

 private:
  friend class MockMediaStreamDispatcherHost;

  // IPC message handlers.
  void OnGenerateStream(int render_frame_id,
                        int page_request_id,
                        const StreamControls& controls,
                        const url::Origin& security_origin,
                        bool user_gesture);

  // mojom::MediaStreamDispatcherHost implementation
  void CancelGenerateStream(int32_t render_frame_id,
                            int32_t request_id) override;
  void StopStreamDevice(int32_t render_frame_id,
                        const std::string& device_id) override;
  void OpenDevice(int32_t render_frame_id,
                  int32_t request_id,
                  const std::string& device_id,
                  MediaStreamType type,
                  const url::Origin& security_origin) override;
  void CloseDevice(const std::string& label) override;
  void SetCapturingLinkSecured(int32_t session_id,
                               MediaStreamType type,
                               bool is_secure) override;
  void StreamStarted(const std::string& label) override;

  int render_process_id_;
  std::string salt_;
  MediaStreamManager* media_stream_manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
