// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_H_

#include <list>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"

namespace base {
class MessageLoopProxy;
}

class RenderViewImpl;

// MediaStreamDispatcher is a delegate for the Media Stream API messages.
// MediaStreams are used by WebKit to open media devices such as Video Capture
// and Audio input devices.
// It's the complement of MediaStreamDispatcherHost (owned by
// BrowserRenderProcessHost).
class CONTENT_EXPORT MediaStreamDispatcher
    : public content::RenderViewObserver,
      public base::SupportsWeakPtr<MediaStreamDispatcher> {
 public:
  explicit MediaStreamDispatcher(RenderViewImpl* render_view);
  virtual ~MediaStreamDispatcher();

  // Request a new media stream to be created.
  // This can be used either by WebKit or a plugin.
  // Note: The event_handler must be valid for as long as the stream exists.
  virtual void GenerateStream(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
      media_stream::StreamOptions components,
      const GURL& security_origin);

  // Like GenerateStream above, except use the device specified by |device_id|
  // rather than allow the user to choose one.
  virtual void GenerateStreamForDevice(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
      media_stream::StreamOptions components,
      const std::string& device_id,
      const GURL& security_origin);

  // Cancel the request for a new media stream to be created.
  virtual void CancelGenerateStream(int request_id);

  // Stop a started stream. Label is the label provided in OnStreamGenerated.
  virtual void StopStream(const std::string& label);

  // Request to enumerate devices.
  void EnumerateDevices(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
      media_stream::MediaStreamType type,
      const GURL& security_origin);

  // Request to stop enumerating devices.
  void StopEnumerateDevices(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler);

  // Request to open a device.
  void OpenDevice(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
      const std::string& device_id,
      media_stream::MediaStreamType type,
      const GURL& security_origin);

  // Close a started device. |label| is provided in OnDeviceOpened.
  void CloseDevice(const std::string& label);

  // Check if the label is a valid stream.
  virtual bool IsStream(const std::string& label);
  // Get the video session_id given a label. The label identifies a stream.
  // index is the index in the video_device_array of the stream.
  virtual int video_session_id(const std::string& label, int index);
  // Returns an audio session_id given a label and an index.
  virtual int audio_session_id(const std::string& label, int index);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, BasicStream);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, BasicStreamForDevice);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, BasicVideoDevice);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, TestFailure);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherTest, CancelGenerateStream);

  struct Request;

  // Private class for keeping track of opened devices and who have
  // opened it.
  struct Stream;

  struct EnumerationRequest {
    EnumerationRequest(
        const base::WeakPtr<MediaStreamDispatcherEventHandler>& handler,
        int request_id);
    ~EnumerationRequest();

    base::WeakPtr<MediaStreamDispatcherEventHandler> handler;
    int request_id;
  };

  // List of requests made to EnumerateDevices.
  typedef std::list<EnumerationRequest> EnumerationRequestList;

  struct EnumerationState {
    EnumerationState();
    ~EnumerationState();

    struct CachedDevices;

    // If |ipc_id| >= 0, then we've started.
    int ipc_id;
    scoped_ptr<CachedDevices> cached_devices;
    EnumerationRequestList requests;
  };

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
  void OnDevicesEnumerated(
      int request_id,
      const std::string& label,
      const media_stream::StreamDeviceInfoArray& device_array);
  void OnDevicesEnumerationFailed(int request_id);
  void OnDeviceOpened(
      int request_id,
      const std::string& label,
      const media_stream::StreamDeviceInfo& device_info);
  void OnDeviceOpenFailed(int request_id);

  void RemoveEnumerationRequest(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
      EnumerationState* state);

  // Used for DCHECKs so methods calls won't execute in the wrong thread.
  scoped_refptr<base::MessageLoopProxy> main_loop_;

  int next_ipc_id_;
  typedef std::map<std::string, Stream> LabelStreamMap;
  LabelStreamMap label_stream_map_;

  EnumerationState audio_enumeration_state_;
  EnumerationState video_enumeration_state_;

  // List of calls made to GenerateStream/GenerateStreamForDevice that has not
  // yet completed.
  typedef std::list<Request> RequestList;
  RequestList requests_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcher);
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DISPATCHER_H_
