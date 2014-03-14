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
  explicit Core(const base::WeakPtr<MediaStreamUIProxy>& proxy,
                RenderViewHostDelegate* test_render_delegate);
  ~Core();

  void RequestAccess(const MediaStreamRequest& request);
  void OnStarted();

 private:
  void ProcessAccessRequestResponse(const MediaStreamDevices& devices,
                                    content::MediaStreamRequestResult result,
                                    scoped_ptr<MediaStreamUI> stream_ui);
  void ProcessStopRequestFromUI();

  base::WeakPtr<MediaStreamUIProxy> proxy_;
  scoped_ptr<MediaStreamUI> ui_;

  RenderViewHostDelegate* const test_render_delegate_;

  // WeakPtr<> is used to RequestMediaAccessPermission() because there is no way
  // cancel media requests.
  base::WeakPtrFactory<Core> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

MediaStreamUIProxy::Core::Core(const base::WeakPtr<MediaStreamUIProxy>& proxy,
                               RenderViewHostDelegate* test_render_delegate)
    : proxy_(proxy),
      test_render_delegate_(test_render_delegate),
      weak_factory_(this) {
}

MediaStreamUIProxy::Core::~Core() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void MediaStreamUIProxy::Core::RequestAccess(
    const MediaStreamRequest& request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderViewHostDelegate* render_delegate;

  if (test_render_delegate_) {
    render_delegate = test_render_delegate_;
  } else {
    RenderViewHostImpl* host = RenderViewHostImpl::FromID(
        request.render_process_id, request.render_view_id);

    // Tab may have gone away.
    if (!host || !host->GetDelegate()) {
      ProcessAccessRequestResponse(
          MediaStreamDevices(),
          MEDIA_DEVICE_INVALID_STATE,
          scoped_ptr<MediaStreamUI>());
      return;
    }

    render_delegate = host->GetDelegate();
  }

  render_delegate->RequestMediaAccessPermission(
      request, base::Bind(&Core::ProcessAccessRequestResponse,
                          weak_factory_.GetWeakPtr()));
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
    content::MediaStreamRequestResult result,
    scoped_ptr<MediaStreamUI> stream_ui) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ui_ = stream_ui.Pass();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamUIProxy::ProcessAccessRequestResponse,
                 proxy_, devices, result));
}

void MediaStreamUIProxy::Core::ProcessStopRequestFromUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamUIProxy::ProcessStopRequestFromUI, proxy_));
}

// static
scoped_ptr<MediaStreamUIProxy> MediaStreamUIProxy::Create() {
  return scoped_ptr<MediaStreamUIProxy>(new MediaStreamUIProxy(NULL));
}

// static
scoped_ptr<MediaStreamUIProxy> MediaStreamUIProxy::CreateForTests(
    RenderViewHostDelegate* render_delegate) {
  return scoped_ptr<MediaStreamUIProxy>(
      new MediaStreamUIProxy(render_delegate));
}

MediaStreamUIProxy::MediaStreamUIProxy(
    RenderViewHostDelegate* test_render_delegate)
    : weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  core_.reset(new Core(weak_factory_.GetWeakPtr(), test_render_delegate));
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
    const MediaStreamDevices& devices,
    content::MediaStreamRequestResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!response_callback_.is_null());

  ResponseCallback cb = response_callback_;
  response_callback_.Reset();
  cb.Run(devices, result);
}

void MediaStreamUIProxy::ProcessStopRequestFromUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!stop_callback_.is_null());

  base::Closure cb = stop_callback_;
  stop_callback_.Reset();
  cb.Run();
}

FakeMediaStreamUIProxy::FakeMediaStreamUIProxy()
  : MediaStreamUIProxy(NULL) {
}

FakeMediaStreamUIProxy::~FakeMediaStreamUIProxy() {}

void FakeMediaStreamUIProxy::SetAvailableDevices(
    const MediaStreamDevices& devices) {
  devices_ = devices;
}

void FakeMediaStreamUIProxy::RequestAccess(
    const MediaStreamRequest& request,
    const ResponseCallback& response_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  response_callback_ = response_callback;

  MediaStreamDevices devices_to_use;
  bool accepted_audio = false;
  bool accepted_video = false;
  // Use the first capture device of the same media type in the list for the
  // fake UI.
  for (MediaStreamDevices::const_iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if (!accepted_audio &&
        IsAudioMediaType(request.audio_type) &&
        IsAudioMediaType(it->type) &&
        (request.requested_audio_device_id.empty() ||
         request.requested_audio_device_id == it->id)) {
      devices_to_use.push_back(*it);
      accepted_audio = true;
    } else if (!accepted_video &&
               IsVideoMediaType(request.video_type) &&
               IsVideoMediaType(it->type) &&
               (request.requested_video_device_id.empty() ||
                request.requested_video_device_id == it->id)) {
      devices_to_use.push_back(*it);
      accepted_video = true;
    }
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamUIProxy::ProcessAccessRequestResponse,
                 weak_factory_.GetWeakPtr(),
                 devices_to_use,
                 devices_to_use.empty() ?
                     MEDIA_DEVICE_NO_HARDWARE :
                     MEDIA_DEVICE_OK));
}

void FakeMediaStreamUIProxy::OnStarted(const base::Closure& stop_callback) {
}

}  // namespace content
