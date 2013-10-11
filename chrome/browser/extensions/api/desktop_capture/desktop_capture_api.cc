// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/desktop_streams_registry.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"

namespace extensions {

namespace {

const char kInvalidSourceNameError[] = "Invalid source type specified.";
const char kEmptySourcesListError[] =
    "At least one source type must be specified.";
const char kTabCaptureNotSupportedError[] = "Tab capture is not supported yet.";

DesktopCaptureChooseDesktopMediaFunction::PickerFactory* g_picker_factory =
    NULL;

}  // namespace

// static
void DesktopCaptureChooseDesktopMediaFunction::SetPickerFactoryForTests(
    PickerFactory* factory) {
  g_picker_factory = factory;
}

DesktopCaptureChooseDesktopMediaFunction::
    DesktopCaptureChooseDesktopMediaFunction() {}

DesktopCaptureChooseDesktopMediaFunction::
    ~DesktopCaptureChooseDesktopMediaFunction() {
  // RenderViewHost may be already destroyed.
  if (render_view_host()) {
    DesktopCaptureRequestsRegistry::GetInstance()->RemoveRequest(
        render_view_host()->GetProcess()->GetID(), request_id_);
  }
}

void DesktopCaptureChooseDesktopMediaFunction::Cancel() {
  // Keep reference to |this| to ensure the object doesn't get destroyed before
  // we return.
  scoped_refptr<DesktopCaptureChooseDesktopMediaFunction> self(this);
  if (picker_) {
    picker_.reset();
    SetResult(new base::StringValue(std::string()));
    SendResponse(true);
  }
}

bool DesktopCaptureChooseDesktopMediaFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetSize() > 0);

  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &request_id_));
  args_->Remove(0, NULL);

  scoped_ptr<api::desktop_capture::ChooseDesktopMedia::Params> params =
      api::desktop_capture::ChooseDesktopMedia::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DesktopCaptureRequestsRegistry::GetInstance()->AddRequest(
      render_view_host()->GetProcess()->GetID(), request_id_, this);

  scoped_ptr<webrtc::ScreenCapturer> screen_capturer;
  scoped_ptr<webrtc::WindowCapturer> window_capturer;

  for (std::vector<api::desktop_capture::DesktopCaptureSourceType>::iterator
       it = params->sources.begin(); it != params->sources.end(); ++it) {
    switch (*it) {
      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_NONE:
        error_ = kInvalidSourceNameError;
        return false;

      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_SCREEN:
#if defined(OS_WIN)
        // ScreenCapturerWin disables Aero by default.
        screen_capturer.reset(
            webrtc::ScreenCapturer::CreateWithDisableAero(false));
#else
        screen_capturer.reset(webrtc::ScreenCapturer::Create());
#endif
        break;

      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_WINDOW:
        window_capturer.reset(webrtc::WindowCapturer::Create());
        break;

      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_TAB:
        error_ = kTabCaptureNotSupportedError;
        return false;
    }
  }

  if (!screen_capturer && !window_capturer) {
    error_ = kEmptySourcesListError;
    return false;
  }

  if (params->origin) {
    origin_ = GURL(*(params->origin));
  } else {
    origin_ = GetExtension()->url();
  }

  scoped_ptr<DesktopMediaPickerModel> model;
  if (g_picker_factory) {
    model = g_picker_factory->CreateModel(
        screen_capturer.Pass(), window_capturer.Pass());
    picker_ = g_picker_factory->CreatePicker();
  } else {
    // DesktopMediaPicker is implemented only for Windows, OSX and
    // Aura Linux builds.
#if (defined(TOOLKIT_VIEWS) && !defined(OS_CHROMEOS)) || defined(OS_MACOSX)
    model.reset(new DesktopMediaPickerModelImpl(
        screen_capturer.Pass(), window_capturer.Pass()));
    picker_ = DesktopMediaPicker::Create();
#else
    const char kNotImplementedError[] =
        "Desktop Capture API is not yet implemented for this platform.";
    error_ = kNotImplementedError;
    return false;
#endif
  }
  DesktopMediaPicker::DoneCallback callback = base::Bind(
      &DesktopCaptureChooseDesktopMediaFunction::OnPickerDialogResults, this);
  picker_->Show(NULL, NULL, UTF8ToUTF16(GetExtension()->name()),
                model.Pass(), callback);
  return true;
}

void DesktopCaptureChooseDesktopMediaFunction::OnPickerDialogResults(
    content::DesktopMediaID source) {
  std::string result;
  if (source.type != content::DesktopMediaID::TYPE_NONE) {
    DesktopStreamsRegistry* registry =
        MediaCaptureDevicesDispatcher::GetInstance()->
        GetDesktopStreamsRegistry();
    result = registry->RegisterStream(origin_, source);
  }

  SetResult(new base::StringValue(result));
  SendResponse(true);
}

DesktopCaptureRequestsRegistry::RequestId::RequestId(int process_id,
                                                     int request_id)
    : process_id(process_id),
      request_id(request_id) {
}

bool DesktopCaptureRequestsRegistry::RequestId::operator<(
    const RequestId& other) const {
  if (process_id != other.process_id) {
    return process_id < other.process_id;
  } else {
    return request_id < other.request_id;
  }
}

DesktopCaptureCancelChooseDesktopMediaFunction::
    DesktopCaptureCancelChooseDesktopMediaFunction() {}

DesktopCaptureCancelChooseDesktopMediaFunction::
    ~DesktopCaptureCancelChooseDesktopMediaFunction() {}

bool DesktopCaptureCancelChooseDesktopMediaFunction::RunImpl() {
  int request_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &request_id));

  DesktopCaptureRequestsRegistry::GetInstance()->CancelRequest(
      render_view_host()->GetProcess()->GetID(), request_id);
  return true;
}

DesktopCaptureRequestsRegistry::DesktopCaptureRequestsRegistry() {}
DesktopCaptureRequestsRegistry::~DesktopCaptureRequestsRegistry() {}

// static
DesktopCaptureRequestsRegistry* DesktopCaptureRequestsRegistry::GetInstance() {
  return Singleton<DesktopCaptureRequestsRegistry>::get();
}

void DesktopCaptureRequestsRegistry::AddRequest(
    int process_id,
    int request_id,
    DesktopCaptureChooseDesktopMediaFunction* handler) {
  requests_.insert(
      RequestsMap::value_type(RequestId(process_id, request_id), handler));
}

void DesktopCaptureRequestsRegistry::RemoveRequest(int process_id,
                                                   int request_id) {
  requests_.erase(RequestId(process_id, request_id));
}

void DesktopCaptureRequestsRegistry::CancelRequest(int process_id,
                                                   int request_id) {
  RequestsMap::iterator it = requests_.find(RequestId(process_id, request_id));
  if (it != requests_.end())
    it->second->Cancel();
}


}  // namespace extensions
