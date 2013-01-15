// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_controller.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/media/media_stream_settings_requester.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_observer.h"
#include "content/public/common/media_stream_request.h"
#include "googleurl/src/gurl.h"

namespace content {
namespace {

// Helper class to handle the callbacks to a MediaStreamUIController instance.
// This class will make sure that the call to PostResponse is executed on the IO
// thread (and that the instance of MediaStreamUIController still exists).
// This allows us to pass a simple base::Callback object to any class that needs
// to post a response to the MediaStreamUIController object. This logic cannot
// be implemented inside MediaStreamUIController::PostResponse since that
// would imply that the WeakPtr<MediaStreamUIController> pointer has been
// dereferenced already (which would cause an error in the ThreadChecker before
// we even get there).
class ResponseCallbackHelper
    : public base::RefCountedThreadSafe<ResponseCallbackHelper> {
 public:
  explicit ResponseCallbackHelper(
      base::WeakPtr<MediaStreamUIController> controller)
      : controller_(controller) {
  }

  void PostResponse(const std::string& label,
                    const MediaStreamDevices& devices) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&MediaStreamUIController::PostResponse,
                     controller_, label, devices));
      return;
    } else if (controller_) {
      controller_->PostResponse(label, devices);
    }
  }

 private:
  friend class base::RefCountedThreadSafe<ResponseCallbackHelper>;
  ~ResponseCallbackHelper() {}

  base::WeakPtr<MediaStreamUIController> controller_;

  DISALLOW_COPY_AND_ASSIGN(ResponseCallbackHelper);
};

}  // namespace

// UI request contains all data needed to keep track of requests between the
// different calls.
class MediaStreamRequestForUI : public MediaStreamRequest {
 public:
  MediaStreamRequestForUI(int render_pid,
                          int render_vid,
                          const GURL& origin,
                          const StreamOptions& options,
                          MediaStreamRequestType request_type,
                          const std::string& requested_device_id)
      : MediaStreamRequest(render_pid, render_vid, origin,
                           request_type, requested_device_id,
                           options.audio_type, options.video_type),
        posted_task(false) {
    DCHECK(IsAudioMediaType(options.audio_type) ||
           IsVideoMediaType(options.video_type));
  }

  ~MediaStreamRequestForUI() {}

  // Whether or not a task was posted to make the call to
  // RequestMediaAccessPermission, to make sure that we never post twice to it.
  bool posted_task;
};

namespace {

// Sends the request to the appropriate WebContents.
void ProceedMediaAccessPermission(const MediaStreamRequestForUI& request,
                                  const MediaResponseCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Send the permission request to the web contents.
  RenderViewHostImpl* host = RenderViewHostImpl::FromID(
      request.render_process_id, request.render_view_id);

  // Tab may have gone away.
  if (!host || !host->GetDelegate()) {
    callback.Run(MediaStreamDevices());
    return;
  }

  host->GetDelegate()->RequestMediaAccessPermission(request, callback);
}

}  // namespace

MediaStreamUIController::MediaStreamUIController(SettingsRequester* requester)
    : requester_(requester),
      use_fake_ui_(false),
      weak_ptr_factory_(this) {
  DCHECK(requester_);
}

MediaStreamUIController::~MediaStreamUIController() {
  DCHECK(requests_.empty());
}

void MediaStreamUIController::MakeUIRequest(
    const std::string& label,
    int render_process_id,
    int render_view_id,
    const StreamOptions& request_options,
    const GURL& security_origin, MediaStreamRequestType request_type,
    const std::string& requested_device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Create a new request.
  if (!requests_.insert(
          std::make_pair(label, new MediaStreamRequestForUI(
              render_process_id, render_view_id, security_origin,
              request_options, request_type, requested_device_id))).second) {
    NOTREACHED();
  }

  if (use_fake_ui_) {
    PostRequestToFakeUI(label);
    return;
  }

  // The UI can handle only one request at the time, do not post the
  // request to the view if the UI is handling any other request.
  if (IsUIBusy(render_process_id, render_view_id))
    return;

  PostRequestToUI(label);
}

void MediaStreamUIController::CancelUIRequest(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UIRequests::iterator request_iter = requests_.find(label);
  if (request_iter != requests_.end()) {
    // Proceed the next pending request for the same page.
    scoped_ptr<MediaStreamRequestForUI> request(request_iter->second);
    int render_view_id = request->render_view_id;
    int render_process_id = request->render_process_id;
    bool was_posted = request->posted_task;

    // TODO(xians): Post a cancel request on UI thread to dismiss the infobar
    // if request has been sent to the UI.
    // Remove the request from the queue.
    requests_.erase(request_iter);

    // Simply return if the canceled request has not been brought to UI.
    if (!was_posted)
      return;

    // Process the next pending request to replace the old infobar on the same
    // page.
    ProcessNextRequestForView(render_process_id, render_view_id);
  }
}

