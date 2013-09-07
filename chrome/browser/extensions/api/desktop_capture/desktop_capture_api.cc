// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/desktop_streams_registry.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"

namespace extensions {

namespace {

const char kNotImplementedError[] =
    "Desktop Capture API is not yet implemented for this platform.";
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
DesktopCaptureChooseDesktopMediaFunction() {
}

DesktopCaptureChooseDesktopMediaFunction::
~DesktopCaptureChooseDesktopMediaFunction() {
}

bool DesktopCaptureChooseDesktopMediaFunction::RunImpl() {
  scoped_ptr<api::desktop_capture::ChooseDesktopMedia::Params> params =
      api::desktop_capture::ChooseDesktopMedia::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_ptr<webrtc::ScreenCapturer> screen_capturer;
  scoped_ptr<webrtc::WindowCapturer> window_capturer;

  for (std::vector<api::desktop_capture::DesktopCaptureSourceType>::iterator
       it = params->sources.begin(); it != params->sources.end(); ++it) {
    switch (*it) {
      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_NONE:
        error_ = kInvalidSourceNameError;
        return false;

      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_SCREEN:
        screen_capturer.reset(webrtc::ScreenCapturer::Create());
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
    // DesktopMediaPicker is not implented for all platforms yet.
#if defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)
    model.reset(new DesktopMediaPickerModelImpl(
        screen_capturer.Pass(), window_capturer.Pass()));
    picker_ = DesktopMediaPicker::Create();
#else
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

}  // namespace extensions
