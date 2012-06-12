// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_device_settings.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
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
class MediaStreamDeviceSettingsRequest : public MediaStreamRequest {
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
  DeviceMap devices_full;
  // Whether or not a task was posted to make the call to
  // RequestMediaAccessPermission, to make sure that we never post twice to it.
  bool posted_task;
};

namespace {

// Sends the request to the appropriate WebContents.
void DoDeviceRequest(
    const MediaStreamDeviceSettingsRequest* request,
    const content::MediaResponseCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Send the permission request to the web contents.
  content::RenderViewHostImpl* host =
      content::RenderViewHostImpl::FromID(request->render_process_id,
                                          request->render_view_id);

  // Tab may have gone away.
  if (!host || !host->GetDelegate())
    callback.Run(content::MediaStreamDevices());

  host->GetDelegate()->RequestMediaAccessPermission(request, callback);
}

}  // namespace

MediaStreamDeviceSettings::MediaStreamDeviceSettings(
    SettingsRequester* requester)
    : requester_(requester),
      use_fake_ui_(false) {
  DCHECK(requester_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

MediaStreamDeviceSettings::~MediaStreamDeviceSettings() {
  STLDeleteValues(&requests_);
}

void MediaStreamDeviceSettings::RequestCaptureDeviceUsage(
    const std::string& label, int render_process_id, int render_view_id,
    const StreamOptions& request_options, const GURL& security_origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (requests_.find(label) != requests_.end()) {
    // Request with this id already exists.
    requester_->SettingsError(label);
    return;
  }

  // Create a new request.
  requests_.insert(std::make_pair(label, new MediaStreamDeviceSettingsRequest(
      render_process_id, render_view_id, security_origin, request_options)));
}

void MediaStreamDeviceSettings::RemovePendingCaptureRequest(
    const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SettingsRequests::iterator request_it = requests_.find(label);
  if (request_it != requests_.end()) {
    MediaStreamDeviceSettingsRequest* request = request_it->second;
    requests_.erase(request_it);
    delete request;
  }
}

void MediaStreamDeviceSettings::AvailableDevices(
    const std::string& label,
    MediaStreamType stream_type,
    const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SettingsRequests::iterator request_it = requests_.find(label);
  DCHECK(request_it != requests_.end());

  // Add the answer for the request.
  MediaStreamDeviceSettingsRequest* request = request_it->second;
  DCHECK_EQ(request->devices_full.count(stream_type), static_cast<size_t>(0)) <<
      "This request already has a list of devices for this stream type.";
  request->devices_full[stream_type] = devices;

  // Check if we're done.
  size_t num_media_requests = 0;
  if (request->options.audio) {
    num_media_requests++;
  }
  if (request->options.video) {
    num_media_requests++;
  }

  if (request->devices_full.size() == num_media_requests) {
    // We have all answers needed.
    if (!use_fake_ui_) {
      // Abort if the task was already posted: wait for it to PostResponse.
      if (request->posted_task) {
        return;
      }
      // Since the UI can only handle one request at the time, verify there
      // is no unanswered request posted for this view. If there is, this
      // new request will be handled once we get a response for the first one.
      if (IsUiBusy(request->render_view_id, request->render_process_id)) {
        return;
      }
      PostRequestToUi(label);
    } else {
      // Used to fake UI, which is needed for server based testing.
      // Choose first non-opened device for each media type.
      StreamDeviceInfoArray devices_to_use;
      for (DeviceMap::iterator it = request->devices_full.begin();
           it != request->devices_full.end(); ++it) {
        for (StreamDeviceInfoArray::iterator device_it = it->second.begin();
             device_it != it->second.end(); ++device_it) {
          if (!device_it->in_use) {
            devices_to_use.push_back(*device_it);
            break;
          }
        }
      }

      if (!request->devices_full[
              content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE].empty() &&
          num_media_requests != devices_to_use.size()) {
        // Not all requested device types were opened. This happens if all
        // video capture devices are already opened, |in_use| isn't set for
        // audio devices. Allow the first video capture device in the list to be
        // opened for this user too.
        StreamDeviceInfoArray device_array =
            request->devices_full[
                content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE];
        devices_to_use.push_back(*(device_array.begin()));
      }

      // Post result and delete request.
      requester_->DevicesAccepted(label, devices_to_use);
      requests_.erase(request_it);
      delete request;
    }
  }
}

void MediaStreamDeviceSettings::PostResponse(
    const std::string& label,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SettingsRequests::iterator req = requests_.find(label);
  DCHECK(req != requests_.end()) << "Invalid request label.";
  DCHECK(requester_);
  MediaStreamDeviceSettingsRequest* request = req->second;
  requests_.erase(req);

  // Look for queued requests for the same view. If there is a pending request,
  // post it for user approval.
  std::string new_label = FindReadyRequestForView(request->render_view_id,
                                                  request->render_process_id);
  if (!new_label.empty()) {
    PostRequestToUi(new_label);
  }

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
  delete request;
}

void MediaStreamDeviceSettings::UseFakeUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  use_fake_ui_ = true;
}

bool MediaStreamDeviceSettings::IsUiBusy(int render_view_id,
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

std::string MediaStreamDeviceSettings::FindReadyRequestForView(
    int render_view_id, int render_process_id) {
  for (SettingsRequests::iterator it = requests_.begin(); it != requests_.end();
       ++it) {
    if (it->second->render_process_id == render_process_id &&
        it->second->render_view_id == render_view_id) {
      // This request belongs to the given render view.
      MediaStreamDeviceSettingsRequest* request = it->second;
      if (request->options.audio &&
          request->devices_full[
              content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE].empty()) {
        // Audio requested, but no devices enumerated yet. Continue to next
        // request.
        continue;
      }
      if (request->options.video &&
          request->devices_full[
              content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE].empty()) {
        // Video requested, but no devices enumerated yet. Continue to next
        // request.
        continue;
      }
      // This request belongs to the same view as the treated request and is
      // ready to be requested. Return its label.
      return it->first;
    }
  }
  return std::string();
}

void MediaStreamDeviceSettings::PostRequestToUi(const std::string& label) {
  MediaStreamDeviceSettingsRequest* request = requests_[label];
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
      new ResponseCallbackHelper(AsWeakPtr());
  content::MediaResponseCallback callback =
      base::Bind(&ResponseCallbackHelper::PostResponse,
                 helper.get(), label);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DoDeviceRequest, request, callback));
}

}  // namespace media_stream
