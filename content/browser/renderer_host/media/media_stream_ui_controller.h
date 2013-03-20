// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaStreamUIController is used to decide which of the available capture
// device to use as well as getting user permission to use the capture device.
// There will be one instance of MediaStreamDeviceSettings handling all
// requests.

// Expected call flow:
// 1. MakeUIRequest() is called to create a new request to the UI for capture
//    device access.
// 2. Pick device and get user confirmation.
// 3. Confirm by calling SettingsRequester::DevicesAccepted().
// Repeat step 1 - 3 for new device requests.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_UI_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_UI_CONTROLLER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

class MediaStreamRequestForUI;
class SettingsRequester;

// MediaStreamUIController is responsible for getting user permission to use
// a media capture device as well as selecting what device to use.
class CONTENT_EXPORT MediaStreamUIController {
 public:
  explicit MediaStreamUIController(SettingsRequester* requester);
  virtual ~MediaStreamUIController();

  // Called when a new request for the capture device access is made.
  // Users are responsible for canceling the pending request if they don't wait
  // for the result from the UI.
  void MakeUIRequest(const std::string& label,
                     int render_process_id,
                     int render_view_id,
                     const StreamOptions& stream_components,
                     const GURL& security_origin,
                     MediaStreamRequestType request_type,
                     const std::string& requested_device_id);

  // Called to cancel a pending UI request of capture device access when the
  // user has no action for the media stream InfoBar.
  void CancelUIRequest(const std::string& label);

  // Called by the InfoBar when the user grants/denies access to some devices
  // to the webpage. This is placed here, so the request can be cleared from the
  // list of pending requests, instead of letting the InfoBar itself respond to
  // the requester. An empty list of devices means that access has been denied.
  // This method must be called on the IO thread.
  void PostResponse(const std::string& label,
                    const MediaStreamDevices& devices);

  // Called to signal the UI indicator that the devices are opened.
  void NotifyUIIndicatorDevicesOpened(
      const std::string& label,
      int render_process_id,
      int render_view_id,
      const MediaStreamDevices& devices);

  // Called to signal the UI indicator that the devices are closed.
  void NotifyUIIndicatorDevicesClosed(
      int render_process_id,
      int render_view_id,
      const MediaStreamDevices& devices);

  // Used for testing only. This function is called to use faked UI, which is
  // needed for server based tests. The first non-opened device(s) will be
  // picked.
  void UseFakeUI();

 private:
  typedef std::map<std::string, MediaStreamRequestForUI*> UIRequests;

  // Returns true if the UI is already processing a request for this render
  // view.
  bool IsUIBusy(int render_process_id, int render_view_id);

  // Process the next pending request and bring it up to the UI on the given
  // page for user approval.
  void ProcessNextRequestForView(int render_process_id, int render_view_id);

  // Posts a request to be approved/denied by UI.
  void PostRequestToUI(const std::string& label);

  // Posts a request to fake UI which is used for testing purpose.
  void PostRequestToFakeUI(const std::string& label);

  // Callback for UI called when user requests a stream to be stopped.
  void OnStopStreamFromUI(const std::string& label);

  SettingsRequester* requester_;
  UIRequests requests_;

  // See comment above for method UseFakeUI. Used for automated testing.
  bool use_fake_ui_;

  base::WeakPtrFactory<MediaStreamUIController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamUIController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_UI_CONTROLLER_H_
