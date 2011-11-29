// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_H_

#include <list>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"

class RenderViewImpl;

// MediaStreamDispatcher is a delegate for the Media Stream API messages.
// MediaStreams are used by WebKit to open media devices such as Video Capture
// and Audio input devices.
// It's the complement of MediaStreamDispatcherHost (owned by
// BrowserRenderProcessHost).
class CONTENT_EXPORT MediaStreamDispatcher
    : public content::RenderViewObserver {
 public:
  explicit MediaStreamDispatcher(RenderViewImpl* render_view);
  virtual ~MediaStreamDispatcher();

  // Request a new media stream to be created.
  // This can be used either of WebKit or a plugin.
  // Note: The event_handler must be valid for as long as the stream exists.
  virtual void GenerateStream(
      int request_id,
      MediaStreamDispatcherEventHandler* event_handler,
      media_stream::StreamOptions components,
      const std::string& security_origin);

  // Stop a started stream. Label is the label provided in OnStreamGenerated.
  virtual void StopStream(const std::string& label);

  // Check if the label is a valid stream.
  virtual bool IsStream(const std::string& label);
  // Get the video session_id given a label. The label identifies a stream.
  // index is the index in the video_device_array of the stream.
  virtual int video_session_id(const std::string& label, int index);
  // Returns an audio session_id given a label and an index.
  virtual int audio_session_id(const std::string& label, int index);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, TestFailure);

  struct Request;

  // Private class for keeping track of opened devices and who have
  // opened it.
  struct Stream;

  // Messages from the browser.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  void OnStreamGenerated(
      int request_id,
      const std::string& label,
      const media_stream::StreamDeviceInfoArray& audio_array,
      const media_stream::StreamDeviceInfoArray& video_array);
  void OnStreamGenerationFailed(int request_id);
  void OnVideoDeviceFailed(const std::string& label, int index);
  void OnAudioDeviceFailed(const std::string& label, int index);

  int next_ipc_id_;
  typedef std::map<std::string, Stream> LabelStreamMap;
  LabelStreamMap label_stream_map_;

  // List of calls made to GenerateStream that has not yet completed.
  typedef std::list<Request> RequestList;
  RequestList requests_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcher);
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_H_
