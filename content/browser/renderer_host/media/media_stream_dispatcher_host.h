// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_

#include <map>
#include <string>
#include <utility>

#include "content/browser/browser_message_filter.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"

namespace content {
class ResourceContext;
}  // namespace content

namespace media_stream {

// MediaStreamDispatcherHost is a delegate for Media Stream API messages used by
// MediaStreamImpl. It's the complement of MediaStreamDispatcher
// (owned by RenderView).
class CONTENT_EXPORT MediaStreamDispatcherHost
    : public BrowserMessageFilter,
      public MediaStreamRequester {
 public:
  MediaStreamDispatcherHost(const content::ResourceContext* resource_context,
                            int render_process_id);
  virtual ~MediaStreamDispatcherHost();

  // MediaStreamRequester implementation.
  virtual void StreamGenerated(
      const std::string& label,
      const StreamDeviceInfoArray& audio_devices,
      const StreamDeviceInfoArray& video_devices) OVERRIDE;

  virtual void StreamGenerationFailed(const std::string& label) OVERRIDE;
  virtual void AudioDeviceFailed(const std::string& label, int index) OVERRIDE;
  virtual void VideoDeviceFailed(const std::string& label, int index) OVERRIDE;

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

 private:
  friend class MockMediaStreamDispatcherHost;

  void OnGenerateStream(int render_view_id,
                        int page_request_id,
                        const StreamOptions& components,
                        const std::string& security_origin);

  void OnStopGeneratedStream(int render_view_id, const std::string& label);

  // Returns the media stream manager to forward events to,
  // creating one if needed.
  MediaStreamManager* manager();

  const content::ResourceContext* resource_context_;
  int render_process_id_;

  struct StreamRequest;
  typedef std::map<std::string, StreamRequest> StreamMap;
  // Streams generated for this host.
  StreamMap streams_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcherHost);
};

}  // namespace media_stream

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
