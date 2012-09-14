// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_device_settings.h"

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
#include "content/public/common/media_stream_request.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;
using content::MediaStreamDevice;
using content::MediaStreamRequest;

namespace {

// Helper class to handle the callbacks to a MediaStreamDeviceSettings instance.
// This class will make sure that the call to PostResponse is executed on the IO
// thread (and that the instance of MediaStreamDeviceSettings still exists).
// This allows us to pass a simple base::Callback object to any class that needs
// to post a response to the MediaStreamDeviceSettings object. This logic cannot
// be implemented inside MediaStreamDeviceSettings::PostResponse since that
// would imply that the WeakPtr<MediaStreamDeviceSettings> pointer has been
// dereferenced already (which would cause an error in the ThreadChecker before
// we even get there).
class ResponseCallbackHelper
    : public base::RefCountedThreadSafe<ResponseCallbackHelper> {
 public:
  explicit ResponseCallbackHelper(
      base::WeakPtr<media_stream::MediaStreamDeviceSettings> settings)
      : settings_(settings) {
  }

  void PostResponse(const std::string& label,
                    const content::MediaStreamDevices& devices) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&media_stream::MediaStreamDeviceSettings::PostResponse,
                     settings_, label, devices));
      return;
    } else if (settings_) {
      settings_->PostResponse(label, devices);
    }
  }

 private:
  friend class base::RefCountedThreadSafe<ResponseCallbackHelper>;
  ~ResponseCallbackHelper() {}

  base::WeakPtr<media_stream::MediaStreamDeviceSettings> settings_;

  DISALLOW_COPY_AND_ASSIGN(ResponseCallbackHelper);
};

// Predicate used in find_if below to find a device with a given ID.
class DeviceIdEquals {
 public:
  explicit DeviceIdEquals(const std::string& device_id)
      : device_id_(device_id) {
  }

  bool operator() (const media_stream::StreamDeviceInfo& device) {
    return (device.device_id == device_id_);
  }

 private:
  std::string device_id_;
};

}  // namespace

namespace media_stream {

typedef std::map<MediaStreamType, StreamDeviceInfoArray> DeviceMap;

// Device request contains all data needed to keep track of requests between the
// different calls.
struct MediaStreamDeviceSettingsRequest : public MediaStreamRequest {
 public:
  MediaStreamDeviceSettingsRequest(
      int render_pid,
      int render_vid,
      const GURL& origin,
      const StreamOptions& request_options)
      : MediaStreamRequest(render_pid, render_vid, origin),
        options(request_options),
        posted_task(false) {}

  ~MediaStreamDeviceSettingsRequest() {}

  // Request options.
  StreamOptions options;
  // Map containing available devices for the requested capture types.
  // Note, never call devices_full[stream_type].empty() before making sure
  // that type of device has existed on the map, otherwise it will create an
  // empty device entry on the map.
  DeviceMap devices_full;
  // Whether or not a task was posted to make the call to
  // RequestMediaAccessPermission, to make sure that we never post twice to it.
  bool posted_task;
};

namespace {

// Sends the request to the appropriate WebContents.
void DoDeviceRequest(const MediaStreamDeviceSettingsRequest& request,
                     const content::MediaResponseCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Send the permission request to the web contents.
  content::RenderViewHostImpl* host =
      content::RenderViewHostImpl::FromID(request.render_process_id,
                                          request.render_view_id);

  // Tab may have gone away.
  if (!host || !host->GetDelegate()) {
    callback.Run(content::MediaStreamDevices());
    return;
  }

  host->GetDelegate()->RequestMediaAccessPermission(&request, callback);
}

bool IsRequestReadyForView(
    media_stream::MediaStreamDeviceSettingsRequest* request) {
  if ((content::IsAudioMediaType(request->options.audio_type) &&
       request->devices_full.count(request->options.audio_type) == 0) ||
      (content::IsVideoMediaType(request->options.video_type) &&
       request->devices_full.count(request->options.video_type) == 0)) {
    return false;
  }

  // We have got all the requested devices, it is ready if it has not
  // been posted for UI yet.
  return !request->posted_task;
}

}  // namespace

MediaStreamDeviceSettings::MediaStreamDeviceSettings(
    SettingsRequester* requester)
    : requester_(requester),
      use_fake_ui_(false),
      weak_ptr_factory_(this) {
  DCHECK(requester_);
}

MediaStreamDeviceSettings::~MediaStreamDeviceSettings() {
  STLDeleteValues(&requests_);
}

void MediaStreamDeviceSettings::RequestCaptureDeviceUsage(
    const std::string& label, int render_process_id, int render_view_id,
    const StreamOptions& request_options, const GURL& security_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Create a new request.
  if (!requests_.insert(std::make_pair(label,
                                       new MediaStreamDeviceSettingsRequest(
                                           render_process_id,
                                           render_view_id,
                                           security_origin,
                                           request_options))).second) {
    NOTREACHED();
  }
}

void MediaStreamDeviceSettings::RemovePendingCaptureRequest(
    const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SettingsRequests::iterator request_it = requests_.find(label);
  if (request_it != requests_.end()) {
    // Proceed the next pending request for the same page.
    MediaStreamDeviceSettingsRequest* request = request_it->second;
    int render_view_id = request->render_view_id;
    int render_process_id = request->render_process_id;
    bool was_posted = request->posted_task;

    // TODO(xians): Post a cancel request on UI thread to dismiss the infobar
    // if request has been sent to the UI.
    // Remove the request from the queue.
    requests_.erase(request_it);
    delete request;

    // Simply return if the canceled request has not been brought to UI.
    if (!was_posted)
      return;

    // Process the next pending request to replace the old infobar on the same
    // page.
    ProcessNextRequestForView(render_view_id, render_process_id);
  }
}

void MediaStreamDeviceSettings::AvailableDevices(
    const std::string& label,
    MediaStreamType stream_type,
    const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SettingsRequests::iterator request_it = requests_.find(label);
  if (request_it == requests_.end()) {
    NOTREACHED();
    return;
  }

  // Add the answer for the request.
  MediaStreamDeviceSettingsRequest* request = request_it->second;
  DCHECK_EQ(request->devices_full.count(stream_type), static_cast<size_t>(0))
      << "This request already has a list of devices for this stream type.";
  request->devices_full[stream_type] = devices;

  if (IsRequestReadyForView(request)) {
    if (use_fake_ui_) {
      PostRequestToFakeUI(label);
      return;
    }

    if (IsUIBusy(request->render_view_id, request->render_process_id)) {
      // The UI can handle only one request at the time, do not post the
      // request to the view if the UI is handling any other request.
      return;
    }

    PostRequestToUI(label);
  }
}

void MediaStreamDeviceSettings::PostResponse(
    const std::string& label,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SettingsRequests::iterator req = requests_.find(label);
  // Return if the request has been removed.
  if (req == requests_.end())
    return;

  DCHECK(requester_);
  scoped_ptr<MediaStreamDeviceSettingsRequest> request(req->second);
  requests_.erase(req);

  // Look for queued requests for the same view. If there is a pending request,
  // post it for user approval.
  ProcessNextRequestForView(request->render_view_id,
                            request->render_process_id);

  if (devices.size() > 0) {
    // Build a list of "full" device objects for the accepted devices.
    StreamDeviceInfoArray deviceList;
    for (content::MediaStreamDevices::const_iterator dev = devices.begin();
         dev != devices.end(); ++dev) {
      DeviceMap::iterator subList = request->devices_full.find(dev->type);
      DCHECK(subList != request->devices_full.end());

      deviceList.push_back(*std::find_if(subList->second.begin(),
          subList->second.end(), DeviceIdEquals(dev->device_id)));
    }
    requester_->DevicesAccepted(label, deviceList);
  } else {
    requester_->SettingsError(label);
  }
}

void MediaStreamDeviceSettings::UseFakeUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  use_fake_ui_ = true;
}

