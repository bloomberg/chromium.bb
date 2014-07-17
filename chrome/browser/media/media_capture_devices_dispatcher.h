// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_
#define CHROME_BROWSER_MEDIA_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_

#include <deque>
#include <list>
#include <map>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/media_stream_request.h"

class DesktopStreamsRegistry;
class MediaStreamCaptureIndicator;
class Profile;

namespace extensions {
class Extension;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This singleton is used to receive updates about media events from the content
// layer.
class MediaCaptureDevicesDispatcher : public content::MediaObserver,
                                      public content::NotificationObserver {
 public:
  class Observer {
   public:
    // Handle an information update consisting of a up-to-date audio capture
    // device lists. This happens when a microphone is plugged in or unplugged.
    virtual void OnUpdateAudioDevices(
        const content::MediaStreamDevices& devices) {}

    // Handle an information update consisting of a up-to-date video capture
    // device lists. This happens when a camera is plugged in or unplugged.
    virtual void OnUpdateVideoDevices(
        const content::MediaStreamDevices& devices) {}

    // Handle an information update related to a media stream request.
    virtual void OnRequestUpdate(
        int render_process_id,
        int render_frame_id,
        content::MediaStreamType stream_type,
        const content::MediaRequestState state) {}

    // Handle an information update that a new stream is being created.
    virtual void OnCreatingAudioStream(int render_process_id,
                                       int render_frame_id) {}

    virtual ~Observer() {}
  };

  static MediaCaptureDevicesDispatcher* GetInstance();

  // Registers the preferences related to Media Stream default devices.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Methods for observers. Called on UI thread.
  // Observers should add themselves on construction and remove themselves
  // on destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  const content::MediaStreamDevices& GetAudioCaptureDevices();
  const content::MediaStreamDevices& GetVideoCaptureDevices();

  // Method called from WebCapturerDelegate implementations to process access
  // requests. |extension| is set to NULL if request was made from a drive-by
  // page.
  void ProcessMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension);

  // Helper to get the default devices which can be used by the media request.
  // Uses the first available devices if the default devices are not available.
  // If the return list is empty, it means there is no available device on the
  // OS.
  // Called on the UI thread.
  void GetDefaultDevicesForProfile(Profile* profile,
                                   bool audio,
                                   bool video,
                                   content::MediaStreamDevices* devices);

  // Helpers for picking particular requested devices, identified by raw id.
  // If the device requested is not available it will return NULL.
  const content::MediaStreamDevice*
  GetRequestedAudioDevice(const std::string& requested_audio_device_id);
  const content::MediaStreamDevice*
  GetRequestedVideoDevice(const std::string& requested_video_device_id);

  // Returns the first available audio or video device, or NULL if no devices
  // are available.
  const content::MediaStreamDevice* GetFirstAvailableAudioDevice();
  const content::MediaStreamDevice* GetFirstAvailableVideoDevice();

  // Unittests that do not require actual device enumeration should call this
  // API on the singleton. It is safe to call this multiple times on the
  // signleton.
  void DisableDeviceEnumerationForTesting();

  // Overridden from content::MediaObserver:
  virtual void OnAudioCaptureDevicesChanged() OVERRIDE;
  virtual void OnVideoCaptureDevicesChanged() OVERRIDE;
  virtual void OnMediaRequestStateChanged(
      int render_process_id,
      int render_frame_id,
      int page_request_id,
      const GURL& security_origin,
      content::MediaStreamType stream_type,
      content::MediaRequestState state) OVERRIDE;
  virtual void OnCreatingAudioStream(int render_process_id,
                                     int render_frame_id) OVERRIDE;
  virtual void OnAudioStreamPlaying(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const ReadPowerAndClipCallback& power_read_callback) OVERRIDE;
  virtual void OnAudioStreamStopped(
      int render_process_id,
      int render_frame_id,
      int stream_id) OVERRIDE;

  scoped_refptr<MediaStreamCaptureIndicator> GetMediaStreamCaptureIndicator();

  DesktopStreamsRegistry* GetDesktopStreamsRegistry();

  bool IsDesktopCaptureInProgress();

  // Only for testing.
  void SetTestAudioCaptureDevices(const content::MediaStreamDevices& devices);
  void SetTestVideoCaptureDevices(const content::MediaStreamDevices& devices);

 private:
  friend struct DefaultSingletonTraits<MediaCaptureDevicesDispatcher>;

  struct PendingAccessRequest {
    PendingAccessRequest(const content::MediaStreamRequest& request,
                         const content::MediaResponseCallback& callback);
    ~PendingAccessRequest();

    // TODO(gbillock): make the MediaStreamDevicesController owned by
    // this object when we're using bubbles.
    content::MediaStreamRequest request;
    content::MediaResponseCallback callback;
  };
  typedef std::deque<PendingAccessRequest> RequestsQueue;
  typedef std::map<content::WebContents*, RequestsQueue> RequestsQueues;

  MediaCaptureDevicesDispatcher();
  virtual ~MediaCaptureDevicesDispatcher();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Helpers for ProcessMediaAccessRequest().
  void ProcessDesktopCaptureAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension);
  void ProcessScreenCaptureAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension);
  void ProcessTabCaptureAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension);
  void ProcessMediaAccessRequestFromPlatformAppOrExtension(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension);
  void ProcessRegularMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback);
  void ProcessQueuedAccessRequest(content::WebContents* web_contents);
  void OnAccessRequestResponse(content::WebContents* web_contents,
                               const content::MediaStreamDevices& devices,
                               content::MediaStreamRequestResult result,
                               scoped_ptr<content::MediaStreamUI> ui);

  // Called by the MediaObserver() functions, executed on UI thread.
  void NotifyAudioDevicesChangedOnUIThread();
  void NotifyVideoDevicesChangedOnUIThread();
  void UpdateMediaRequestStateOnUIThread(
      int render_process_id,
      int render_frame_id,
      int page_request_id,
      const GURL& security_origin,
      content::MediaStreamType stream_type,
      content::MediaRequestState state);
  void OnCreatingAudioStreamOnUIThread(int render_process_id,
                                       int render_frame_id);

  // Only for testing, a list of cached audio capture devices.
  content::MediaStreamDevices test_audio_devices_;

  // Only for testing, a list of cached video capture devices.
  content::MediaStreamDevices test_video_devices_;

  // A list of observers for the device update notifications.
  ObserverList<Observer> observers_;

  // Flag used by unittests to disable device enumeration.
  bool is_device_enumeration_disabled_;

  RequestsQueues pending_requests_;

  scoped_refptr<MediaStreamCaptureIndicator> media_stream_capture_indicator_;

  scoped_ptr<DesktopStreamsRegistry> desktop_streams_registry_;

  content::NotificationRegistrar notifications_registrar_;

  // Tracks MEDIA_DESKTOP_VIDEO_CAPTURE sessions which reach the
  // MEDIA_REQUEST_STATE_DONE state.  Sessions are remove when
  // MEDIA_REQUEST_STATE_CLOSING is encountered.
  struct DesktopCaptureSession {
    int render_process_id;
    int render_frame_id;
    int page_request_id;
  };
  typedef std::list<DesktopCaptureSession> DesktopCaptureSessions;
  DesktopCaptureSessions desktop_capture_sessions_;

  DISALLOW_COPY_AND_ASSIGN(MediaCaptureDevicesDispatcher);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_