void MediaStreamUIController::PostResponse(
    const std::string& label,
    const MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UIRequests::iterator request_iter = requests_.find(label);
  // Return if the request has been removed.
  if (request_iter == requests_.end())
    return;

  DCHECK(requester_);
  scoped_ptr<MediaStreamRequestForUI> request(request_iter->second);
  requests_.erase(request_iter);

  // Look for queued requests for the same view. If there is a pending request,
  // post it for user approval.
  ProcessNextRequestForView(request->render_process_id,
                            request->render_view_id);

  if (devices.size() > 0) {
    // Build a list of "full" device objects for the accepted devices.
    StreamDeviceInfoArray device_list;
    // TODO(xians): figure out if it is all right to hard code in_use to false,
    // though DevicesAccepted seems to do so.
    for (MediaStreamDevices::const_iterator dev = devices.begin();
         dev != devices.end(); ++dev) {
      device_list.push_back(StreamDeviceInfo(
          dev->type, dev->name, dev->id, false));
    }

    requester_->DevicesAccepted(label, device_list);
  } else {
    requester_->SettingsError(label);
  }
}

void MediaStreamUIController::NotifyUIIndicatorDevicesOpened(
    int render_process_id,
    int render_view_id,
    const MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!devices.empty());
  MediaObserver* media_observer =
      GetContentClient()->browser()->GetMediaObserver();
  if (media_observer == NULL)
    return;

  media_observer->OnCaptureDevicesOpened(render_process_id,
                                         render_view_id,
                                         devices);
}

void MediaStreamUIController::NotifyUIIndicatorDevicesClosed(
    int render_process_id,
    int render_view_id,
    const MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!devices.empty());
  MediaObserver* media_observer =
      GetContentClient()->browser()->GetMediaObserver();
  if (media_observer == NULL)
    return;

  media_observer->OnCaptureDevicesClosed(render_process_id,
                                         render_view_id,
                                         devices);
}

void MediaStreamUIController::UseFakeUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  use_fake_ui_ = true;
}

bool MediaStreamUIController::IsUIBusy(int render_process_id,
                                       int render_view_id) {
  for (UIRequests::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    if (it->second->render_process_id == render_process_id &&
        it->second->render_view_id == render_view_id &&
        it->second->posted_task) {
      return true;
    }
  }
  return false;
}

void MediaStreamUIController::ProcessNextRequestForView(
    int render_process_id,
    int render_view_id) {
  std::string next_request_label;
  for (UIRequests::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    if (it->second->render_process_id == render_process_id &&
        it->second->render_view_id == render_view_id) {
      // This request belongs to the given render view.
      if (!it->second->posted_task) {
        next_request_label = it->first;
        break;
      }
    }
  }

  if (next_request_label.empty())
    return;

  if (use_fake_ui_) {
    PostRequestToFakeUI(next_request_label);
  } else {
    PostRequestToUI(next_request_label);
  }
}

void MediaStreamUIController::PostRequestToUI(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UIRequests::iterator request_iter = requests_.find(label);
  if (request_iter == requests_.end()) {
    NOTREACHED();
    return;
  }
  MediaStreamRequestForUI* request = request_iter->second;
  DCHECK(request != NULL);

  request->posted_task = true;

  scoped_refptr<ResponseCallbackHelper> helper =
      new ResponseCallbackHelper(weak_ptr_factory_.GetWeakPtr());
  MediaResponseCallback callback =
      base::Bind(&ResponseCallbackHelper::PostResponse,
                 helper.get(), label);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ProceedMediaAccessPermission, *request, callback));
}

void MediaStreamUIController::PostRequestToFakeUI(const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(requester_);
  UIRequests::iterator request_iter = requests_.find(label);
  DCHECK(request_iter != requests_.end());
  MediaStreamRequestForUI* request = request_iter->second;

  MediaStreamDevices devices;
  requester_->GetAvailableDevices(&devices);
  MediaStreamDevices devices_to_use;
  bool accepted_audio = false;
  bool accepted_video = false;
  // Use the first capture device of the same media type in the list for the
  // fake UI.
  for (MediaStreamDevices::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if (!accepted_audio &&
        IsAudioMediaType(request->audio_type) &&
        IsAudioMediaType(it->type)) {
      devices_to_use.push_back(*it);
      accepted_audio = true;
    } else if (!accepted_video &&
               IsVideoMediaType(request->video_type) &&
               IsVideoMediaType(it->type)) {
      devices_to_use.push_back(*it);
      accepted_video = true;
    }
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamUIController::PostResponse,
                 weak_ptr_factory_.GetWeakPtr(), label, devices_to_use));
}

}  // namespace content