bool MediaStreamDeviceSettings::IsUIBusy(int render_view_id,
                                         int render_process_id) {
  for (SettingsRequests::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    if (it->second->render_process_id == render_process_id &&
        it->second->render_view_id == render_view_id &&
        it->second->posted_task) {
      return true;
    }
  }
  return false;
}

void MediaStreamDeviceSettings::ProcessNextRequestForView(
    int render_view_id, int render_process_id) {
  std::string new_label;
  for (SettingsRequests::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    if (it->second->render_process_id == render_process_id &&
        it->second->render_view_id == render_view_id) {
      // This request belongs to the given render view.
      MediaStreamDeviceSettingsRequest* request = it->second;
      if (IsRequestReadyForView(request)) {
        new_label = it->first;
        break;
      }
    }
  }

  if (new_label.empty())
    return;

  if (use_fake_ui_)
    PostRequestToFakeUI(new_label);
  else
    PostRequestToUI(new_label);
}

void MediaStreamDeviceSettings::PostRequestToUI(const std::string& label) {
  SettingsRequests::iterator request_it = requests_.find(label);
  if (request_it == requests_.end()) {
    NOTREACHED();
    return;
  }
  MediaStreamDeviceSettingsRequest* request = request_it->second;
  DCHECK(request != NULL);

  request->posted_task = true;

  // Create the simplified list of devices.
  for (DeviceMap::iterator it = request->devices_full.begin();
       it != request->devices_full.end(); ++it) {
    request->devices[it->first].clear();
    for (StreamDeviceInfoArray::iterator device = it->second.begin();
        device != it->second.end(); ++device) {
      request->devices[it->first].push_back(MediaStreamDevice(
          it->first, device->device_id, device->name));
    }
  }

  scoped_refptr<ResponseCallbackHelper> helper =
      new ResponseCallbackHelper(weak_ptr_factory_.GetWeakPtr());
  content::MediaResponseCallback callback =
      base::Bind(&ResponseCallbackHelper::PostResponse,
                 helper.get(), label);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DoDeviceRequest, *request, callback));
}

void MediaStreamDeviceSettings::PostRequestToFakeUI(const std::string& label) {
  SettingsRequests::iterator request_it = requests_.find(label);
  DCHECK(request_it != requests_.end());
  MediaStreamDeviceSettingsRequest* request = request_it->second;
  // Used to fake UI, which is needed for server based testing.
  // Choose first non-opened device for each media type.
  content::MediaStreamDevices devices_to_use;
  for (DeviceMap::iterator it = request->devices_full.begin();
       it != request->devices_full.end(); ++it) {
    StreamDeviceInfoArray::iterator device_it = it->second.begin();
    for (; device_it != it->second.end(); ++device_it) {
      if (!device_it->in_use) {
        devices_to_use.push_back(content::MediaStreamDevice(
            device_it->stream_type, device_it->device_id, device_it->name));
        break;
      }
    }

    if (it->second.size() != 0 && device_it == it->second.end()) {
      // Use the first capture device in the list if all the devices are
      // being used.
      devices_to_use.push_back(
          content::MediaStreamDevice(it->second.begin()->stream_type,
                                     it->second.begin()->device_id,
                                     it->second.begin()->name));
    }
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamDeviceSettings::PostResponse,
                 weak_ptr_factory_.GetWeakPtr(), label, devices_to_use));
}

}  // namespace media_stream
