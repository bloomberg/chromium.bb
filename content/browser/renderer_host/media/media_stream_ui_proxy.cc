// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"

#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "media/video/capture/fake_video_capture_device.h"

namespace content {

class MediaStreamUIProxy::Core {
 public:
  explicit Core(const base::WeakPtr<MediaStreamUIProxy>& proxy);
  ~Core();

  void RequestAccess(const MediaStreamRequest& request);
  void OnStarted();

 private:
  void ProcessAccessRequestResponse(const MediaStreamDevices& devices,
                                    scoped_ptr<MediaStreamUI> stream_ui);
  void ProcessStopRequestFromUI();

  base::WeakPtr<MediaStreamUIProxy> proxy_;
  scoped_ptr<MediaStreamUI> ui_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

MediaStreamUIProxy::Core::Core(const base::WeakPtr<MediaStreamUIProxy>& proxy)
    : proxy_(proxy) {
}

MediaStreamUIProxy::Core::~Core() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void MediaStreamUIProxy::Core::RequestAccess(
    const MediaStreamRequest& request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderViewHostImpl* host = RenderViewHostImpl::FromID(
      request.render_process_id, request.render_view_id);

  // Tab may have gone away.
  if (!host || !host->GetDelegate()) {
    ProcessAccessRequestResponse(
        MediaStreamDevices(), scoped_ptr<MediaStreamUI>());
    return;
  }

  host->GetDelegate()->RequestMediaAccessPermission(
      request, base::Bind(&Core::ProcessAccessRequestResponse,
                          base::Unretained(this)));
}

void MediaStreamUIProxy::Core::OnStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (ui_) {
    ui_->OnStarted(base::Bind(&Core::ProcessStopRequestFromUI,
                              base::Unretained(this)));
  }
}

void MediaStreamUIProxy::Core::ProcessAccessRequestResponse(
    const MediaStreamDevices& devices,
    scoped_ptr<MediaStreamUI> stream_ui) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ui_ = stream_ui.Pass();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamUIProxy::ProcessAccessRequestResponse,
                 proxy_, devices));
}

void MediaStreamUIProxy::Core::ProcessStopRequestFromUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamUIProxy::ProcessStopRequestFromUI, proxy_));
}

MediaStreamUIProxy::MediaStreamUIProxy()
    : weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  core_.reset(new Core(weak_factory_.GetWeakPtr()));
}

MediaStreamUIProxy::~MediaStreamUIProxy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, core_.release());
}

void MediaStreamUIProxy::RequestAccess(
    const MediaStreamRequest& request,
    const ResponseCallback& response_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  response_callback_ = response_callback;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::RequestAccess, base::Unretained(core_.get()), request));
}

void MediaStreamUIProxy::OnStarted(const base::Closure& stop_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  stop_callback_ = stop_callback;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::OnStarted, base::Unretained(core_.get())));
}

void MediaStreamUIProxy::ProcessAccessRequestResponse(
    const MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!response_callback_.is_null());

  ResponseCallback cb = response_callback_;
  response_callback_.Reset();
  cb.Run(devices);
}

void MediaStreamUIProxy::ProcessStopRequestFromUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!stop_callback_.is_null());

  base::Closure cb = stop_callback_;
  stop_callback_.Reset();
  cb.Run();
}

FakeMediaStreamUIProxy::FakeMediaStreamUIProxy() {}
FakeMediaStreamUIProxy::~FakeMediaStreamUIProxy() {}

void FakeMediaStreamUIProxy::RequestAccess(
    const MediaStreamRequest& request,
    const ResponseCallback& response_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  MediaStreamDevices devices;

  if (IsAudioMediaType(request.audio_type)) {
    devices.push_back(MediaStreamDevice(request.audio_type, "Mic", "Mic"));
  }

  if (IsVideoMediaType(request.video_type)) {
    media::VideoCaptureDevice::Names device_names;
    media::FakeVideoCaptureDevice::GetDeviceNames(&device_names);
    if (!device_names.empty()) {
      devices.push_back(MediaStreamDevice(
          request.video_type,
          device_names.front().unique_id,
          device_names.front().device_name));
    }
  }

  response_callback_ = response_callback;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamUIProxy::ProcessAccessRequestResponse,
                 weak_factory_.GetWeakPtr(), devices));
}

void FakeMediaStreamUIProxy::OnStarted(const base::Closure& stop_callback) {
}

}  // namespace content
